#pragma once

#include "idatabase.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "print_compat.hpp"
#include <format>
#include <string>
#include <chrono>

using json = nlohmann::json;

class AdminRouter {
public:
    explicit AdminRouter(IDatabase& db, std::string admin_token)
        : db_(db), token_(std::move(admin_token)) {}

    void register_routes(httplib::Server& svr) {
        svr.Get("/admin/relatorio", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) return;

            auto campanhas  = db_.get_active_campaigns();
            auto ranking    = db_.get_ranking(10);

            uint64_t total_doacoes = 0;
            uint64_t total_online  = 0;
            uint64_t total_fisico  = 0;

            for (const auto& r : ranking)
                total_doacoes += r.points;

            json resp = {
                { "versao",          "AlissonAsk V0.6" },
                { "total_pontos",    total_doacoes },
                { "campanhas_ativas", static_cast<int>(campanhas.size()) },
                { "top_doadores",    json::array() },
                { "gerado_em",       timestamp() }
            };

            for (const auto& r : ranking)
                resp["top_doadores"].push_back({
                    { "nome",   r.name   },
                    { "pontos", r.points },
                    { "rank",   r.rank   }
                });

            send_json(res, resp);
            println("[Admin] GET /relatorio");
        });

        svr.Get("/admin/campanhas", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) return;

            auto campanhas = db_.get_active_campaigns();
            json arr = json::array();
            for (const auto& c : campanhas)
                arr.push_back({
                    { "id",        c.id          },
                    { "slug",      c.slug        },
                    { "nome",      c.name        },
                    { "descricao", c.description },
                    { "urgente",   c.urgent      }
                });

            send_json(res, { { "campanhas", arr } });
            println("[Admin] GET /campanhas");
        });

        svr.Post("/admin/campanhas", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) return;

            auto body = parse_body(req, res);
            if (!body) return;

            if (!body->contains("nome") || !body->contains("slug")) {
                send_error(res, 400, "Campos obrigatórios: nome, slug");
                return;
            }

            json resp = {
                { "sucesso",  true },
                { "mensagem", std::format("Campanha '{}' criada com sucesso!",
                              (*body)["nome"].get<std::string>()) },
                { "criado_em", timestamp() }
            };

            send_json(res, resp, 201);
            println("[Admin] POST /campanhas → {}",
                         (*body)["nome"].get<std::string>());
        });

        svr.Post("/admin/doacao", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) return;

            auto body = parse_body(req, res);
            if (!body) return;

            if (!body->contains("phone_id") || !body->contains("tipo") || !body->contains("valor_brl")) {
                send_error(res, 400, "Campos obrigatórios: phone_id, tipo, valor_brl");
                return;
            }

            std::string phone    = (*body)["phone_id"].get<std::string>();
            std::string tipo     = (*body)["tipo"].get<std::string>();
            double      valor    = (*body)["valor_brl"].get<double>();
            int32_t     campanha = body->value("campanha_id", 0);

            auto user = db_.get_or_create_user(phone);

            Donation d;
            d.user_id       = user.id;
            d.campaign_id   = campanha;
            d.type          = tipo;
            d.points_earned = (tipo == "online") ? 100u : 50u;
            d.registered_at = now_unix();

            int64_t donation_id = db_.register_donation(d);
            uint32_t total_pts  = db_.add_points(user.id, d.points_earned, "admin_donation");

            json resp = {
                { "sucesso",      true },
                { "doacao_id",    donation_id },
                { "phone_id",     phone },
                { "tipo",         tipo },
                { "valor_brl",    valor },
                { "pontos_ganhos", d.points_earned },
                { "total_pontos", total_pts },
                { "nivel",        points_to_level(total_pts) },
                { "registrado_em", timestamp() }
            };

            send_json(res, resp, 201);
            println("[Admin] POST /doacao → {} | R$ {:.2f} | {} pts",
                         phone, valor, d.points_earned);
        });

        svr.Get("/admin/ranking", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) return;

            int limit = 20;
            if (req.has_param("limit"))
                limit = std::stoi(req.get_param_value("limit"));

            auto ranking = db_.get_ranking(limit);
            json arr = json::array();
            for (const auto& r : ranking)
                arr.push_back({
                    { "rank",   r.rank   },
                    { "nome",   r.name   },
                    { "pontos", r.points },
                    { "nivel",  points_to_level(r.points) }
                });

            send_json(res, { { "ranking", arr }, { "total", static_cast<int>(arr.size()) } });
            println("[Admin] GET /ranking (top {})", limit);
        });

        svr.Get("/admin/coletas", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) return;

            std::string bairro = req.has_param("bairro") ? req.get_param_value("bairro") : "";
            auto coletas = db_.get_collection_points(bairro);

            json arr = json::array();
            for (const auto& c : coletas)
                arr.push_back({
                    { "id",       c.id           },
                    { "nome",     c.name         },
                    { "endereco", c.address      },
                    { "bairro",   c.neighborhood }
                });

            send_json(res, { { "pontos_coleta", arr } });
            println("[Admin] GET /coletas");
        });

        svr.Get("/admin/health", [](const httplib::Request&, httplib::Response& res) {
            send_json(res, {
                { "status",  "ok" },
                { "versao",  "AlissonAsk V0.6" },
                { "servico", "admin-api" }
            });
        });

        println("[AdminRouter] Rotas registradas:");
        println("  GET  /admin/health");
        println("  GET  /admin/relatorio");
        println("  GET  /admin/campanhas");
        println("  POST /admin/campanhas");
        println("  POST /admin/doacao");
        println("  GET  /admin/ranking");
        println("  GET  /admin/coletas");
    }

private:
    IDatabase&  db_;
    std::string token_;

    bool authorized(const httplib::Request& req, httplib::Response& res) {
        auto auth = req.get_header_value("Authorization");
        if (auth == std::format("Bearer {}", token_)) return true;
        send_error(res, 401, "Token inválido ou ausente.");
        println("[Admin] ⚠️  Acesso não autorizado de {}", req.remote_addr);
        return false;
    }

    static void send_json(httplib::Response& res, const json& j, int status = 200) {
        res.status = status;
        res.set_content(j.dump(2), "application/json");
    }

    static void send_error(httplib::Response& res, int status, const std::string& msg) {
        res.status = status;
        res.set_content(
            json({ { "erro", msg }, { "status", status } }).dump(2),
            "application/json");
    }

    static std::optional<json> parse_body(const httplib::Request& req, httplib::Response& res) {
        try {
            return json::parse(req.body);
        } catch (...) {
            send_error(res, 400, "JSON inválido no corpo da requisição.");
            return std::nullopt;
        }
    }

    static std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
        return buf;
    }

    static int64_t now_unix() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};
