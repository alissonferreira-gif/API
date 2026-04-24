#pragma once

#include "asm_utils.hpp"
#include <array>
#include <deque>
#include <mutex>
#include <string>
#include <cstdint>
#include <chrono>

class RateLimiter {
public:
    explicit RateLimiter(uint32_t max_per_window = 20,
                         int64_t  window_ms      = 60'000)
        : max_(max_per_window), window_ms_(window_ms) {}

    [[nodiscard]] bool is_limited(const std::string& phone_id) {
        const uint64_t h      = fast_hash(phone_id);
        const size_t   bucket = static_cast<size_t>(h % kBuckets);

        std::lock_guard<std::mutex> lock(mutexes_[bucket]);
        auto& dq  = buckets_[bucket][phone_id];
        const int64_t now = epoch_ms();

        while (!dq.empty() && (now - dq.front()) > window_ms_)
            dq.pop_front();

        if (dq.size() >= max_)
            return true;   // bloqueado

        dq.push_back(now);
        return false;      // permitido
    }

    [[nodiscard]] uint32_t remaining(const std::string& phone_id) {
        const uint64_t h      = fast_hash(phone_id);
        const size_t   bucket = static_cast<size_t>(h % kBuckets);

        std::lock_guard<std::mutex> lock(mutexes_[bucket]);
        auto& dq  = buckets_[bucket][phone_id];
        const int64_t now = epoch_ms();

        while (!dq.empty() && (now - dq.front()) > window_ms_)
            dq.pop_front();

        const auto used = static_cast<uint32_t>(dq.size());
        return (used >= max_) ? 0u : (max_ - used);
    }

private:
    static constexpr size_t kBuckets = 256;

    uint32_t max_;
    int64_t  window_ms_;

    std::array<std::unordered_map<std::string, std::deque<int64_t>>, kBuckets> buckets_;
    std::array<std::mutex, kBuckets> mutexes_;
};
