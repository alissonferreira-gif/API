#pragma once
// ============================================================
// AlissonAsk V0.7 — asm_utils.hpp
// Interface C++ para as funções Assembly em asm_utils.asm
// Compilar asm: nasm -f elf64 asm_utils.asm -o asm_utils.o
// ============================================================

#include <cstdint>
#include <cstddef>
#include <string>

extern "C" {
    // FNV-1a 64-bit — hash rápido para strings (cache keys, rate limiter)
    uint64_t asm_fnv1a_hash(const char* str, size_t len);

    // Valida formato E.164: +[1-9][0-9]{7,14}
    // Retorna 1 se válido, 0 se inválido
    int asm_validate_e164(const char* phone, size_t len);

    // Retorna epoch em milissegundos (clock_gettime syscall direto)
    int64_t asm_get_epoch_ms();

    // Conta bits 1 em uint32 (popcount SSE4.2)
    uint32_t asm_popcount32(uint32_t x);
}

// ── Wrappers C++ convenientes ─────────────────────────────────

[[nodiscard]] inline uint64_t fast_hash(const std::string& s) noexcept {
    return asm_fnv1a_hash(s.data(), s.size());
}

[[nodiscard]] inline bool is_valid_phone(const std::string& phone) noexcept {
    return asm_validate_e164(phone.data(), phone.size()) == 1;
}

[[nodiscard]] inline int64_t epoch_ms() noexcept {
    return asm_get_epoch_ms();
}
