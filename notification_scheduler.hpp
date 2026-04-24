#pragma once

#include "idatabase.hpp"
#include "thread_pool.hpp"
#include <functional>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include "print_compat.hpp"
#include <format>

using SendMessageFn = std::function<void(const std::string& phone_id,
                                          const std::string& message)>;
using AdminAlertFn  = std::function<void(const std::string& subject,
                                          const std::string& body)>;

class NotificationScheduler {
public:
    NotificationScheduler(IDatabase&     db,
                           ThreadPool&    pool,
                           SendMessageFn  send_fn,
                           AdminAlertFn   alert_fn)
        : db_(db), pool_(pool),
          send_(std::move(send_fn)),
          alert_(std::move(alert_fn))
    {}

    void start() {
        running_ = true;
        pool_.submit([this] { scheduler_loop(); });
    }

    void stop() { running_ = false; }

    void on_new_campaign(const Campaign& campaign,
                          const std::vector<std::string>& all_phones)
    {
        for (const auto& phone : all_phones) {
            pool_.submit([this, phone, campaign] {
                auto user = db_.get_or_create_user(phone);
                auto donations = db_.get_user_donations(user.id, 1);

                bool should_notify = donations.empty();
                if (!donations.empty()) {
                    int64_t last_ts = donations[0].registered_at;
                    int64_t now_s   = epoch_s();
                    should_notify   = (now_s - last_ts) > (30 * 86400);
                }

                if (should_notify) {
                    std::string msg = std::format(
                        "💙 Olá, {}! Nova campanha chegou:\n\n"
                        "{} {}\n\n"
                        "Sua ajuda faz diferença! Quer participar? 🤝",
                        user.name,
                        campaign.urgent ? "🔴 URGENTE:" : "📢",
                        campaign.name);
                    send_(phone, msg);
                }
            });
        }
    }

    void check_stale_campaigns() {
        auto campaigns = db_.get_active_campaigns();
        int64_t now = epoch_s();
        for (auto& c : campaigns) {
            if (c.total_donated == 0) {
                alert_(
                    std::format("⚠️ Campanha sem doações: {}", c.name),
                    std::format("A campanha '{}' (slug: {}) não recebeu nenhuma doação.\n"
                                "Considere divulgar ou revisar a campanha.", c.name, c.slug));
            }
        }
    }

private:
    IDatabase&    db_;
    ThreadPool&   pool_;
    SendMessageFn send_;
    AdminAlertFn  alert_;
    std::atomic<bool> running_{false};

    void scheduler_loop() {
        while (running_) {
            for (int i = 0; i < 3600 && running_; ++i)
                std::this_thread::sleep_for(std::chrono::seconds(1));

            if (!running_) break;

            try {
                check_stale_campaigns();
            } catch (const std::exception& e) {
                println(stderr, "[Scheduler] Erro: {}", e.what());
            }
        }
    }

    static int64_t epoch_s() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};
