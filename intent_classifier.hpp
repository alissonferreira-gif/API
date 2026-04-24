#pragma once

#include "asm_utils.hpp"
#include <string>
#include <array>
#include <algorithm>
#include <cctype>

enum class Intent : uint8_t {
    UNKNOWN = 0,
    DONATE_INTENT,
    VOLUNTEER_INTENT,
    COLLECTION_POINT,
    RANKING,
    CAMPAIGNS,
    RECEIPT_DONATION,
    LOCATION_REQUEST,
    MISSION,
    GROUP,
    HELP,
    FRUSTRATION,
};

struct ClassificationResult {
    Intent      intent    = Intent::UNKNOWN;
    float       confidence = 0.0f;
    std::string matched_keyword;
};

class IntentClassifier {
public:
    [[nodiscard]] ClassificationResult classify(const std::string& message) const {
        const std::string lower = to_lower(message);

        for (const auto& rule : RULES_) {
            for (const char* kw : rule.keywords) {
                if (!kw) break;
                if (lower.find(kw) != std::string::npos) {
                    return { rule.intent, rule.base_confidence, kw };
                }
            }
        }
        return { Intent::UNKNOWN, 0.0f, "" };
    }

    [[nodiscard]] static std::string offline_response(Intent intent) {
        switch (intent) {
        case Intent::DONATE_INTENT:
            return "💚 Que incrível! Para doar, escolha uma campanha:\n\n"
                   "🍽️ *Fome Zero* — alimentos não-perecíveis (URGENTE)\n"
                   "👕 *Agasalho 2025* — roupas de inverno\n"
                   "📚 *Livros que Transformam* — livros didáticos\n"
                   "💊 *Farmácia Solidária* — medicamentos (URGENTE)\n\n"
                   "Responda com o nome da campanha! 🤝";

        case Intent::COLLECTION_POINT:
            return "📍 Nossos pontos de coleta:\n\n"
                   "🏛️ Casa da Cidadania — Rua das Flores, 123 (Centro)\n"
                   "⛪ Igreja São Francisco — Av. Principal, 456 (Jardim)\n"
                   "🏥 CRAS Municipal — Rua Esperança, 789 (Periferia)\n\n"
                   "Mande sua localização 📌 para ver o mais próximo!";

        case Intent::RANKING:
            return "🏆 Seus pontos e conquistas estão sendo carregados...\n"
                   "Mensagem: +5pts | Doação física: +50pts | Online: +100pts\n\n"
                   "Continue doando para subir no ranking! ⚡";

        case Intent::CAMPAIGNS:
            return "📢 Campanhas ativas:\n\n"
                   "🍽️ Fome Zero — URGENTE! 🔴\n"
                   "💊 Farmácia Solidária — URGENTE! 🔴\n"
                   "👕 Agasalho 2025\n"
                   "📚 Livros que Transformam\n\n"
                   "Qual campanha você quer apoiar? 💙";

        case Intent::VOLUNTEER_INTENT:
            return "🙌 Quer ser voluntário? Incrível!\n\n"
                   "Por favor, me envie:\n"
                   "1️⃣ Seu nome completo\n"
                   "2️⃣ Seu e-mail\n"
                   "3️⃣ Dias disponíveis\n\n"
                   "Você ganha +200 pontos ao se cadastrar! 🌟";

        case Intent::MISSION:
            return "🎯 Missões da semana:\n\n"
                   "📦 Doe 3 vezes esta semana → +150 pts bônus\n"
                   "🌟 Indique um amigo que doe → +100 pts\n\n"
                   "As missões resetam toda segunda-feira! Bora? 💪";

        case Intent::HELP:
            return "👋 Olá! Sou o AlissonAsk, assistente solidário!\n\n"
                   "Posso te ajudar com:\n"
                   "🎁 *Doar* — campanhas ativas\n"
                   "📍 *Pontos de coleta* — onde entregar\n"
                   "🙌 *Voluntariar* — fazer parte do time\n"
                   "🏆 *Ranking* — seus pontos e conquistas\n"
                   "🎯 *Missões* — desafios semanais\n\n"
                   "O que você quer fazer? 😊";

        case Intent::FRUSTRATION:
            return "😔 Peço desculpas pela experiência ruim!\n"
                   "Vou conectar você com um atendente humano agora.\n"
                   "Aguarde um momento... 🙏";

        default:
            return "";
        }
    }

    [[nodiscard]] static bool requires_human_escalation(Intent i) {
        return i == Intent::FRUSTRATION;
    }

private:
    struct Rule {
        Intent intent;
        float  base_confidence;
        const char* keywords[13];
    };

    static constexpr std::array<Rule, 12> RULES_ = {{
        { Intent::DONATE_INTENT, 0.9f,
          {"quero doar","como doar","ajudar","fazer doação","contribuir",
           "dar uma doação","quero contribuir","doação","doe",nullptr} },

        { Intent::VOLUNTEER_INTENT, 0.9f,
          {"voluntário","voluntaria","ser voluntário","quero ajudar pessoalmente",
           "me cadastrar","fazer parte","voluntariar",nullptr} },

        { Intent::COLLECTION_POINT, 0.9f,
          {"onde fica","ponto de coleta","endereço","como entregar","onde deixar",
           "como chegar","coleta",nullptr} },

        { Intent::RANKING, 0.85f,
          {"ranking","meu nível","meus pontos","quantos pontos","nivel",
           "classificação","posição",nullptr} },

        { Intent::CAMPAIGNS, 0.85f,
          {"campanhas","o que precisa","campanha ativa","urgente","o que está",
           "lista de campanhas",nullptr} },

        { Intent::RECEIPT_DONATION, 0.95f,
          {"já doei","doei hoje","fiz a doação","acabei de doar","trouxe",
           "entreguei",nullptr} },

        { Intent::LOCATION_REQUEST, 0.9f,
          {"perto de mim","mais próximo","localização","mapa",
           "onde tem","coordenadas",nullptr} },

        { Intent::MISSION, 0.85f,
          {"missão","desafio","missões","meta semanal","missão da semana",
           "desafio semanal",nullptr} },

        { Intent::GROUP, 0.85f,
          {"grupo","time solidário","meu grupo","criar grupo","equipe",
           "grupo de doação",nullptr} },

        { Intent::HELP, 0.7f,
          {"ajuda","oi","olá","ola","menu","início","inicio","oi tudo",
           "help","bom dia","boa tarde","boa noite",nullptr} },

        { Intent::FRUSTRATION, 0.95f,
          {"não funciona","que merda","pessimo","péssimo","horrível","lixo",
           "absurdo","não adianta","inútil","problema",nullptr} },
    }};

    static std::string to_lower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return out;
    }
};
