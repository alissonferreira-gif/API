// ============================================================
//  AlissonAsk V0.6 — Demo Standalone
//  Criado e Integrado por: Álisson Ferreira Dos Santos
//
//  Testa o chatbot direto no terminal sem precisar do
//  servidor WhatsApp. Simula conversas de múltiplos usuários.
//
//  Compilar (Arch Linux / GCC):
//    g++ -std=c++23 demo.cpp src/gemini_client.cpp \
//        src/conversation_manager.cpp src/chatbot_engine.cpp \
//        -Iinclude -lcurl -o demo
//
//  Rodar:
//    export GEMINI_KEY_1=sua_key
//    ./demo
// ============================================================

#include "gemini_client.hpp"
#include "conversation_manager.hpp"
#include "chatbot_engine.hpp"
#include "idatabase.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <print>
#include <format>
#include <unordered_map>

// ── Banco mock inline ─────────────────────────────────────────

class DemoDatabase : public IDatabase {
public:
    UserProfile get_or_create_user(const std::string& phone_id) override {
        auto& u = users_[phone_id];
        if (u.phone_id.empty()) {
            u.id       = static_cast<int32_t>(users_.size());
            u.phone_id = phone_id;
            u.name     = "Demo_" + phone_id.substr(phone_id.size() - 4);
            u.level    = "🤝 Iniciante";
            u.points   = 0;
            u.donations = 0;
        }
        return u;
    }

    uint32_t add_points(int32_t /*user_id*/, uint32_t amount, const std::string& reason) override {
        total_ += amount;
        std::println("  [+{} pts | motivo: {}]", amount, reason);
        return total_;
    }

    std::vector<Campaign>        get_active_campaigns()                           override { return {}; }
    void                         increment_campaign(int32_t, int_fast32_t)        override {}
    std::vector<CollectionPoint> get_collection_points(const std::string&)        override { return {}; }
    int64_t                      register_donation(const Donation&)               override { return 0; }
    std::vector<Donation>        get_user_donations(int32_t, int_fast32_t)        override { return {}; }
    std::vector<RankingEntry>    get_ranking(int_fast32_t)                        override { return {}; }
    int32_t                      get_user_rank(int32_t)                           override { return 0; }
    std::vector<Achievement>     get_user_achievements(int32_t)                   override { return {}; }
    bool                         unlock_achievement(int32_t, const std::string&)  override { return false; }
    int32_t                      register_volunteer(const VolunteerRegistration&) override { return 0; }

private:
    std::unordered_map<std::string, UserProfile> users_;
    uint32_t total_ = 0;
};

// ── System prompt ─────────────────────────────────────────────

constexpr std::string_view SYSTEM_PROMPT = R"(
Você é AlissonAsk V0.6, assistente oficial do projeto Solidário.
Criado e integrado por Álisson Ferreira Dos Santos.

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

// ── Carrega keys do ambiente ──────────────────────────────────

static std::vector<std::string> load_keys() {
    std::vector<std::string> keys;
    for (int i = 1; i <= 10; ++i) {
        std::string name = std::format("GEMINI_KEY_{}", i);
        const char* v = std::getenv(name.c_str());
        if (v && *v) keys.emplace_back(v);
    }
    // Fallback legado
    if (keys.empty()) {
        const char* v = std::getenv("GEMINI_API_KEY");
        if (v && *v) keys.emplace_back(v);
    }
    return keys;
}

// ── Exibe banner ──────────────────────────────────────────────

static void banner(size_t num_keys) {
    std::println("╔══════════════════════════════════════════════╗");
    std::println("║     AlissonAsk V0.6 — Demo Terminal          ║");
    std::println("║     Criado por: Álisson Ferreira Dos Santos  ║");
    std::println("╠══════════════════════════════════════════════╣");
    std::println("║  🔑 {} API key(s) carregada(s)               ║", num_keys);
    std::println("║  💾 Cache ativo (60 min TTL)                 ║");
    std::println("║  📊 Capacidade: {} req/dia               ║", num_keys * 1500);
    std::println("╠══════════════════════════════════════════════╣");
    std::println("║  Formato:  <numero> <mensagem>               ║");
    std::println("║  Exemplo:  5511999998888 Quero doar roupas   ║");
    std::println("║  Sair:     digite 'sair'                     ║");
    std::println("╚══════════════════════════════════════════════╝\n");
}

// ── main ──────────────────────────────────────────────────────

int main() {
    try {
        auto keys = load_keys();
        if (keys.empty()) {
            std::println(stderr,
                "[ERRO] Nenhuma API key encontrada!\n"
                "Configure: export GEMINI_KEY_1=sua_key");
            return EXIT_FAILURE;
        }

        banner(keys.size());

        // Configura Gemini
        GeminiClient::Config gcfg;
        gcfg.model         = "gemini-2.0-flash";
        gcfg.temperature   = 0.72;
        gcfg.max_tokens    = 512;
        gcfg.timeout_sec   = 20;
        gcfg.system_prompt = std::string(SYSTEM_PROMPT);
        gcfg.cache_ttl_min = 60;

        GeminiClient gemini(std::move(keys), gcfg);

        // Configura ConversationManager
        ConversationManager::Config ccfg;
        ccfg.max_history = 20;
        ccfg.timeout_min = 30;
        ConversationManager conv(gemini, ccfg);

        // Banco demo
        DemoDatabase db;

        // Engine
        ChatbotEngine engine(gemini, conv, db);

        // ── Loop principal ────────────────────────────────────
        std::string line;
        while (true) {
            std::print(">>> ");
            std::cout.flush();

            if (!std::getline(std::cin, line)) break;
            if (line == "sair" || line == "exit" || line == "q") break;
            if (line.empty()) continue;

            // Comandos especiais
            if (line == "cache") {
                std::println("[Cache] {} entradas armazenadas.", gemini.cache_size());
                continue;
            }
            if (line == "key") {
                std::println("[ApiKeyManager] Key ativa: índice {}", gemini.active_key_index());
                continue;
            }
            if (line == "ajuda") {
                std::println("Comandos: cache | key | sair");
                std::println("Mensagem: <numero> <texto>");
                continue;
            }

            // Formato: <numero> <mensagem>
            auto sep = line.find(' ');
            if (sep == std::string::npos) {
                std::println("[!] Formato: <numero> <mensagem>  |  ou: ajuda");
                continue;
            }

            std::string phone   = line.substr(0, sep);
            std::string message = line.substr(sep + 1);

            try {
                std::print("[{}] ⏳ Aguarde...\r", phone);
                std::cout.flush();

                auto result = engine.handle_message(phone, message);

                std::println("\n┌─ AlissonAsk → {} ─────────────────────", phone);
                std::println("│ {}", result.reply);
                std::println("├───────────────────────────────────────────");
                std::println("│ 📊 Pontos: {} | Nível: {}", result.total_points, result.level);

                if (result.achievement_unlocked)
                    std::println("│ 🏆 CONQUISTA: {}", result.achievement_name);

                std::println("└───────────────────────────────────────────\n");

            } catch (const RateLimitException&) {
                std::println("[!] Todas as keys em rate limit. Aguarde um momento.\n");
            } catch (const GeminiException& e) {
                std::println("[ERRO Gemini] {} (HTTP {})\n", e.what(), e.http_code());
            } catch (const std::exception& e) {
                std::println("[ERRO] {}\n", e.what());
            }
        }

        std::println("\nAlissonAsk encerrado. Até logo! 👋");

    } catch (const std::exception& e) {
        std::println(stderr, "[FATAL] {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
