
section .text
global asm_fnv1a_hash
global asm_validate_e164
global asm_get_epoch_ms
global asm_popcount32

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

asm_validate_e164:
    cmp     rsi, 8
    jl      .invalid
    cmp     rsi, 16
    jg      .invalid
    movzx   eax, byte [rdi]
    cmp     al, '+'
    jne     .invalid
    movzx   eax, byte [rdi + 1]
    cmp     al, '1'
    jl      .invalid
    cmp     al, '9'
    jg      .invalid
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

asm_get_epoch_ms:
    sub     rsp, 16                   ; aloca timespec na stack (tv_sec=8, tv_nsec=8)
    xor     edi, edi                  ; CLOCK_REALTIME = 0
    mov     rsi, rsp                  ; &timespec
    mov     eax, 228                  ; syscall clock_gettime (Linux x86-64)
    syscall
    mov     rax, [rsp]                ; tv_sec
    imul    rax, 1000                 ; → ms
    mov     rcx, [rsp + 8]            ; tv_nsec
    xor     rdx, rdx
    mov     r8, 1000000
    div     r8                        ; tv_nsec / 1_000_000 → ms remainder
    add     rax, rcx                  ; total ms (após div rcx tem quociente)
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

asm_popcount32:
    popcnt  eax, edi                  ; instrução nativa SSE4.2
    ret
