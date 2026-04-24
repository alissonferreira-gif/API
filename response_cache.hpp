#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <cctype>

struct CachedEntry {
    std::string  response;
    std::chrono::steady_clock::time_point cached_at;
};

class ResponseCache {
public:
    explicit ResponseCache(int ttl_min = 60) : ttl_min_(ttl_min) {}

    static std::string normalize(const std::string& msg) {
        std::string out;
        out.reserve(msg.size());
        bool last_space = false;
        for (char c : msg) {
            char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!last_space && !out.empty()) out += ' ';
                last_space = true;
            } else {
                out += lc;
                last_space = false;
            }
        }
        return out;
    }

    std::string get(const std::string& message) {
        prune();
        auto key = normalize(message);
        auto it  = cache_.find(key);
        if (it == cache_.end()) return {};

        auto age = std::chrono::steady_clock::now() - it->second.cached_at;
        if (age > std::chrono::minutes(ttl_min_)) {
            cache_.erase(it);
            return {};
        }
        return it->second.response;
    }

    void set(const std::string& message, const std::string& response) {
        auto key = normalize(message);
        cache_[key] = { response, std::chrono::steady_clock::now() };
    }

    size_t size()  const noexcept { return cache_.size(); }
    void   clear() noexcept       { cache_.clear(); }

private:
    int ttl_min_;
    std::unordered_map<std::string, CachedEntry> cache_;

    void prune() {
        const auto now = std::chrono::steady_clock::now();
        std::erase_if(cache_, [&](const auto& kv) {
            return (now - kv.second.cached_at) > std::chrono::minutes(ttl_min_);
        });
    }
};
