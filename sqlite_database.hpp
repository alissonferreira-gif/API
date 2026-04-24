#pragma once

#include "idatabase.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <format>
#include <chrono>

#define SQL_CHECK(rc, db) \
    if ((rc) != SQLITE_OK) \
        throw std::runtime_error(std::format("SQLite: {}", sqlite3_errmsg(db)))

class SQLiteDatabase final : public IDatabase {
public:
    explicit SQLiteDatabase(const std::string& path = "alisonask.db") {
        int rc = sqlite3_open(path.c_str(), &db_);
        SQL_CHECK(rc, db_);
        sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        create_schema();
    }

    ~SQLiteDatabase() override {
        if (db_) sqlite3_close(db_);
    }


    UserProfile get_or_create_user(const std::string& phone_id) override {
        exec(std::format(
            "INSERT OR IGNORE INTO users(phone_id, name, level, points, donations) "
            "VALUES('{}','Usuário {}','🤝 Iniciante',0,0);",
            esc(phone_id), phone_id.size() >= 4
                ? phone_id.substr(phone_id.size()-4) : phone_id));

        UserProfile u;
        auto stmt = prepare(std::format(
            "SELECT id,phone_id,name,level,points,donations FROM users WHERE phone_id='{}';",
            esc(phone_id)));
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            u.id        = sqlite3_column_int(stmt, 0);
            u.phone_id  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            u.name      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            u.level     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            u.points    = static_cast<uint32_t>(sqlite3_column_int(stmt, 4));
            u.donations = static_cast<uint32_t>(sqlite3_column_int(stmt, 5));
        }
        sqlite3_finalize(stmt);
        return u;
    }

    uint32_t add_points(int32_t user_id, uint32_t amount,
                        const std::string& reason) override {
        exec(std::format(
            "UPDATE users SET points = points + {} WHERE id = {};",
            amount, user_id));
        auto stmt = prepare(std::format(
            "SELECT points FROM users WHERE id = {};", user_id));
        uint32_t total = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            total = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
        sqlite3_finalize(stmt);

        std::string lvl = points_to_level(total);
        exec(std::format(
            "UPDATE users SET level = '{}' WHERE id = {};",
            esc(lvl), user_id));

        exec(std::format(
            "INSERT INTO audit_log(user_id,action,detail,ts) VALUES({},'add_points','{}+{}',{});",
            user_id, esc(reason), amount, now_unix()));

        return total;
    }

    std::vector<Campaign> get_active_campaigns() override {
        std::vector<Campaign> result;
        auto stmt = prepare(
            "SELECT id,slug,name,description,urgent FROM campaigns WHERE active=1;");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Campaign c;
            c.id          = sqlite3_column_int(stmt, 0);
            c.slug        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            c.name        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            c.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            c.urgent      = sqlite3_column_int(stmt, 4) != 0;
            result.push_back(c);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    void increment_campaign(int32_t campaign_id, int_fast32_t amount) override {
        exec(std::format(
            "UPDATE campaigns SET total_donated = total_donated + {} WHERE id = {};",
            amount, campaign_id));
    }

    std::vector<CollectionPoint> get_collection_points(
        const std::string& neighborhood) override
    {
        std::vector<CollectionPoint> result;
        std::string sql;
        if (neighborhood.empty())
            sql = "SELECT id,name,address,neighborhood,lat,lng FROM collection_points;";
        else
            sql = std::format(
                "SELECT id,name,address,neighborhood,lat,lng FROM collection_points "
                "WHERE neighborhood LIKE '%{}%';", esc(neighborhood));

        auto stmt = prepare(sql);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CollectionPoint cp;
            cp.id           = sqlite3_column_int(stmt, 0);
            cp.name         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            cp.address      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            cp.neighborhood = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            cp.lat          = sqlite3_column_double(stmt, 4);
            cp.lng          = sqlite3_column_double(stmt, 5);
            result.push_back(cp);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    int64_t register_donation(const Donation& d) override {
        exec(std::format(
            "INSERT INTO donations(user_id,campaign_id,type,points_earned,registered_at) "
            "VALUES({},{},{}'{}',{},{});",
            d.user_id, d.campaign_id,
            d.campaign_id == 0 ? "NULL," : std::format("{},", d.campaign_id),
            esc(d.type), d.points_earned, d.registered_at));
        int64_t id = sqlite3_last_insert_rowid(db_);
        exec(std::format(
            "UPDATE users SET donations = donations + 1 WHERE id = {};", d.user_id));
        return id;
    }

    std::vector<Donation> get_user_donations(int32_t user_id,
                                              int_fast32_t limit) override {
        std::vector<Donation> result;
        auto stmt = prepare(std::format(
            "SELECT user_id,campaign_id,type,points_earned,registered_at "
            "FROM donations WHERE user_id={} ORDER BY registered_at DESC LIMIT {};",
            user_id, limit));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Donation d;
            d.user_id       = sqlite3_column_int(stmt, 0);
            d.campaign_id   = sqlite3_column_int(stmt, 1);
            d.type          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            d.points_earned = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));
            d.registered_at = sqlite3_column_int64(stmt, 4);
            result.push_back(d);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    std::vector<RankingEntry> get_ranking(int_fast32_t limit) override {
        std::vector<RankingEntry> result;
        auto stmt = prepare(std::format(
            "SELECT id,name,points, "
            "ROW_NUMBER() OVER (ORDER BY points DESC) as rank "
            "FROM users ORDER BY points DESC LIMIT {};", limit));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            RankingEntry e;
            e.user_id = sqlite3_column_int(stmt, 0);
            e.name    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            e.points  = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
            e.rank    = sqlite3_column_int(stmt, 3);
            result.push_back(e);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    int32_t get_user_rank(int32_t user_id) override {
        auto stmt = prepare(std::format(
            "SELECT COUNT(*)+1 FROM users WHERE points > "
            "(SELECT points FROM users WHERE id={});", user_id));
        int32_t rank = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            rank = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return rank;
    }

    std::vector<Achievement> get_user_achievements(int32_t user_id) override {
        std::vector<Achievement> result;
        auto stmt = prepare(std::format(
            "SELECT a.slug,a.name,a.description FROM achievements a "
            "JOIN user_achievements ua ON ua.achievement_slug=a.slug "
            "WHERE ua.user_id={};", user_id));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Achievement a;
            a.slug        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            a.name        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            a.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            result.push_back(a);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    bool unlock_achievement(int32_t user_id, const std::string& slug) override {
        auto check = prepare(std::format(
            "SELECT 1 FROM user_achievements WHERE user_id={} AND achievement_slug='{}';",
            user_id, esc(slug)));
        bool already = (sqlite3_step(check) == SQLITE_ROW);
        sqlite3_finalize(check);
        if (already) return false;

        exec(std::format(
            "INSERT OR IGNORE INTO user_achievements(user_id,achievement_slug,unlocked_at) "
            "VALUES({},'{}',{});", user_id, esc(slug), now_unix()));
        return true;
    }

    int32_t register_volunteer(const VolunteerRegistration& reg) override {
        exec(std::format(
            "INSERT INTO volunteers(user_id,full_name,email,available_days,registered_at) "
            "VALUES({},'{}','{}','{}',{});",
            reg.user_id, esc(reg.full_name), esc(reg.email),
            esc(reg.available_days), reg.registered_at));
        return static_cast<int32_t>(sqlite3_last_insert_rowid(db_));
    }


    void save_user_memory(const std::string& phone_id,
                          const std::string& memory_json) {
        exec(std::format(
            "INSERT OR REPLACE INTO user_memory(phone_id,memory_json,updated_at) "
            "VALUES('{}','{}',{});",
            esc(phone_id), esc(memory_json), now_unix()));
    }

    std::string get_user_memory(const std::string& phone_id) {
        auto stmt = prepare(std::format(
            "SELECT memory_json FROM user_memory WHERE phone_id='{}';",
            esc(phone_id)));
        std::string mem;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (t) mem = t;
        }
        sqlite3_finalize(stmt);
        return mem;
    }

    std::vector<WeeklyMission> get_active_missions() override {
        std::vector<WeeklyMission> result;
        int64_t now = now_unix();
        auto stmt = prepare(std::format(
            "SELECT id,description,target,bonus_points,expires_at "
            "FROM weekly_missions WHERE expires_at > {} AND active=1;", now));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            WeeklyMission m;
            m.id           = sqlite3_column_int(stmt, 0);
            m.description  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            m.target       = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
            m.bonus_points = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));
            m.expires_at   = sqlite3_column_int64(stmt, 4);
            result.push_back(m);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    std::string export_donations_csv(int year, int month) {
        std::string csv = "id,user_phone,campaign,type,points,date\n";
        auto stmt = prepare(std::format(
            "SELECT d.rowid, u.phone_id, c.name, d.type, d.points_earned, d.registered_at "
            "FROM donations d "
            "LEFT JOIN users u ON u.id=d.user_id "
            "LEFT JOIN campaigns c ON c.id=d.campaign_id "
            "WHERE strftime('%Y','{}',d.registered_at,'unixepoch')='{}' "
            "AND   strftime('%m','{}',d.registered_at,'unixepoch')='{:02d}';",
            "", year, "", month));

        sqlite3_finalize(stmt);
        stmt = prepare(
            "SELECT d.rowid, u.phone_id, COALESCE(c.name,'N/A'), d.type, "
            "d.points_earned, d.registered_at "
            "FROM donations d "
            "LEFT JOIN users u ON u.id=d.user_id "
            "LEFT JOIN campaigns c ON c.id=d.campaign_id;");

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int64_t ts = sqlite3_column_int64(stmt, 5);
            std::time_t t = static_cast<std::time_t>(ts);
            std::tm* tm_  = std::gmtime(&t);
            if (!tm_) continue;
            if ((tm_->tm_year + 1900) != year) continue;
            if ((tm_->tm_mon + 1) != month)    continue;

            char date_buf[32];
            std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_);

            csv += std::format("{},{},{},{},{},{}\n",
                sqlite3_column_int(stmt, 0),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
                sqlite3_column_int(stmt, 4),
                date_buf);
        }
        sqlite3_finalize(stmt);
        return csv;
    }

    void log_admin_action(const std::string& admin_id,
                          const std::string& action,
                          const std::string& detail = "") {
        exec(std::format(
            "INSERT INTO audit_log(user_id,action,detail,ts) VALUES(0,'ADMIN:{}','{}:{} {}',{});",
            esc(action), esc(admin_id), esc(action), esc(detail), now_unix()));
    }

private:
    sqlite3* db_ = nullptr;

    static int64_t now_unix() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static std::string esc(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 4);
        for (char c : s) {
            if (c == '\'') out += "''";
            else out += c;
        }
        return out;
    }

    void exec(const std::string& sql) {
        char* err = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::string msg = err ? err : "unknown";
            sqlite3_free(err);
            throw std::runtime_error("SQLite exec: " + msg + "\nSQL: " + sql);
        }
    }

    sqlite3_stmt* prepare(const std::string& sql) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
            throw std::runtime_error(std::format("SQLite prepare: {}", sqlite3_errmsg(db_)));
        return stmt;
    }

    void create_schema() {
        const char* schema = R"SQL(
        CREATE TABLE IF NOT EXISTS users (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            phone_id    TEXT UNIQUE NOT NULL,
            name        TEXT NOT NULL DEFAULT '',
            level       TEXT NOT NULL DEFAULT '🤝 Iniciante',
            points      INTEGER NOT NULL DEFAULT 0,
            donations   INTEGER NOT NULL DEFAULT 0,
            created_at  INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        );
        CREATE TABLE IF NOT EXISTS campaigns (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            slug          TEXT UNIQUE NOT NULL,
            name          TEXT NOT NULL,
            description   TEXT NOT NULL DEFAULT '',
            urgent        INTEGER NOT NULL DEFAULT 0,
            active        INTEGER NOT NULL DEFAULT 1,
            total_donated INTEGER NOT NULL DEFAULT 0,
            goal          INTEGER NOT NULL DEFAULT 0,
            created_at    INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        );
        CREATE TABLE IF NOT EXISTS collection_points (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT NOT NULL,
            address      TEXT NOT NULL,
            neighborhood TEXT NOT NULL DEFAULT '',
            lat          REAL NOT NULL DEFAULT 0.0,
            lng          REAL NOT NULL DEFAULT 0.0,
            qr_code      TEXT NOT NULL DEFAULT ''
        );
        CREATE TABLE IF NOT EXISTS donations (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id       INTEGER NOT NULL,
            campaign_id   INTEGER,
            type          TEXT NOT NULL DEFAULT 'physical',
            points_earned INTEGER NOT NULL DEFAULT 0,
            registered_at INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS achievements (
            slug        TEXT PRIMARY KEY,
            name        TEXT NOT NULL,
            description TEXT NOT NULL DEFAULT ''
        );
        CREATE TABLE IF NOT EXISTS user_achievements (
            user_id          INTEGER NOT NULL,
            achievement_slug TEXT NOT NULL,
            unlocked_at      INTEGER NOT NULL,
            PRIMARY KEY (user_id, achievement_slug)
        );
        CREATE TABLE IF NOT EXISTS volunteers (
            id             INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id        INTEGER NOT NULL,
            full_name      TEXT NOT NULL,
            email          TEXT NOT NULL,
            available_days TEXT NOT NULL DEFAULT '',
            registered_at  INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS user_memory (
            phone_id     TEXT PRIMARY KEY,
            memory_json  TEXT NOT NULL DEFAULT '{}',
            updated_at   INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS weekly_missions (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            description  TEXT NOT NULL,
            target       INTEGER NOT NULL DEFAULT 1,
            bonus_points INTEGER NOT NULL DEFAULT 150,
            active       INTEGER NOT NULL DEFAULT 1,
            expires_at   INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS mission_progress (
            user_id    INTEGER NOT NULL,
            mission_id INTEGER NOT NULL,
            progress   INTEGER NOT NULL DEFAULT 0,
            completed  INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (user_id, mission_id)
        );
        CREATE TABLE IF NOT EXISTS donation_groups (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT NOT NULL,
            created_by INTEGER NOT NULL,
            created_at INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS group_members (
            group_id INTEGER NOT NULL,
            user_id  INTEGER NOT NULL,
            PRIMARY KEY (group_id, user_id)
        );
        CREATE TABLE IF NOT EXISTS audit_log (
            id      INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL DEFAULT 0,
            action  TEXT NOT NULL,
            detail  TEXT NOT NULL DEFAULT '',
            ts      INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_donations_user   ON donations(user_id);
        CREATE INDEX IF NOT EXISTS idx_donations_ts     ON donations(registered_at);
        CREATE INDEX IF NOT EXISTS idx_users_points     ON users(points DESC);
        CREATE INDEX IF NOT EXISTS idx_audit_ts         ON audit_log(ts);

        -- Conquistas padrão
        INSERT OR IGNORE INTO achievements VALUES
            ('first_don',   '💝 Primeiro Coração',   'Realizou a primeira doação'),
            ('don_5',       '🌟 Coração Generoso',   '5 doações realizadas'),
            ('don_20',      '💎 Doador de Elite',    '20 doações realizadas'),
            ('volunteer',   '🙌 Voluntário',         'Cadastrou-se como voluntário'),
            ('god_level',   '⚡ Ascensão Divina',    'Atingiu o nível Deus'),
            ('streak_3',    '🔥 Em Chamas',          '3 dias consecutivos doando'),
            ('group_leader','👑 Líder Solidário',    'Criou um grupo de doação');

        -- Campanhas padrão (se não existirem)
        INSERT OR IGNORE INTO campaigns(slug,name,description,urgent,goal) VALUES
            ('fome_zero',   '🍽️ Fome Zero',           'Alimentos não-perecíveis',       1, 500),
            ('agasalho',    '👕 Agasalho 2025',       'Roupas de inverno',              0, 300),
            ('livros',      '📚 Livros que Transformam','Livros didáticos',             0, 200),
            ('farmacia',    '💊 Farmácia Solidária',  'Medicamentos',                   1, 400);

        -- Pontos de coleta padrão
        INSERT OR IGNORE INTO collection_points(id,name,address,neighborhood,lat,lng) VALUES
            (1,'Casa da Cidadania','Rua das Flores, 123','Centro',   -23.5505, -46.6333),
            (2,'Igreja São Francisco','Av. Principal, 456','Jardim', -23.5605, -46.6433),
            (3,'CRAS Municipal','Rua Esperança, 789','Periferia',   -23.5705, -46.6533);
        )SQL";

        exec(schema);
    }
};
