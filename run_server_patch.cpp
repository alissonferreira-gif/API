// ============================================================
//  AlissonAsk V0.6 — run_server atualizado
//  Substitua a função run_server no seu main.cpp por esta.
//  Adicione também o include no topo:
//    #include "admin_router.hpp"
// ============================================================

static void run_server(ChatbotEngine& engine, WhatsAppClient& wa,
                       const std::string& verify_token,
                       const std::string& admin_token,
                       IDatabase& db)
{
    httplib::Server svr;

    // ── Rotas Admin ───────────────────────────────────────
    AdminRouter admin(db, admin_token);
    admin.register_routes(svr);

    // ── Verificação do webhook WhatsApp ───────────────────
    svr.Get("/webhook", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.get_param_value("hub.verify_token") == verify_token)
            res.set_content(req.get_param_value("hub.challenge"), "text/plain");
        else
            res.status = 403;
    });

    // ── Recebe mensagens do WhatsApp ──────────────────────
    svr.Post("/webhook", [&](const httplib::Request& req, httplib::Response& res) {
        auto wm = WhatsAppClient::parse_webhook(req.body);
        if (!wm.valid) { res.status = 200; return; }

        std::println("[Webhook] De: {} | Msg: {}", wm.from, wm.text);

        try {
            auto result = engine.handle_message(wm.from, wm.text);
            wa.send_text(wm.from, result.reply);

            if (result.achievement_unlocked) {
                wa.send_text(wm.from, std::format(
                    "🏆 Você desbloqueou: {}!\n\nTotal: {} pts — Nível: {}",
                    result.achievement_name, result.total_points, result.level));
            }
        } catch (const std::exception& e) {
            std::println("[ERRO] {}", e.what());
        }

        res.status = 200;
    });

    std::println("╔══════════════════════════════════════════════╗");
    std::println("║   AlissonAsk V0.6 — Servidor Completo        ║");
    std::println("║   Porta: 8080                                 ║");
    std::println("║   /webhook        → WhatsApp                  ║");
    std::println("║   /admin/*        → Painel Administrativo     ║");
    std::println("╚══════════════════════════════════════════════╝");
    svr.listen("0.0.0.0", 8080);
}

// ── No main(), adicione a leitura do admin token: ─────────
//
//  const std::string admin_token = require_env("ADMIN_TOKEN");
//  run_server(engine, wa, wa_verify, admin_token, db);
