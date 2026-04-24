
#include "gemini_client.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>
#include <format>
#include <algorithm>

using json = nlohmann::json;


static size_t write_cb(char* ptr, size_t sz, size_t nmemb, void* udata) {
    static_cast<std::string*>(udata)->append(ptr, sz * nmemb);
    return sz * nmemb;
}


GeminiClient::GeminiClient(std::vector<std::string> api_keys, Config cfg)
    : key_mgr_(std::move(api_keys)),
      cache_(cfg.cache_ttl_min),
      cfg_(std::move(cfg))
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_ = curl_easy_init();
    if (!curl_) throw std::runtime_error("Falha ao inicializar libcurl");
}

GeminiClient::GeminiClient(std::vector<std::string> api_keys)
    : GeminiClient(std::move(api_keys), Config{}) {}

GeminiClient::GeminiClient(std::string api_key, Config cfg)
    : GeminiClient(std::vector<std::string>{ std::move(api_key) }, std::move(cfg)) {}

GeminiClient::GeminiClient(std::string api_key)
    : GeminiClient(std::move(api_key), Config{}) {}

GeminiClient::~GeminiClient() {
    if (curl_) curl_easy_cleanup(static_cast<CURL*>(curl_));
    curl_global_cleanup();
}


std::string GeminiClient::build_payload(const std::vector<Message>& history) const {
    json body;

    if (!cfg_.system_prompt.empty()) {
        body["system_instruction"]["parts"][0]["text"] = cfg_.system_prompt;
    }

    json contents = json::array();

    contents.push_back({
        { "role",  "user"  },
        { "parts", json::array({ { { "text", "[início da conversa]" } } }) }
    });
    contents.push_back({
        { "role",  "model" },
        { "parts", json::array({ { { "text", "Entendido!" } } }) }
    });

    for (const auto& m : history) {
        contents.push_back({
            { "role",  m.role == "assistant" ? "model" : "user" },
            { "parts", json::array({ { { "text", m.content } } }) }
        });
    }

    body["contents"] = contents;
    body["generationConfig"] = {
        { "maxOutputTokens", cfg_.max_tokens   },
        { "temperature",     cfg_.temperature  },
        { "stopSequences",   json::array()     },
    };

    return body.dump();
}


std::string GeminiClient::http_post(const std::string& url, const std::string& body) {
    CURL* curl = static_cast<CURL*>(curl_);
    std::string response;
    long http_code = 0;

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       static_cast<long>(cfg_.timeout_sec));
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);

    if (res != CURLE_OK)
        throw GeminiException(std::format("Erro de rede: {}", curl_easy_strerror(res)));

    if (http_code == 429) throw RateLimitException("Rate limit atingido", 429);
    if (http_code == 400) throw InvalidKeyException("API key inválida ou requisição malformada", 400);
    if (http_code >= 400)
        throw GeminiException(std::format("Erro HTTP {}", http_code), static_cast<int32_t>(http_code));

    return response;
}


ChatResponse GeminiClient::parse_response(const std::string& raw) const {
    auto j = json::parse(raw);

    auto& candidate = j["candidates"][0];
    std::string finish = candidate.value("finishReason", "STOP");
    if (finish == "SAFETY")
        throw SafetyFilterException("Resposta bloqueada pelo filtro de segurança do Gemini");

    ChatResponse resp;
    resp.content       = candidate["content"]["parts"][0]["text"].get<std::string>();
    resp.finish_reason = finish;

    if (j.contains("usageMetadata")) {
        resp.input_tokens  = j["usageMetadata"].value("promptTokenCount",     0);
        resp.output_tokens = j["usageMetadata"].value("candidatesTokenCount", 0);
    }

    return resp;
}


ChatResponse GeminiClient::chat(const std::vector<Message>& history) {
    const auto user_msg = last_user_message(history);
    if (!user_msg.empty()) {
        if (auto cached = cache_.get(user_msg); !cached.empty()) {
            ChatResponse resp;
            resp.content = std::move(cached);
            resp.finish_reason = "STOP";
            resp.from_cache = true;
            return resp;
        }
    }

    const std::string url = std::format(
        "https://generativelanguage.googleapis.com/v1beta/models/{}:generateContent?key={}",
        cfg_.model, key_mgr_.current()
    );
    std::string payload = build_payload(history);
    std::string raw     = http_post(url, payload);
    ChatResponse resp   = parse_response(raw);

    if (!user_msg.empty() && !resp.content.empty())
        cache_.set(user_msg, resp.content);

    return resp;
}

std::string GeminiClient::last_user_message(const std::vector<Message>& history) {
    auto it = std::find_if(history.rbegin(), history.rend(), [](const Message& m) {
        return m.role == "user";
    });
    return (it == history.rend()) ? std::string{} : it->content;
}
