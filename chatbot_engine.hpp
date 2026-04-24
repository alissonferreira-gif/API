#pragma once

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

    [[nodiscard]] EngineResult handle_message(
        const std::string& phone_id,
        const std::string& message
    );

    [[nodiscard]] EngineResult handle_location(
        const std::string& phone_id,
        double lat, double lng
    );

    [[nodiscard]] EngineResult handle_donation(
        const std::string& phone_id,
        const std::string& campaign_id
    );

    [[nodiscard]] EngineResult handle_qr_scan(
        const std::string& phone_id,
        const std::string& qr_token
    );

    [[nodiscard]] EngineResult handle_volunteer(
        const std::string& phone_id,
        const std::string& full_name,
        const std::string& email,
        const std::string& available_days
    );

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

    void check_achievements(const UserProfile& user, EngineResult& out);

    std::string build_context(const std::string& phone_id,
                               const UserProfile& user);

    void update_user_memory(const std::string& phone_id,
                             const std::string& message,
                             Intent intent);

    void escalate_to_human(const std::string& phone_id,
                            const std::string& message,
                            const std::string& reason);

    static int64_t now_unix();
};
