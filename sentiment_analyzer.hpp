#pragma once
// ============================================================
// AlissonAsk V0.7 — sentiment_analyzer.hpp
// Análise de sentimento local: detecta frustração/urgência
// e decide se deve escalar para atendente humano.
// ============================================================

#include <string>
#include <array>
#include <algorithm>
#include <cctype>

enum class Sentiment : uint8_t {
    POSITIVE,
    NEUTRAL,
    NEGATIVE,
    FRUSTRATED,   // escalar para humano
};

struct SentimentResult {
    Sentiment   sentiment    = Sentiment::NEUTRAL;
    float       score        = 0.0f;   // -1.0 (muito negativo) a +1.0 (muito positivo)
    bool        escalate     = false;  // deve chamar atendente humano?
    std::string reason;                // palavra que disparou
};

class SentimentAnalyzer {
public:
    [[nodiscard]] SentimentResult analyze(const std::string& message) const {
        const std::string lower = to_lower(message);
        float score = 0.0f;
        std::string trigger;

        for (const auto& w : POSITIVE_WORDS_)
            if (lower.find(w) != std::string::npos) { score += 0.3f; trigger = w; }

        for (const auto& w : NEGATIVE_WORDS_)
            if (lower.find(w) != std::string::npos) { score -= 0.4f; trigger = w; }

        for (const auto& w : FRUSTRATION_WORDS_)
            if (lower.find(w) != std::string::npos) { score -= 0.8f; trigger = w; }

        // Clamp
        if (score >  1.0f) score =  1.0f;
        if (score < -1.0f) score = -1.0f;

        SentimentResult r;
        r.score  = score;
        r.reason = trigger;

        if (score >= 0.2f)       r.sentiment = Sentiment::POSITIVE;
        else if (score <= -0.6f) { r.sentiment = Sentiment::FRUSTRATED; r.escalate = true; }
        else if (score <= -0.2f) r.sentiment = Sentiment::NEGATIVE;
        else                     r.sentiment = Sentiment::NEUTRAL;

        return r;
    }

private:
    static constexpr std::array<const char*, 12> POSITIVE_WORDS_ = {{
        "obrigado","grato","incrível","maravilhoso","parabéns",
        "ótimo","excelente","muito bom","adorei","amei","perfeito","top"
    }};

    static constexpr std::array<const char*, 10> NEGATIVE_WORDS_ = {{
        "ruim","errado","problema","não entendi","confuso",
        "demora","demorou","erro","falhou","não consigo"
    }};

    static constexpr std::array<const char*, 10> FRUSTRATION_WORDS_ = {{
        "que merda","péssimo","horrível","inútil","lixo",
        "absurdo","não funciona","ridículo","incompetente","me ignora"
    }};

    static std::string to_lower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return out;
    }
};
