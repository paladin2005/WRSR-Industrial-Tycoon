.intel_syntax noprefix
.text
// RunAsphaltStub(stub, site(rbx), game(rbp), observed)
// observed in : [0] al, the result of 0x1521F0 (flags come from test al,al as in the game)
// observed out: [8] r11 path (1 admit, 2 reject), [16] rbx, [24] rbp, [32] rcx, [40] rdx,
//               [48] rsi, [56] rdi, [64] r12, [72] r14, [80] rsp delta (0 when the stack is balanced)
.global RunAsphaltStub
RunAsphaltStub:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 0x20
    mov r10, rcx
    mov rbx, rdx
    mov rbp, r8
    mov r13, r9
    mov rcx, 0x1111222233334444
    mov rdx, 0x5555666677778888
    mov rsi, 0x0FEDCBA987654321
    mov rdi, 0x1234AAAABBBBCCCC
    mov r12, 0x0ABCDEF012345678
    mov r14, 0x7777666655554444
    xor r11d, r11d
    mov [r13+80], rsp
    movzx eax, byte ptr [r13]
    test al, al
    call r10
    sub [r13+80], rsp
    mov [r13+8], r11
    mov [r13+16], rbx
    mov [r13+24], rbp
    mov [r13+32], rcx
    mov [r13+40], rdx
    mov [r13+48], rsi
    mov [r13+56], rdi
    mov [r13+64], r12
    mov [r13+72], r14
    add rsp, 0x20
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
