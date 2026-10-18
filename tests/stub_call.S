.intel_syntax noprefix
.text
.global RunLaborStub
RunLaborStub:
    push rbx
    push rbp
    push rsi
    push rdi
    sub rsp, 0x58
    movdqu [rsp+0x30], xmm7
    mov rsi, rcx
    mov rbx, rdx
    mov rbp, r8
    mov rdi, r9
    pcmpeqd xmm7, xmm7
    mov rax, 0x123456789abcdef0
    xor edx, edx
    sub edx, 1
    pushfq
    pop rcx
    mov [rdi], rcx
    call rsi
    pushfq
    pop rcx
    mov [rdi+8], rcx
    mov [rdi+16], rax
    mov [rdi+24], rbx
    mov [rdi+32], rbp
    movdqu [rdi+40], xmm7
    movdqu xmm7, [rsp+0x30]
    add rsp, 0x58
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
