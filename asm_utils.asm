; ============================================================
; AlissonAsk V0.7 — asm_utils.asm
; Utilitários críticos em Assembly x86-64 (Linux, NASM)
;
; Funções exportadas (extern "C"):
;   uint64_t asm_fnv1a_hash(const char* str, size_t len)
;   int      asm_validate_e164(const char* phone, size_t len)
;   int64_t  asm_get_epoch_ms()
;   uint32_t asm_popcount32(uint32_t x)
; ============================================================

section .text
global asm_fnv1a_hash
global asm_validate_e164
global asm_get_epoch_ms
global asm_popcount32

; ── FNV-1a 64-bit hash ────────────────────────────────────────
; Usado pelo RateLimiter e ResponseCache como hash de chave
; rdi = const char* str
; rsi = size_t len
; retorna rax = hash uint64_t
asm_fnv1a_hash:
    mov     rax, 0xcbf29ce484222325   ; FNV offset basis
    test    rsi, rsi
    jz      .done
    mov     rcx, rsi
    xor     rdx, rdx
.loop:
    movzx   r8d, byte [rdi]
    xor     rax, r8                   ; XOR com byte atual
    imul    rax, 0x100000001b3         ; FNV prime
    inc     rdi
    dec     rcx
    jnz     .loop
.done:
    ret

; ── Validação E.164 ───────────────────────────────────────────
; Valida formato internacional: +[1-9][0-9]{7,14}  (total 8-15 chars)
; rdi = const char* phone_str
; rsi = size_t len
; retorna rax = 1 se válido, 0 se inválido
asm_validate_e164:
    ; comprimento mínimo 8 (+1234567), máximo 16 (+123456789012345)
    cmp     rsi, 8
    jl      .invalid
    cmp     rsi, 16
    jg      .invalid
    ; primeiro char deve ser '+'
    movzx   eax, byte [rdi]
    cmp     al, '+'
    jne     .invalid
    ; segundo char: 1-9 (não pode ser 0)
    movzx   eax, byte [rdi + 1]
    cmp     al, '1'
    jl      .invalid
    cmp     al, '9'
    jg      .invalid
    ; demais chars: todos 0-9
    mov     rcx, rsi
    sub     rcx, 2                    ; já validamos pos 0 e 1
    lea     rdi, [rdi + 2]
.digit_loop:
    test    rcx, rcx
    jz      .valid
    movzx   eax, byte [rdi]
    cmp     al, '0'
    jl      .invalid
    cmp     al, '9'
    jg      .invalid
    inc     rdi
    dec     rcx
    jmp     .digit_loop
.valid:
    mov     rax, 1
    ret
.invalid:
    xor     rax, rax
    ret

; ── Timestamp em milissegundos (syscall clock_gettime) ────────
; Usa CLOCK_REALTIME (id=0) direto via syscall
; Retorna rax = epoch ms como int64_t
asm_get_epoch_ms:
    sub     rsp, 16                   ; aloca timespec na stack (tv_sec=8, tv_nsec=8)
    xor     edi, edi                  ; CLOCK_REALTIME = 0
    mov     rsi, rsp                  ; &timespec
    mov     eax, 228                  ; syscall clock_gettime (Linux x86-64)
    syscall
    ; rax retorna 0 se ok
    mov     rax, [rsp]                ; tv_sec
    imul    rax, 1000                 ; → ms
    mov     rcx, [rsp + 8]            ; tv_nsec
    xor     rdx, rdx
    mov     r8, 1000000
    div     r8                        ; tv_nsec / 1_000_000 → ms remainder
    add     rax, rcx                  ; total ms (após div rcx tem quociente)
    ; corrige: div retorna quociente em rax, mas sobrescrevemos — refaz
    ; método direto sem div:
    mov     rax, [rsp]
    imul    rax, 1000
    mov     rcx, [rsp + 8]
    mov     r8, 1000000
    xor     rdx, rdx
    mov     r9, rcx
    xor     rdx, rdx
    mov     rax, r9
    div     r8                        ; rax = tv_nsec / 1_000_000
    mov     r10, rax
    mov     rax, [rsp]
    imul    rax, 1000
    add     rax, r10
    add     rsp, 16
    ret

; ── Popcount 32-bit (conta bits 1) ───────────────────────────
; Usado em bitmask de dias ativos de missões semanais
; edi = uint32_t x
; retorna eax = número de bits 1
asm_popcount32:
    popcnt  eax, edi                  ; instrução nativa SSE4.2
    ret
