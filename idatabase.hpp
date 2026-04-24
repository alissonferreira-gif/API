#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>


struct UserProfile {
    int32_t     id       = 0;
    std::string phone_id;
    std::string name;
    std::string level;
    uint32_t    points    = 0;
    uint32_t    donations = 0;
};

struct Campaign {
    int32_t     id;
    std::string slug;
    std::string name;
    std::string description;
    bool        urgent        = false;
    uint32_t    total_donated = 0;
    uint32_t    goal          = 0;

    [[nodiscard]] int progress_pct() const noexcept {
        if (goal == 0) return 0;
        int p = static_cast<int>((total_donated * 100u) / goal);
        return p > 100 ? 100 : p;
    }

    [[nodiscard]] std::string progress_bar() const {
        int pct   = progress_pct();
        int filled = pct / 20;          // 5 blocos = 100%
        std::string bar;
        for (int i = 0; i < 5; ++i)
            bar += (i < filled) ? "🟩" : "⬜";
        return bar + " " + std::to_string(pct) + "%";
    }
};

struct CollectionPoint {
    int32_t     id;
    std::string name;
    std::string address;
    std::string neighborhood;
    double      lat = 0.0;     // latitude (para rota)
    double      lng = 0.0;     // longitude
    std::string qr_code;       // token único para registro físico
};

struct Donation {
    int32_t     user_id       = 0;
    int32_t     campaign_id   = 0;
    std::string type;           // "physical" | "online" | "qr_scan"
    uint32_t    points_earned  = 0;
    int64_t     registered_at  = 0;
};

struct RankingEntry {
    int32_t     user_id;
    std::string name;
    uint32_t    points;
    int32_t     rank;
};

struct Achievement {
    std::string slug;
    std::string name;
    std::string description;
};

struct VolunteerRegistration {
    int32_t     user_id       = 0;
    std::string full_name;
    std::string email;
    std::string available_days;
    int64_t     registered_at = 0;
};

struct WeeklyMission {
    int32_t     id;
    std::string description;
    uint32_t    target;
    uint32_t    bonus_points;
    int64_t     expires_at;
};

struct DonationGroup {
    int32_t     id;
    std::string name;
    int32_t     created_by;
    uint32_t    total_points = 0;
    std::vector<int32_t> member_ids;
};


class IDatabase {
public:
    virtual ~IDatabase() = default;

    virtual UserProfile get_or_create_user (const std::string& phone_id) = 0;
    virtual uint32_t    add_points         (int32_t user_id, uint32_t amount,
                                            const std::string& reason) = 0;

    virtual std::vector<Campaign> get_active_campaigns  ()                              = 0;
    virtual void                  increment_campaign     (int32_t id, int_fast32_t amt) = 0;

    virtual std::vector<CollectionPoint> get_collection_points(const std::string& neighborhood) = 0;

    virtual int64_t              register_donation  (const Donation& d)                    = 0;
    virtual std::vector<Donation> get_user_donations (int32_t user_id, int_fast32_t limit) = 0;

    virtual std::vector<RankingEntry> get_ranking   (int_fast32_t limit) = 0;
    virtual int32_t                   get_user_rank (int32_t user_id)    = 0;

    virtual std::vector<Achievement> get_user_achievements (int32_t user_id)                      = 0;
    virtual bool                     unlock_achievement     (int32_t user_id, const std::string& slug) = 0;

    virtual int32_t register_volunteer(const VolunteerRegistration& reg) = 0;

    virtual void        save_user_memory(const std::string& phone_id,
                                         const std::string& memory_json) = 0;
    virtual std::string get_user_memory (const std::string& phone_id)    = 0;

    virtual std::vector<WeeklyMission> get_active_missions() = 0;

    virtual std::string export_donations_csv(int year, int month)            = 0;
    virtual void        log_admin_action    (const std::string& admin_id,
                                             const std::string& action,
                                             const std::string& detail = "") = 0;
};

[[nodiscard]] inline std::string points_to_level(uint32_t pts) {
    if (pts >= 2500) return "⚡ Deus";
    if (pts >= 1500) return "🔥 Lendário";
    if (pts >= 800)  return "💎 Diamante";
    if (pts >= 400)  return "🥇 Ouro";
    if (pts >= 150)  return "🥈 Prata";
    if (pts >= 50)   return "🥉 Bronze";
    return "🤝 Iniciante";
}
