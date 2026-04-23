#pragma once
// ============================================================
//  AlissonAsk V0.6 — ApiKeyManager
//  Gerencia múltiplas API keys do Gemini em rodízio.
//  Se uma key atingir rate limit, passa automaticamente
//  para a próxima.
// ============================================================

#include <string>
#include <vector>
#include <atomic>
#include <stdexcept>

class ApiKeyManager {
public:
    explicit ApiKeyManager(std::vector<std::string> keys)
        : keys_(std::move(keys)), index_(0)
    {
        if (keys_.empty())
            throw std::runtime_error("ApiKeyManager: nenhuma API key fornecida!");
    }

    // Retorna a key ativa no momento
    [[nodiscard]] std::string current() const {
        return keys_[index_.load() % keys_.size()];
    }

    // Avança para a próxima key (chame quando RateLimitException ocorrer)
    void rotate() {
        index_.fetch_add(1, std::memory_order_relaxed);
    }

    // Retorna a key atual e rotaciona se forceRotate=true
    [[nodiscard]] std::string get(bool force_rotate = false) {
        if (force_rotate) rotate();
        return current();
    }

    size_t total()   const noexcept { return keys_.size(); }
    size_t current_index() const noexcept { return index_.load() % keys_.size(); }

private:
    std::vector<std::string> keys_;
    std::atomic<size_t>      index_;
};
