// ============================================================
//  AlissonAsk V0.5 — ConversationManager Implementation
//  Criado e Integrado por: Álisson Ferreira Dos Santos
//  Arquivo: src/conversation_manager.cpp
// ============================================================

#include "conversation_manager.hpp"
#include <algorithm>

using Clock = std::chrono::steady_clock;

ConversationManager::ConversationManager(GeminiClient& client, Config cfg)
    : client_(client), cfg_(std::move(cfg)) {}

// ── Obtém sessão existente ou cria nova ───────────────────────

ConversationManager::Session&
ConversationManager::get_or_create(const std::string& user_id) {
    prune_expired();

    auto it = sessions_.find(user_id);
    if (it != sessions_.end()) {
        it->second.last_active = Clock::now();
        return it->second;
    }

    Session s;
    s.last_active = Clock::now();
    sessions_[user_id] = std::move(s);
    return sessions_[user_id];
}

// ── Remove sessões expiradas por inatividade ──────────────────

void ConversationManager::prune_expired() {
    const auto timeout = std::chrono::minutes(cfg_.timeout_min);
    const auto now     = Clock::now();
    std::erase_if(sessions_, [&](const auto& kv) {
        return (now - kv.second.last_active) > timeout;
    });
}

// ── Mantém histórico dentro do limite (preserva contexto recente) ─

void ConversationManager::trim_history(Session& s) {
    const auto max = static_cast<size_t>(cfg_.max_history);
    if (s.history.size() <= max) return;
    // Remove as mais antigas (índice 0 = mais antiga)
    s.history.erase(s.history.begin(),
                    s.history.begin() + static_cast<ptrdiff_t>(s.history.size() - max));
}

// ── Responde ao usuário ───────────────────────────────────────

ChatResponse ConversationManager::reply(
    const std::string& user_id,
    const std::string& user_message)
{
    auto& s = get_or_create(user_id);
    s.history.push_back({ "user", user_message });
    trim_history(s);

    ChatResponse resp = client_.chat(s.history);
    s.history.push_back({ "assistant", resp.content });

    return resp;
}

// ── Gerenciamento de sessões ──────────────────────────────────

void ConversationManager::reset_session(const std::string& user_id) {
    sessions_.erase(user_id);
}

void ConversationManager::reset_all() {
    sessions_.clear();
}

bool ConversationManager::has_session(const std::string& user_id) const {
    return sessions_.contains(user_id);
}

std::optional<std::vector<Message>>
ConversationManager::get_history(const std::string& user_id) const {
    auto it = sessions_.find(user_id);
    if (it == sessions_.end()) return std::nullopt;
    return it->second.history;
}
