#pragma once
// ============================================================
// AlissonAsk V0.7 — chatbot_engine.hpp  (substitui V0.5)
// Orquestra todos os sistemas:
//   - IntentClassifier (zero tokens para intenções conhecidas)
//   - SentimentAnalyzer (escala para humano se frustrado)
//   - GamificationEngine (missões, multiplicadores, streaks)
//   - GeoLocation (ponto de coleta mais próximo)
//   - RateLimiter (anti-spam por phone_id)
//   - SQLiteDatabase (persistência real)
//   - Modo offline (fallback sem Gemini)
//   - Memória de usuário persistida
// ============================================================

#include "gemini_client.hpp"
#include "conversation_manager.hpp"
#include "idatabase.hpp"
#include "intent_classifier.hpp"
#include "sentiment_analyzer.hpp"
#include "gamification.hpp"
#include "geolocation.hpp"
#include "rate_limiter.hpp"
#include "asm_utils.hpp"
#include <string>
#include <cstdint>
#include <optional>

// Resultado de cada interação
struct EngineResult {
    std::string reply;

    uint32_t    points_earned  = 0;
    uint32_t    total_points   = 0;
    std::string level;

    bool        achievement_unlocked = false;
    std::string achievement_name;

    bool        mission_completed    = false;
    std::string mission_name;
    uint32_t    mission_bonus        = 0;

    bool        rate_limited         = false;
    bool        escalated_to_human   = false;

    Intent      detected_intent      = Intent::UNKNOWN;
    Sentiment   sentiment            = Sentiment::NEUTRAL;
};

class ChatbotEngine {
public:
    explicit ChatbotEngine(
        GeminiClient&      gemini,
        ConversationManager& conv,
        IDatabase&         db
    );

    // Processa mensagem de texto vinda do WhatsApp
    [[nodiscard]] EngineResult handle_message(
        const std::string& phone_id,
        const std::string& message
    );

    // Processa localização enviada pelo usuário
    [[nodiscard]] EngineResult handle_location(
        const std::string& phone_id,
        double lat, double lng
    );

    // Processa doação online (webhook PIX ou botão)
    [[nodiscard]] EngineResult handle_donation(
        const std::string& phone_id,
        const std::string& campaign_id
    );

    // Processa scan de QR code de ponto de coleta
    [[nodiscard]] EngineResult handle_qr_scan(
        const std::string& phone_id,
        const std::string& qr_token
    );

    // Processa cadastro de voluntário
    [[nodiscard]] EngineResult handle_volunteer(
        const std::string& phone_id,
        const std::string& full_name,
        const std::string& email,
        const std::string& available_days
    );

    // Presentear pontos para amigo
    [[nodiscard]] EngineResult handle_gift_points(
        const std::string& from_phone,
        const std::string& to_phone,
        uint32_t amount
    );

private:
    GeminiClient&        gemini_;
    ConversationManager& conv_;
    IDatabase&           db_;

    IntentClassifier     classifier_;
    SentimentAnalyzer    sentiment_;
    GamificationEngine   gamification_;
    GeoLocation          geo_;
    RateLimiter          rate_limiter_;

    // Verifica e desbloqueia conquistas após cada ação
    void check_achievements(const UserProfile& user, EngineResult& out);

    // Adiciona contexto de memória ao prompt do Gemini
    std::string build_context(const std::string& phone_id,
                               const UserProfile& user);

    // Persiste fatos relevantes da conversa na memória do usuário
    void update_user_memory(const std::string& phone_id,
                             const std::string& message,
                             Intent intent);

    // Escalona para atendente humano via webhook
    void escalate_to_human(const std::string& phone_id,
                            const std::string& message,
                            const std::string& reason);

    static int64_t now_unix();
};
