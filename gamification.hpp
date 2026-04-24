#pragma once

#include "idatabase.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

struct StreakResult {
    bool     achievement_3 = false;
    uint32_t streak_days   = 0;
};

struct MissionUpdate {
    int32_t     mission_id   = 0;
    std::string mission_desc;
    uint32_t    bonus_points = 0;
};

struct PointsCalculation {
    uint32_t    base_points = 0;
    uint32_t    final_points = 0;
    float       multiplier = 1.0f;
    std::string bonus_reason;
};

struct GiftResult {
    bool        success = false;
    std::string message;
};

class GamificationEngine {
public:
    explicit GamificationEngine(IDatabase& db) : db_(db) {}

    [[nodiscard]] StreakResult check_streak(int32_t user_id) const {
        StreakResult out;
        auto recent = db_.get_user_donations(user_id, 3);
        out.streak_days = static_cast<uint32_t>(recent.size());
        out.achievement_3 = recent.size() >= 3;
        return out;
    }

    [[nodiscard]] std::vector<MissionUpdate> update_missions(int32_t user_id) const {
        std::vector<MissionUpdate> completed;
        const auto missions = db_.get_active_missions();
        const auto donations_count = db_.get_user_donations(user_id, 1000).size();

        for (const auto& m : missions) {
            if (donations_count >= m.target) {
                completed.push_back(MissionUpdate{
                    .mission_id = m.id,
                    .mission_desc = m.description,
                    .bonus_points = m.bonus_points
                });
            }
        }
        return completed;
    }

    [[nodiscard]] PointsCalculation calculate_points(uint32_t base_points,
                                                     const Campaign& campaign) const {
        PointsCalculation out;
        out.base_points = base_points;
        out.final_points = base_points;

        if (campaign.urgent) {
            out.multiplier = 1.5f;
            out.final_points = static_cast<uint32_t>(base_points * out.multiplier);
            out.bonus_reason = "🔥 Campanha urgente: bônus de 50% aplicado.";
        }
        return out;
    }

    [[nodiscard]] GiftResult gift_points(const std::string& from_phone,
                                         const std::string& to_phone,
                                         uint32_t amount) const {
        if (amount == 0) {
            return { false, "❌ A quantidade de pontos deve ser maior que zero." };
        }
        if (from_phone == to_phone) {
            return { false, "❌ Você não pode transferir pontos para si mesmo." };
        }

        const auto sender = db_.get_or_create_user(from_phone);
        if (sender.points < amount) {
            return { false, "❌ Pontos insuficientes para transferir." };
        }

        return { false, "⚠️ Transferência de pontos indisponível no momento." };
    }

private:
    IDatabase& db_;
};
