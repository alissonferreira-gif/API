
static void run_server(ChatbotEngine& engine, WhatsAppClient& wa,
                       const std::string& verify_token,
                       const std::string& admin_token,
                       IDatabase& db)
{
    httplib::Server svr;

    AdminRouter admin(db, admin_token);
    admin.register_routes(svr);

    svr.Get("/webhook", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.get_param_value("hub.verify_token") == verify_token)
            res.set_content(req.get_param_value("hub.challenge"), "text/plain");
        else
            res.status = 403;
    });

    svr.Post("/webhook", [&](const httplib::Request& req, httplib::Response& res) {
        auto wm = WhatsAppClient::parse_webhook(req.body);
        if (!wm.valid) { res.status = 200; return; }

        println("[Webhook] De: {} | Msg: {}", wm.from, wm.text);

        try {
            auto result = engine.handle_message(wm.from, wm.text);
            wa.send_text(wm.from, result.reply);

            if (result.achievement_unlocked) {
                wa.send_text(wm.from, std::format(
                    "🏆 Você desbloqueou: {}!\n\nTotal: {} pts — Nível: {}",
                    result.achievement_name, result.total_points, result.level));
            }
        } catch (const std::exception& e) {
            println("[ERRO] {}", e.what());
        }

        res.status = 200;
    });

    println("╔══════════════════════════════════════════════╗");
    println("║   AlissonAsk V0.6 — Servidor Completo        ║");
    println("║   Porta: 8080                                 ║");
    println("║   /webhook        → WhatsApp                  ║");
    println("║   /admin/*        → Painel Administrativo     ║");
    println("╚══════════════════════════════════════════════╝");
    svr.listen("0.0.0.0", 8080);
}

