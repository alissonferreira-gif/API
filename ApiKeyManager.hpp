#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <stdexcept>

class ApiKeyManager {
public:
    ApiKeyManager(std::vector<std::string> keys)
        : m_keys(std::move(keys)), m_currentIndex(0)
    {
        if (m_keys.empty())
            throw std::runtime_error("Nenhuma API key fornecida!");
    }

    // Retorna a key atual
    std::string getCurrentKey() const {
        return m_keys[m_currentIndex % m_keys.size()];
    }

    // Vai para a próxima key (chame quando a atual falhar ou atingir limite)
    void nextKey() {
        m_currentIndex = (m_currentIndex + 1) % m_keys.size();
    }

    // Tenta a requisição com rodízio automático em caso de falha
    // Retorna o índice da key que funcionou, ou lança exceção se todas falharem
    std::string getKeyWithFallback(bool forceNext = false) {
        if (forceNext) nextKey();

        size_t attempts = 0;
        while (attempts < m_keys.size()) {
            std::string key = getCurrentKey();
            if (!key.empty()) return key;
            nextKey();
            attempts++;
        }
        throw std::runtime_error("Todas as API keys estão indisponíveis!");
    }

    size_t totalKeys() const { return m_keys.size(); }
    size_t currentIndex() const { return m_currentIndex; }

private:
    std::vector<std::string> m_keys;
    std::atomic<size_t> m_currentIndex;
};
