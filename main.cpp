
#include "gemini_client.hpp"
#include "conversation_manager.hpp"
#include "chatbot_engine.hpp"
#include "sqlite_database.hpp"
#include "rate_limiter.hpp"
#include "thread_pool.hpp"
#include "notification_scheduler.hpp"
#include "asm_utils.hpp"
#include "whatsapp_client.hpp"
#include "admin_router.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <cstdlib>
#include "print_compat.hpp"
#include <format>

using json = nlohmann::json;


[[nodiscard]] static std::string require_env(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v)
        throw std::runtime_error(std::format(
            "Variável {} não definida.\n"
            "Execute: export {}=seu_valor", name, name));
    return std::string(v);
}

[[nodiscard]] static std::string optional_env(const char* name,
                                               const std::string& fallback = "") {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : fallback;
}


constexpr std::string_view SYSTEM_PROMPT = R"(
Você é AlissonAsk V0.7, assistente oficial do projeto Solidário.
Criado e integrado por Álisson Ferreira Dos Santos.

Se perguntarem quem você é: "Sou AlissonAsk V0.7, criado por Álisson Ferreira Dos Santos."

Personalidade: empático, acolhedor, motivador.
Respostas curtas (máx 3 parágrafos). Use emojis com moderação.

Funcionalidades disponíveis:
- Doações online e físicas (QR code nos pontos de coleta)
- Voluntariado
- Ranking e gamificação (missões semanais, conquistas, streaks)
- Ponto de coleta mais próximo (peça a localização do usuário)
- Presentear pontos para amigos
- Resumo mensal de impacto

Campanhas ativas:
🍽️ Fome Zero – alimentos não-perecíveis (URGENTE 🔴)
👕 Agasalho 2025 – roupas de inverno
📚 Livros que Transformam – livros didáticos
💊 Farmácia Solidária – medicamentos (URGENTE 🔴)

Pontos: mensagem=+5, doação física=+50, online=+100, voluntariado=+200
Nível supremo: ⚡ Deus (2500 pts)
Doações em campanhas URGENTES nas primeiras 24h valem 2x pontos!

Finalize com pergunta ou call-to-action.
)";


static void run_demo(ChatbotEngine& engine) {
    println("╔══════════════════════════════════════════╗");
    println("║  AlissonAsk V0.7 — Modo Demo Terminal   ║");
    println("║  Criado por: Álisson Ferreira Dos Santos║");
    println("║  SQLite | Assembly | ThreadPool | IA    ║");
    println("╚══════════════════════════════════════════╝");
    println("Formato: <+numero> <mensagem>");
    println("Especiais:");
    println("  <+numero> !loc <lat> <lng>     → ponto de coleta próximo");
    println("  <+numero> !doar <campaign>     → doação online");
    println("  <+numero> !qr <token>          → scan QR code");
    println("  <+numero> !presentear <+dest> <pts> → presentear pontos");
    println("Digite 'sair' para encerrar.\n");

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "sair") break;

        auto sep = line.find(' ');
        if (sep == std::string::npos) {
            println("[!] Formato: <+numero> <mensagem>");
            continue;
        }

        std::string phone   = line.substr(0, sep);
        std::string content = line.substr(sep + 1);

        try {
            print("[{}] ", phone);
            EngineResult result;

            if (content.starts_with("!loc ")) {
                double lat = 0, lng = 0;
                std::sscanf(content.c_str(), "!loc %lf %lf", &lat, &lng);
                result = engine.handle_location(phone, lat, lng);
            } else if (content.starts_with("!doar ")) {
                result = engine.handle_donation(phone, content.substr(6));
            } else if (content.starts_with("!qr ")) {
                result = engine.handle_qr_scan(phone, content.substr(4));
            } else if (content.starts_with("!presentear ")) {
                std::string rest = content.substr(12);
                auto s = rest.find(' ');
                if (s != std::string::npos) {
                    std::string to_phone = rest.substr(0, s);
                    uint32_t pts = static_cast<uint32_t>(std::stoul(rest.substr(s+1)));
                    result = engine.handle_gift_points(phone, to_phone, pts);
                }
            } else {
                result = engine.handle_message(phone, content);
            }

            println("\n[AlissonAsk → {}]:\n{}", phone, result.reply);
            println("[Pts: {} | Nível: {} | Intenção: {}]",
                result.total_points, result.level,
                static_cast<int>(result.detected_intent));

            if (result.achievement_unlocked)
                println("🏆 CONQUISTA: {}", result.achievement_name);
            if (result.mission_completed)
                println("🎯 MISSÃO: {} +{}pts", result.mission_name, result.mission_bonus);
            if (result.rate_limited)
                println("⛔ RATE LIMITED");
            if (result.escalated_to_human)
                println("🆘 ESCALADO PARA HUMANO");
            println();

        } catch (const std::exception& e) {
            println("\n[ERRO] {}", e.what());
        }
    }
}


static void run_server(ChatbotEngine&       engine,
                        WhatsAppClient&      wa,
                        SQLiteDatabase&      db,
                        ThreadPool&          pool,
                        const std::string&   verify_token)
{
    httplib::Server svr;

    svr.Get("/webhook", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.get_param_value("hub.verify_token") == verify_token)
            res.set_content(req.get_param_value("hub.challenge"), "text/plain");
        else
            res.status = 403;
    });

    svr.Post("/webhook", [&](const httplib::Request& req, httplib::Response& res) {
        std::string body = req.body;

        pool.submit([&engine, &wa, &db, body] {
            try {
                auto wm = WhatsAppClient::parse_webhook(body);
                if (!wm.valid) return;

                println("[Webhook] De: {} | Msg: {}", wm.from, wm.text);

                EngineResult result;

                if (body.find("\"type\":\"location\"") != std::string::npos) {
                    double lat = 0, lng = 0;
                    GeoLocation::parse_location(body, lat, lng);
                    result = engine.handle_location(wm.from, lat, lng);
                } else {
                    result = engine.handle_message(wm.from, wm.text);
                }

                wa.send_text(wm.from, result.reply);

                if (result.achievement_unlocked)
                    wa.send_text(wm.from, std::format(
                        "🏆 Conquista desbloqueada: {}!\nTotal: {} pts — {}",
                        result.achievement_name, result.total_points, result.level));

                if (result.mission_completed)
                    wa.send_text(wm.from, std::format(
                        "🎯 Missão concluída: {}\n+{} pontos bônus! 🎉",
                        result.mission_name, result.mission_bonus));

            } catch (const std::exception& e) {
                println(stderr, "[Webhook ERRO] {}", e.what());
            }
        });

        res.status = 200;
        res.set_content("{}", "application/json");
    });

    svr.Post("/webhook/pix", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string phone      = j.value("phone_id", "");
            std::string campaign   = j.value("campaign_slug", "");
            std::string tx_id      = j.value("transaction_id", "");

            if (!phone.empty()) {
                pool.submit([&engine, &wa, phone, campaign, tx_id] {
                    auto result = engine.handle_donation(phone, campaign);
                    wa.send_text(phone, std::format(
                        "✅ PIX confirmado! (TX: {})\n\n{}", tx_id, result.reply));
                });
            }
        } catch (...) {}
        res.status = 200;
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/webhook/qr", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j    = json::parse(req.body);
            std::string phone = j.value("phone_id", "");
            std::string token = j.value("qr_token", "");

            if (!phone.empty() && !token.empty()) {
                pool.submit([&engine, &wa, phone, token] {
                    auto result = engine.handle_qr_scan(phone, token);
                    wa.send_text(phone, result.reply);
                });
            }
        } catch (...) {}
        res.status = 200;
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Get("/admin/csv/:year/:month", [&](const httplib::Request& req,
                                             httplib::Response& res) {
        std::string auth = req.get_header_value("Authorization");
        std::string expected = "Bearer " + optional_env("ADMIN_TOKEN", "admin123");
        if (auth != expected) { res.status = 401; return; }

        int year  = std::stoi(req.path_params.at("year"));
        int month = std::stoi(req.path_params.at("month"));

        try {
            db.log_admin_action("api", "export_csv",
                std::format("{}-{:02d}", year, month));
            std::string csv = db.export_donations_csv(year, month);
            res.set_content(csv, "text/csv");
            res.set_header("Content-Disposition",
                std::format("attachment; filename=doacoes-{}-{:02d}.csv", year, month));
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr.Get("/admin/ranking", [&](const httplib::Request& req, httplib::Response& res) {
        std::string auth = req.get_header_value("Authorization");
        if (auth != "Bearer " + optional_env("ADMIN_TOKEN", "admin123")) {
            res.status = 401; return;
        }
        auto ranking = db.get_ranking(20);
        json j = json::array();
        for (auto& e : ranking)
            j.push_back({{"rank",e.rank},{"name",e.name},{"points",e.points}});
        res.set_content(j.dump(2), "application/json");
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"version\":\"0.7\"}", "application/json");
    });

    println("╔══════════════════════════════════════════╗");
    println("║  AlissonAsk V0.7 — Servidor WhatsApp    ║");
    println("║  Porta: 8080 | ThreadPool: {} threads   ║",
        std::thread::hardware_concurrency());
    println("║  SQLite | Assembly | IA + Offline mode  ║");
    println("╚══════════════════════════════════════════╝");
    println("Endpoints:");
    println("  POST /webhook         → mensagens WhatsApp");
    println("  POST /webhook/pix     → confirmação PIX");
    println("  POST /webhook/qr      → scan QR code");
    println("  GET  /admin/csv/:y/:m → exportar CSV");
    println("  GET  /admin/ranking   → top 20");
    println("  GET  /health          → health check\n");

    svr.listen("0.0.0.0", 8080);
}


int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> keys;
        for (int i = 1; i <= 10; ++i) {
            std::string key = optional_env(
                std::format("GEMINI_KEY_{}", i).c_str());
            if (!key.empty()) keys.push_back(key);
        }
        if (keys.empty()) {
            keys.push_back(require_env("GEMINI_API_KEY"));
            println("[Aviso] Usando GEMINI_API_KEY legada. Configure GEMINI_KEY_1..10.");
        } else {
            println("[Keys] {} keys | Capacidade: {} req/dia",
                keys.size(), keys.size() * 1500);
        }

        GeminiClient::Config gcfg;
        gcfg.model        = "gemini-2.0-flash";
        gcfg.temperature  = 0.72;
        gcfg.max_tokens   = 512;
        gcfg.timeout_sec  = 20;
        gcfg.system_prompt= std::string(SYSTEM_PROMPT);
        gcfg.cache_ttl_min= 60;
        GeminiClient gemini(std::move(keys), gcfg);

        ConversationManager::Config ccfg;
        ccfg.max_history = 20;
        ccfg.timeout_min = 30;
        ConversationManager conv(gemini, ccfg);

        std::string db_path = optional_env("DB_PATH", "alisonask.db");
        SQLiteDatabase db(db_path);
        println("[DB] SQLite em: {}", db_path);

        ThreadPool pool;
        println("[ThreadPool] {} threads", pool.thread_count());

        ChatbotEngine engine(gemini, conv, db);

        bool server_mode = (argc > 1 && std::string(argv[1]) == "--server");

        if (server_mode) {
            const std::string wa_phone  = require_env("WA_PHONE_NUMBER_ID");
            const std::string wa_token  = require_env("WA_ACCESS_TOKEN");
            const std::string wa_verify = require_env("WA_VERIFY_TOKEN");

            WhatsAppClient::Config wcfg{ wa_phone, wa_token };
            WhatsAppClient wa(wcfg);

            NotificationScheduler scheduler(
                db, pool,
                [&wa](const std::string& phone, const std::string& msg) {
                    wa.send_text(phone, msg);
                },
                [](const std::string& subj, const std::string& body) {
                    println("[Alert] {}: {}", subj, body);
                }
            );
            scheduler.start();

            run_server(engine, wa, db, pool, wa_verify);

        } else {
            run_demo(engine);
        }

    } catch (const std::exception& e) {
        println(stderr, "[FATAL] {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
