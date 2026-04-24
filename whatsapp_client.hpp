#pragma once

#include <nlohmann/json.hpp>
#include <string>

class WhatsAppClient {
public:
    struct Config {
        std::string phone_number_id;
        std::string access_token;
        std::string api_version = "v20.0";
    };

    struct WebhookMessage {
        bool        valid = false;
        std::string from;
        std::string text;
    };

    explicit WhatsAppClient(Config cfg) : cfg_(std::move(cfg)) {}

    void send_text(const std::string&, const std::string&) const {
        // Placeholder para manter build funcional sem integração externa neste repositório.
    }

    [[nodiscard]] static WebhookMessage parse_webhook(const std::string& body) {
        using json = nlohmann::json;
        WebhookMessage out;

        try {
            const auto j = json::parse(body);
            const auto& value = j.at("entry").at(0).at("changes").at(0).at("value");
            const auto& msg = value.at("messages").at(0);

            out.from = msg.value("from", "");
            if (msg.contains("text"))
                out.text = msg["text"].value("body", "");
            out.valid = !out.from.empty();
        } catch (...) {
            out.valid = false;
        }
        return out;
    }

private:
    Config cfg_;
};
