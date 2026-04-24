#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

extern "C" {
    uint64_t asm_fnv1a_hash(const char* str, size_t len);

    int asm_validate_e164(const char* phone, size_t len);

    int64_t asm_get_epoch_ms();

    uint32_t asm_popcount32(uint32_t x);
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
