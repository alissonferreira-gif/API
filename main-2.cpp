// ============================================================
//  AlissonAsk V0.6 — Entry Point
//  Criado e Integrado por: Álisson Ferreira Dos Santos
//
//  Novidades V0.6:
//  - 10 API keys em rodízio (15.000 req/dia grátis)
//  - Cache de respostas (economiza tokens em perguntas repetidas)
//  - idatabase.hpp incluso
//
//  Uso:
//    AlissonAsk          → modo demo (terminal)
//    AlissonAsk --server → servidor HTTP na porta 8080
// ============================================================

#include "gemini_client.hpp"
#include "conversation_manager.hpp"
#include "chatbot_engine.hpp"
#include "whatsapp_client.hpp"
#include "idatabase.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <cstdlib>
#include <print>
#include <format>

using json = nlohmann::json;

// ── Lê variáveis de ambiente ──────────────────────────────────

[[nodiscard]] static std::string require_env(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v)
        throw std::runtime_error(std::format(
            "Variável de ambiente {} não definida.\n"
            "Execute: export {}=seu_valor  (Linux/Mac)\n"
            "         set {}=seu_valor     (Windows)",
            name, name, name));
    return std::string(v);
}

// Lê env opcional (retorna fallback se não definida)
[[nodiscard]] static std::string optional_env(const char* name, const std::string& fallback = "") {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : fallback;
}

// ── Banco de dados mock ───────────────────────────────────────

class MockDatabase : public IDatabase {
public:
    UserProfile get_or_create_user(const std::string& phone_id) override {
        auto& u = users_[phone_id];
        if (u.phone_id.empty()) {
            u.id       = static_cast<int32_t>(users_.size());
            u.phone_id = phone_id;
            u.name     = "Usuário " + phone_id.substr(phone_id.size() - 4);
            u.level    = "🤝 Iniciante";
        }
        return u;
    }

    uint32_t add_points(int32_t /*user_id*/, uint32_t amount, const std::string& /*reason*/) override {
        total_points_ += amount;
        return total_points_;
    }

    std::vector<Campaign>        get_active_campaigns()                          override { return {}; }
    void                         increment_campaign(int32_t, int_fast32_t)       override {}
    std::vector<CollectionPoint> get_collection_points(const std::string&)       override { return {}; }
    int64_t                      register_donation(const Donation&)              override { return 0; }
    std::vector<Donation>        get_user_donations(int32_t, int_fast32_t)       override { return {}; }
    std::vector<RankingEntry>    get_ranking(int_fast32_t)                       override { return {}; }
    int32_t                      get_user_rank(int32_t)                          override { return 0; }
    std::vector<Achievement>     get_user_achievements(int32_t)                  override { return {}; }
    bool                         unlock_achievement(int32_t, const std::string&) override { return false; }
    int32_t                      register_volunteer(const VolunteerRegistration&)override { return 0; }

private:
    std::unordered_map<std::string, UserProfile> users_;
    uint32_t total_points_ = 0;
};

// ── System prompt ─────────────────────────────────────────────

constexpr std::string_view SYSTEM_PROMPT = R"(
Você é AlissonAsk V0.6, assistente oficial do projeto Solidário.
Criado e integrado por Álisson Ferreira Dos Santos.

Se perguntarem quem você é: "Sou AlissonAsk V0.6, criado e integrado por Álisson Ferreira Dos Santos."
Se perguntarem quem te criou: "Álisson Ferreira Dos Santos me criou e me integrou."

Personalidade: empático, acolhedor, motivador. Respostas curtas (máx 3 parágrafos). Use emojis com moderação.

Campanhas ativas:
  🍽️ Fome Zero – alimentos não-perecíveis (URGENTE)
  👕 Agasalho 2025 – roupas de inverno
  📚 Livros que Transformam – livros didáticos
  💊 Farmácia Solidária – medicamentos (URGENTE)

Pontos de coleta:
  📍 Casa da Cidadania – Rua das Flores, 123 – Centro
  ⛪ Igreja São Francisco – Av. Principal, 456 – Jardim
  🏥 CRAS Municipal – Rua Esperança, 789 – Periferia

Pontos: mensagem=+5, doação física=+50, online=+100, voluntariado=+200
Nível supremo: ⚡ Deus (2500 pts). Finalize com pergunta ou call-to-action.
)";

// ── Modo demo (terminal) ──────────────────────────────────────

static void run_demo(ChatbotEngine& engine) {
    std::println("╔══════════════════════════════════════════╗");
    std::println("║   AlissonAsk V0.6 — Modo Demo Terminal   ║");
    std::println("║   Criado por: Álisson Ferreira Dos Santos║");
    std::println("║   Cache ativo | 10 API keys em rodízio   ║");
    std::println("╚══════════════════════════════════════════╝");
    std::println("Formato: <numero> <mensagem>");
    std::println("Ex:      5511999998888 Quero fazer uma doação");
    std::println("Digite 'sair' para encerrar.\n");

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "sair") break;

        auto sep = line.find(' ');
        if (sep == std::string::npos) {
            std::println("[!] Formato: <numero> <mensagem>");
            continue;
        }

        std::string phone   = line.substr(0, sep);
        std::string message = line.substr(sep + 1);

        try {
            std::print("[{}] Processando... ", phone);
            std::cout.flush();

            auto result = engine.handle_message(phone, message);

            std::println("\n[AlissonAsk → {}]:\n{}", phone, result.reply);
            std::println("[Pontos: {} | Nível: {}]\n", result.total_points, result.level);

            if (result.achievement_unlocked)
                std::println("🏆 CONQUISTA DESBLOQUEADA: {}\n", result.achievement_name);

        } catch (const RateLimitException&) {
            std::println("\n[!] Todas as keys estão em rate limit. Aguarde.");
        } catch (const GeminiException& e) {
            std::println("\n[ERRO] Gemini: {} (HTTP {})", e.what(), e.http_code());
        } catch (const std::exception& e) {
            std::println("\n[ERRO] {}", e.what());
        }
    }
}

// ── Modo servidor (WhatsApp webhook) ─────────────────────────

static void run_server(ChatbotEngine& engine, WhatsAppClient& wa,
                       const std::string& verify_token) {
    httplib::Server svr;

    svr.Get("/webhook", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.get_param_value("hub.verify_token") == verify_token)
            res.set_content(req.get_param_value("hub.challenge"), "text/plain");
        else
            res.status = 403;
    });

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

    std::println("╔══════════════════════════════════════════╗");
    std::println("║   AlissonAsk V0.6 — Servidor WhatsApp    ║");
    std::println("║   Porta: 8080 | /webhook                 ║");
    std::println("║   Cache ativo | 10 API keys em rodízio   ║");
    std::println("╚══════════════════════════════════════════╝");
    svr.listen("0.0.0.0", 8080);
}

// ── main ──────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    try {
        // ── 10 API keys em rodízio (15.000 req/dia grátis) ──
        // Configure as variáveis de ambiente antes de rodar:
        //   export GEMINI_KEY_1=sua_key_1
        //   export GEMINI_KEY_2=sua_key_2 ... até GEMINI_KEY_10
        //
        // Keys não definidas são ignoradas automaticamente.
        std::vector<std::string> keys;
        for (int i = 1; i <= 10; ++i) {
            std::string env_name = std::format("GEMINI_KEY_{}", i);
            std::string key = optional_env(env_name.c_str());
            if (!key.empty()) keys.push_back(key);
        }

        // Fallback: usa GEMINI_API_KEY legada se nenhuma key numerada encontrada
        if (keys.empty()) {
            keys.push_back(require_env("GEMINI_API_KEY"));
            std::println("[Aviso] Usando GEMINI_API_KEY legada. Configure GEMINI_KEY_1..10 para rodízio.");
        } else {
            std::println("[ApiKeyManager] {} keys carregadas. Capacidade: {} req/dia.",
                         keys.size(), keys.size() * 1500);
        }

        // ── Configura Gemini com cache ──
        GeminiClient::Config gcfg;
        gcfg.model         = "gemini-2.0-flash";
        gcfg.temperature   = 0.72;
        gcfg.max_tokens    = 512;
        gcfg.timeout_sec   = 20;
        gcfg.system_prompt = std::string(SYSTEM_PROMPT);
        gcfg.cache_ttl_min = 60;   // respostas ficam no cache por 1 hora

        GeminiClient gemini(std::move(keys), gcfg);

        // ── ConversationManager ──
        ConversationManager::Config ccfg;
        ccfg.max_history = 20;
        ccfg.timeout_min = 30;

        ConversationManager conv(gemini, ccfg);

        // ── Banco mock (equipe substitui por implementação real) ──
        MockDatabase db;

        // ── Engine principal ──
        ChatbotEngine engine(gemini, conv, db);

        // ── Modo servidor ou demo ──
        bool server_mode = (argc > 1 && std::string(argv[1]) == "--server");

        if (server_mode) {
            const std::string wa_phone  = require_env("WA_PHONE_NUMBER_ID");
            const std::string wa_token  = require_env("WA_ACCESS_TOKEN");
            const std::string wa_verify = require_env("WA_VERIFY_TOKEN");

            WhatsAppClient::Config wcfg { wa_phone, wa_token };
            WhatsAppClient wa(wcfg);
            run_server(engine, wa, wa_verify);
        } else {
            run_demo(engine);
        }

    } catch (const std::exception& e) {
        std::println(stderr, "[FATAL] {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
