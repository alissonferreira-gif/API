// ============================================================
// AlissonAsk V0.7 — chatbot_engine.cpp
// Criado e Integrado por: Álisson Ferreira Dos Santos
//
// Novidades V0.7:
//   - IntentClassifier local (zero tokens para ações conhecidas)
//   - SentimentAnalyzer + escalonamento humano
//   - GamificationEngine: missões, multiplicadores 2x, streaks
//   - GeoLocation: ponto mais próximo via Haversine
//   - RateLimiter: anti-spam sliding window (Assembly hash)
//   - Memória de usuário persistida no SQLite
//   - Modo offline completo
//   - QR code de ponto de coleta
//   - Presente de pontos entre usuários
// ============================================================

#include "chatbot_engine.hpp"
#include <format>
#include <chrono>
#include <print>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ── Helpers internos ──────────────────────────────────────────

int64_t ChatbotEngine::now_unix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ── Constructor ───────────────────────────────────────────────

ChatbotEngine::ChatbotEngine(
    GeminiClient&        gemini,
    ConversationManager& conv,
    IDatabase&           db)
    : gemini_(gemini), conv_(conv), db_(db),
      gamification_(db),
      geo_(db),
      rate_limiter_(20, 60'000)   // 20 msgs/minuto por phone_id
{}

// ── build_context: injeta memória do usuário no Gemini ────────

std::string ChatbotEngine::build_context(
    const std::string& phone_id,
    const UserProfile& user)
{
    std::string ctx;
    std::string mem = db_.get_user_memory(phone_id);
    if (!mem.empty() && mem != "{}") {
        ctx += std::format(
            "\n[Contexto do usuário {}]: {}\n"
            "Nível atual: {} | Pontos: {} | Doações: {}\n",
            user.name, mem, user.level, user.points, user.donations);
    }
    return ctx;
}

// ── update_user_memory: persiste fatos da conversa ────────────

void ChatbotEngine::update_user_memory(
    const std::string& phone_id,
    const std::string& message,
    Intent intent)
{
    // Recupera memória existente
    std::string existing = db_.get_user_memory(phone_id);
    json mem = json::object();
    if (!existing.empty() && existing != "{}") {
        try { mem = json::parse(existing); } catch (...) {}
    }

    // Atualiza com base na intenção detectada
    switch (intent) {
    case Intent::DONATE_INTENT:
        mem["last_intent"] = "donate";
        mem["interested_in_donation"] = true;
        break;
    case Intent::VOLUNTEER_INTENT:
        mem["interested_in_volunteering"] = true;
        break;
    case Intent::COLLECTION_POINT:
        mem["asked_collection_point"] = true;
        break;
    default:
        break;
    }

    mem["last_message_ts"] = now_unix();
    db_.save_user_memory(phone_id, mem.dump());
}

// ── escalate_to_human: webhook de escalonamento ───────────────

void ChatbotEngine::escalate_to_human(
    const std::string& phone_id,
    const std::string& message,
    const std::string& reason)
{
    // Log no banco
    db_.log_admin_action("system", "escalate_human",
        std::format("phone={} reason={} msg={}", phone_id, reason, message));

    // TODO: POST para webhook Slack/CRM configurável via env ESCALATION_WEBHOOK
    std::println("[Escalation] {} → humano. Razão: {}", phone_id, reason);
}

// ── handle_message: ponto de entrada principal ────────────────

EngineResult ChatbotEngine::handle_message(
    const std::string& phone_id,
    const std::string& message)
{
    EngineResult result;

    // 1. Valida formato E.164
    if (!is_valid_phone(phone_id)) {
        result.reply = "❌ Número de telefone inválido.";
        return result;
    }

    // 2. Rate limiting (Assembly hash interno)
    if (rate_limiter_.is_limited(phone_id)) {
        result.rate_limited = true;
        result.reply = "⏳ Você enviou muitas mensagens. Aguarde 1 minuto e tente novamente.";
        return result;
    }

    // 3. Garante usuário no banco
    auto user = db_.get_or_create_user(phone_id);

    // 4. Sentiment analysis
    auto sent_result = sentiment_.analyze(message);
    result.sentiment = sent_result.sentiment;

    // 5. Escalonamento por frustração
    if (sent_result.escalate) {
        result.escalated_to_human = true;
        escalate_to_human(phone_id, message, sent_result.reason);
        result.reply = IntentClassifier::offline_response(Intent::FRUSTRATION);
        return result;
    }

    // 6. Classificação de intenção local
    auto cls = classifier_.classify(message);
    result.detected_intent = cls.intent;

    // 7. Atualiza memória do usuário
    update_user_memory(phone_id, message, cls.intent);

    // 8. Se intenção conhecida → resposta instantânea (zero tokens)
    std::string offline = IntentClassifier::offline_response(cls.intent);
    if (!offline.empty() && cls.confidence >= 0.85f) {
        // Adiciona pontos pela mensagem
        uint32_t total = db_.add_points(user.id, 5u, "message");

        result.reply         = offline;
        result.points_earned = 5u;
        result.total_points  = total;
        result.level         = points_to_level(total);

        // Verifica conquistas
        user.points = total;
        check_achievements(user, result);
        return result;
    }

    // 9. Detecta "doei" na mensagem para registrar doação física
    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    if (lower.find("doei") != std::string::npos ||
        lower.find("entreguei") != std::string::npos)
    {
        Donation d;
        d.user_id       = user.id;
        d.campaign_id   = 0;
        d.type          = "physical";
        d.points_earned = 50u;
        d.registered_at = now_unix();
        db_.register_donation(d);

        uint32_t total = db_.add_points(user.id, 50u, "donation_physical");
        user.points    = total;
        user.donations++;

        // Verifica streak
        auto streak = gamification_.check_streak(user.id);
        if (streak.achievement_3)
            db_.unlock_achievement(user.id, "streak_3");

        // Verifica missões semanais
        auto missions = gamification_.update_missions(user.id);
        if (!missions.empty()) {
            result.mission_completed = true;
            result.mission_name      = missions[0].mission_desc;
            result.mission_bonus     = missions[0].bonus_points;
        }

        result.total_points  = total;
        result.points_earned = 50u;
        result.level         = points_to_level(total);
        check_achievements(user, result);
    }

    // 10. Chama Gemini com contexto enriquecido
    std::string context = build_context(phone_id, user);
    std::string enriched = context.empty()
        ? message
        : (context + "\nMensagem do usuário: " + message);

    try {
        ChatResponse ai_resp = conv_.reply(phone_id, enriched);

        uint32_t total = db_.add_points(user.id, 5u, "message");
        result.reply         = ai_resp.content;
        result.points_earned += 5u;
        result.total_points  = total;
        result.level         = points_to_level(total);

        user.points = total;
        check_achievements(user, result);

    } catch (const std::exception& e) {
        // 11. Modo offline: Gemini falhou
        std::println(stderr, "[Engine] Gemini offline: {}", e.what());
        result.reply = IntentClassifier::offline_response(Intent::HELP);
        if (result.reply.empty())
            result.reply = "🤖 Estou em manutenção no momento. Tente novamente em breve! 🙏";
    }

    return result;
}

// ── handle_location ───────────────────────────────────────────

EngineResult ChatbotEngine::handle_location(
    const std::string& phone_id,
    double lat, double lng)
{
    EngineResult result;
    auto nearest = geo_.find_nearest(lat, lng);

    if (nearest) {
        result.reply = nearest->formatted_message;
    } else {
        result.reply = "😔 Não encontrei pontos de coleta cadastrados ainda. "
                       "Fale com a equipe para cadastrar pontos na sua região!";
    }

    uint32_t total       = db_.add_points(
        db_.get_or_create_user(phone_id).id, 5u, "location_shared");
    result.total_points  = total;
    result.points_earned = 5u;
    result.level         = points_to_level(total);
    return result;
}

// ── handle_donation ───────────────────────────────────────────

EngineResult ChatbotEngine::handle_donation(
    const std::string& phone_id,
    const std::string& campaign_id)
{
    auto user = db_.get_or_create_user(phone_id);

    // Busca campanha para calcular multiplicador
    Campaign campaign;
    campaign.slug   = campaign_id;
    campaign.urgent = false;
    for (auto& c : db_.get_active_campaigns())
        if (c.slug == campaign_id) { campaign = c; break; }

    // Calcula pontos com possível 2x URGENTE
    auto pts = gamification_.calculate_points(100u, campaign);

    Donation d;
    d.user_id       = user.id;
    d.campaign_id   = campaign.id;
    d.type          = "online";
    d.points_earned = pts.final_points;
    d.registered_at = now_unix();
    db_.register_donation(d);
    db_.increment_campaign(campaign.id, 1);

    uint32_t total = db_.add_points(user.id, pts.final_points, "donation_online");
    user.points    = total;
    user.donations++;

    // Verifica missões
    auto missions = gamification_.update_missions(user.id);

    EngineResult result;
    std::string bonus_str = pts.multiplier > 1.0f
        ? std::format("\n{}", pts.bonus_reason) : "";

    result.reply = std::format(
        "💚 Doação registrada para *{}*!\n\n"
        "+{} pontos{}. Total: {} pts — Nível: {}.\n\n"
        "{}\n\n"
        "Juntos estamos transformando vidas! 🤝",
        campaign.name.empty() ? campaign_id : campaign.name,
        pts.final_points, bonus_str, total, points_to_level(total),
        campaign.progress_bar());

    result.points_earned = pts.final_points;
    result.total_points  = total;
    result.level         = points_to_level(total);

    if (!missions.empty()) {
        result.mission_completed = true;
        result.mission_name      = missions[0].mission_desc;
        result.mission_bonus     = missions[0].bonus_points;
    }

    check_achievements(user, result);
    return result;
}

// ── handle_qr_scan ────────────────────────────────────────────

EngineResult ChatbotEngine::handle_qr_scan(
    const std::string& phone_id,
    const std::string& qr_token)
{
    auto user = db_.get_or_create_user(phone_id);

    // Busca ponto de coleta pelo QR token
    CollectionPoint matched_cp;
    bool found = false;
    for (auto& cp : db_.get_collection_points("")) {
        if (cp.qr_code == qr_token) { matched_cp = cp; found = true; break; }
    }

    EngineResult result;
    if (!found) {
        result.reply = "❌ QR Code inválido ou expirado.";
        return result;
    }

    Donation d;
    d.user_id       = user.id;
    d.campaign_id   = 0;
    d.type          = "qr_scan";
    d.points_earned = 50u;
    d.registered_at = now_unix();
    db_.register_donation(d);

    uint32_t total = db_.add_points(user.id, 50u, "qr_scan");
    user.points    = total;
    user.donations++;

    result.reply = std::format(
        "✅ Doação física registrada em *{}*!\n\n"
        "+50 pontos! Total: {} pts — Nível: {}.\n\n"
        "Obrigado por aparecer pessoalmente! 💙",
        matched_cp.name, total, points_to_level(total));

    result.points_earned = 50u;
    result.total_points  = total;
    result.level         = points_to_level(total);
    check_achievements(user, result);
    return result;
}

// ── handle_volunteer ──────────────────────────────────────────

EngineResult ChatbotEngine::handle_volunteer(
    const std::string& phone_id,
    const std::string& full_name,
    const std::string& email,
    const std::string& available_days)
{
    auto user = db_.get_or_create_user(phone_id);

    VolunteerRegistration reg;
    reg.user_id       = user.id;
    reg.full_name     = full_name;
    reg.email         = email;
    reg.available_days= available_days;
    reg.registered_at = now_unix();
    db_.register_volunteer(reg);

    uint32_t total = db_.add_points(user.id, 200u, "volunteer");
    db_.unlock_achievement(user.id, "volunteer");

    EngineResult result;
    result.reply = std::format(
        "🙌 Bem-vindo à equipe, {}!\n\n"
        "Cadastro recebido. Entraremos em contato pelo e-mail {}.\n"
        "+200 pontos! Total: {} pts — Nível: {}.",
        full_name, email, total, points_to_level(total));
    result.points_earned     = 200u;
    result.total_points      = total;
    result.level             = points_to_level(total);
    result.achievement_unlocked = true;
    result.achievement_name  = "🙌 Voluntário";
    return result;
}

// ── handle_gift_points ────────────────────────────────────────

EngineResult ChatbotEngine::handle_gift_points(
    const std::string& from_phone,
    const std::string& to_phone,
    uint32_t amount)
{
    EngineResult result;
    auto gift = gamification_.gift_points(from_phone, to_phone, amount);
    result.reply = gift.message;

    if (gift.success) {
        auto user          = db_.get_or_create_user(from_phone);
        result.total_points= user.points;
        result.level       = points_to_level(user.points);
    }
    return result;
}

// ── check_achievements ────────────────────────────────────────

void ChatbotEngine::check_achievements(const UserProfile& user, EngineResult& out) {
    if (user.donations == 1)
        if (db_.unlock_achievement(user.id, "first_don")) {
            out.achievement_unlocked = true;
            out.achievement_name     = "💝 Primeiro Coração";
        }
    if (user.donations >= 5)
        db_.unlock_achievement(user.id, "don_5");
    if (user.donations >= 20)
        db_.unlock_achievement(user.id, "don_20");
    if (user.points >= 2500)
        if (db_.unlock_achievement(user.id, "god_level")) {
            out.achievement_unlocked = true;
            out.achievement_name     = "⚡ Ascensão Divina";
        }
}
