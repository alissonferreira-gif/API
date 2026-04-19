#pragma once
// ============================================================
//  AlissonAsk V0.6 — IDatabase
//  Interface do banco de dados. A equipe implementa essa
//  interface com o BD real (PostgreSQL, SQLite, etc.)
// ============================================================

#include <string>
#include <vector>
#include <cstdint>

// ── Estruturas de dados ───────────────────────────────────────

struct UserProfile {
    int32_t     id       = 0;
    std::string phone_id;
    std::string name;
    std::string level;
    uint32_t    points   = 0;
    uint32_t    donations = 0;
};

struct Campaign {
    int32_t     id;
    std::string slug;
    std::string name;
    std::string description;
    bool        urgent = false;
};

struct CollectionPoint {
    int32_t     id;
    std::string name;
    std::string address;
    std::string neighborhood;
};

struct Donation {
    int32_t     user_id     = 0;
    int32_t     campaign_id = 0;
    std::string type;           // "physical" | "online"
    uint32_t    points_earned = 0;
    int64_t     registered_at = 0;
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
    int32_t     user_id = 0;
    std::string full_name;
    std::string email;
    std::string available_days;
    int64_t     registered_at = 0;
};

// ── Interface IDatabase ───────────────────────────────────────

class IDatabase {
public:
    virtual ~IDatabase() = default;

    virtual UserProfile              get_or_create_user    (const std::string& phone_id)                          = 0;
    virtual uint32_t                 add_points            (int32_t user_id, uint32_t amount, const std::string& reason) = 0;
    virtual std::vector<Campaign>    get_active_campaigns  ()                                                      = 0;
    virtual void                     increment_campaign    (int32_t campaign_id, int_fast32_t amount)              = 0;
    virtual std::vector<CollectionPoint> get_collection_points(const std::string& neighborhood)                   = 0;
    virtual int64_t                  register_donation     (const Donation& d)                                     = 0;
    virtual std::vector<Donation>    get_user_donations    (int32_t user_id, int_fast32_t limit)                   = 0;
    virtual std::vector<RankingEntry> get_ranking          (int_fast32_t limit)                                    = 0;
    virtual int32_t                  get_user_rank         (int32_t user_id)                                       = 0;
    virtual std::vector<Achievement> get_user_achievements (int32_t user_id)                                      = 0;
    virtual bool                     unlock_achievement    (int32_t user_id, const std::string& slug)              = 0;
    virtual int32_t                  register_volunteer    (const VolunteerRegistration& reg)                      = 0;
};

// ── Função utilitária: pontos → nível ─────────────────────────

[[nodiscard]] inline std::string points_to_level(uint32_t pts) {
    if (pts >= 2500) return "⚡ Deus";
    if (pts >= 1500) return "🔥 Lendário";
    if (pts >= 800)  return "💎 Diamante";
    if (pts >= 400)  return "🥇 Ouro";
    if (pts >= 150)  return "🥈 Prata";
    if (pts >= 50)   return "🥉 Bronze";
    return "🤝 Iniciante";
}
