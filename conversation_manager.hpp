#pragma once

#include "gemini_client.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <cstdint>

class ConversationManager {
public:
    struct Config {
        int_fast32_t max_history  = 20;   // máx de mensagens por sessão
        int_fast32_t timeout_min  = 30;   // minutos de inatividade → reset
        std::string  system_prompt;
    };

    explicit ConversationManager(GeminiClient& client, Config cfg = {});

    [[nodiscard]] ChatResponse reply(
        const std::string& user_id,
        const std::string& user_message
    );

    void   reset_session  (const std::string& user_id);
    void   reset_all      ();
    bool   has_session    (const std::string& user_id) const;
    size_t active_sessions() const noexcept { return sessions_.size(); }

    [[nodiscard]] std::optional<std::vector<Message>>
    get_history(const std::string& user_id) const;

private:
    struct Session {
        std::vector<Message>                    history;
        std::chrono::steady_clock::time_point   last_active;
    };

    GeminiClient&                               client_;
    Config                                      cfg_;
    std::unordered_map<std::string, Session>    sessions_;

    Session& get_or_create(const std::string& user_id);
    void     prune_expired();
    void     trim_history (Session& s);
};
