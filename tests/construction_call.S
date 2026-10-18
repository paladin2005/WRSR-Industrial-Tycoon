.intel_syntax noprefix
.text
.global RunConstructionStub
RunConstructionStub:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    sub rsp, 0x60
    movdqu [rsp+0x30], xmm6
    movdqu [rsp+0x40], xmm7
    movdqu [rsp+0x50], xmm15
    mov rbx, rcx
    mov rdi, rdx
    mov rbp, r8
    mov r12, r9
    mov esi, [r12]
    pcmpeqd xmm6, xmm6
    // Register-to-register MOVSS preserves the chosen upper-lane sentinel.
    pcmpeqd xmm7, xmm7
    movd xmm6, [r12+8]
    movss xmm7, xmm6
    pcmpeqd xmm6, xmm6
    // XMM15: renovation progress the routine took from 0x1AA120 in maintenance mode ([r12+112]).
    movd xmm15, dword ptr [r12+112]
    mov rax, 0x123456789abcdef0
    xor edx, edx
    sub edx, 1
    pushfq
    pop rcx
    mov [r12+16], rcx
    // RCX is now used by the stub as a counter pointer: it must come back intact.
    mov rcx, 0x0fedcba987654321
    call rbx
    mov [r12+104], rcx
    pushfq
    pop rcx
    mov [r12+24], rcx
    mov [r12+32], rax
    mov [r12+40], rbx
    mov [r12+48], rbp
    mov [r12+56], rdi
    mov [r12+64], rsi
    movdqu [r12+72], xmm6
    movdqu [r12+88], xmm7
    movd dword ptr [r12+120], xmm15
    movdqu xmm6, [rsp+0x30]
    movdqu xmm7, [rsp+0x40]
    movdqu xmm15, [rsp+0x50]
    add rsp, 0x60
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
