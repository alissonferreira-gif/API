// ============================================================
//  AlissonAsk V0.5 — GeminiClient Implementation
//  Criado e Integrado por: Álisson Ferreira Dos Santos
//  Arquivo: src/gemini_client.cpp
// ============================================================

#include "gemini_client.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>
#include <format>   // C++23

using json = nlohmann::json;

// ── cURL write callback ───────────────────────────────────────

static size_t write_cb(char* ptr, size_t sz, size_t nmemb, void* udata) {
    static_cast<std::string*>(udata)->append(ptr, sz * nmemb);
    return sz * nmemb;
}

// ── Construtor / Destrutor ────────────────────────────────────

GeminiClient::GeminiClient(std::string api_key, Config cfg)
    : api_key_(std::move(api_key)), cfg_(std::move(cfg))
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_ = curl_easy_init();
    if (!curl_) throw std::runtime_error("Falha ao inicializar libcurl");
}

GeminiClient::~GeminiClient() {
    if (curl_) curl_easy_cleanup(static_cast<CURL*>(curl_));
    curl_global_cleanup();
}

// ── Monta payload JSON para a API do Gemini ───────────────────

std::string GeminiClient::build_payload(const std::vector<Message>& history) const {
    json body;

    // System instruction (separado do histórico na API Gemini)
    if (!cfg_.system_prompt.empty()) {
        body["system_instruction"]["parts"][0]["text"] = cfg_.system_prompt;
    }

    // Histórico de mensagens
    json contents = json::array();

    // Gemini exige iniciar com role "user" — injeta turno dummy se necessário
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

// ── HTTP POST via libcurl ─────────────────────────────────────

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

// ── Parseia resposta JSON ─────────────────────────────────────

ChatResponse GeminiClient::parse_response(const std::string& raw) const {
    auto j = json::parse(raw);

    // Verifica filtro de segurança
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

// ── Método público chat() ─────────────────────────────────────

ChatResponse GeminiClient::chat(const std::vector<Message>& history) {
    const std::string url = std::format(
        "https://generativelanguage.googleapis.com/v1beta/models/{}:generateContent?key={}",
        cfg_.model, api_key_
    );
    std::string payload = build_payload(history);
    std::string raw     = http_post(url, payload);
    return parse_response(raw);
}
