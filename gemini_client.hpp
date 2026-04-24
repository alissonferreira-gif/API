#pragma once

#include "api_key_manager.hpp"
#include "response_cache.hpp"

#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>


struct Message {
    std::string role;      // "user" | "assistant"
    std::string content;
};

struct ChatResponse {
    std::string  content;
    std::string  finish_reason;
    int_fast32_t input_tokens  = 0;
    int_fast32_t output_tokens = 0;
    bool         from_cache    = false;  // true = economizou tokens
};


class GeminiException : public std::runtime_error {
public:
    explicit GeminiException(const std::string& msg, int32_t http = 0)
        : std::runtime_error(msg), http_code_(http) {}
    [[nodiscard]] int32_t http_code() const noexcept { return http_code_; }
private:
    int32_t http_code_;
};

class RateLimitException    : public GeminiException { using GeminiException::GeminiException; };
class InvalidKeyException   : public GeminiException { using GeminiException::GeminiException; };
class SafetyFilterException : public GeminiException { using GeminiException::GeminiException; };


class GeminiClient {
public:
    struct Config {
        std::string  model         = "gemini-2.0-flash";
        double       temperature   = 0.72;
        int_fast32_t max_tokens    = 512;
        int_fast32_t timeout_sec   = 20;
        std::string  system_prompt;
        int          cache_ttl_min = 60;   // tempo de vida do cache em minutos
    };

    explicit GeminiClient(std::vector<std::string> api_keys, Config cfg = {});

    explicit GeminiClient(std::string api_key, Config cfg = {});

    ~GeminiClient();

    [[nodiscard]] ChatResponse chat(const std::vector<Message>& history);

    void set_model      (std::string m)  { cfg_.model        = std::move(m); }
    void set_temperature(double t)       { cfg_.temperature  = t; }
    void set_max_tokens (int_fast32_t n) { cfg_.max_tokens   = n; }
    void set_system     (std::string s)  { cfg_.system_prompt = std::move(s); }

    [[nodiscard]] const Config& config()    const noexcept { return cfg_; }
    [[nodiscard]] size_t cache_size()       const noexcept { return cache_.size(); }
    [[nodiscard]] size_t active_key_index() const noexcept { return key_mgr_.current_index(); }

private:
    ApiKeyManager key_mgr_;
    ResponseCache cache_;
    Config        cfg_;
    void*         curl_ = nullptr;

    [[nodiscard]] std::string build_payload (const std::vector<Message>& history) const;
    [[nodiscard]] std::string http_post     (const std::string& url, const std::string& body);
    [[nodiscard]] ChatResponse parse_response(const std::string& raw) const;

    [[nodiscard]] static std::string last_user_message(const std::vector<Message>& history);
};
