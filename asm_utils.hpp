#pragma once
// baixa NASM pra rodar isso aqui e impotante baixar NASM
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>

extern "C" {
    uint64_t asm_fnv1a_hash(const char* str, size_t len);

    int asm_validate_e164(const char* phone, size_t len);

    int64_t asm_get_epoch_ms();

    uint32_t asm_popcount32(uint32_t x);

    size_t asm_training_area_store(const char* data, size_t len);
    const char* asm_training_area_data();
    size_t asm_training_area_size();
    void asm_training_area_clear();
}


[[nodiscard]] inline uint64_t fast_hash(const std::string& s) noexcept {
    return asm_fnv1a_hash(s.data(), s.size());
}

[[nodiscard]] inline bool is_valid_phone(const std::string& phone) noexcept {
    return asm_validate_e164(phone.data(), phone.size()) == 1;
}

[[nodiscard]] inline int64_t epoch_ms() noexcept {
    return asm_get_epoch_ms();
}

[[nodiscard]] inline size_t training_data_append(std::string_view data) noexcept {
    return asm_training_area_store(data.data(), data.size());
}

[[nodiscard]] inline std::string_view training_data_view() noexcept {
    return std::string_view(asm_training_area_data(), asm_training_area_size());
}

inline void training_data_clear() noexcept {
    asm_training_area_clear();
}
