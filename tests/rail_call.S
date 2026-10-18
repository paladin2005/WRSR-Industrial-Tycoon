.intel_syntax noprefix
.text
// RunTrackStub(stub, vehicle(rsi), site(rdi), observed)
// observed in : [0] xmm9 bits
// observed out: [8] xmm9 bits, [16] rax, [24] rcx, [32] rdx, [40] rsi, [48] rdi, [56] rbx,
//               [64] rbp, [72] r12, [80] rsp delta (0 when the stack is balanced)
.global RunTrackStub
RunTrackStub:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 0x38
    movdqu [rsp+0x10], xmm9
    mov rax, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r13, r9
    movd xmm9, dword ptr [r13]
    mov rcx, 0x1111222233334444
    mov rdx, 0x5555666677778888
    mov rbx, 0x0FEDCBA987654321
    mov rbp, 0x1234AAAABBBBCCCC
    mov r12, 0x0ABCDEF012345678
    mov [r13+80], rsp
    call rax
    sub [r13+80], rsp
    movd dword ptr [r13+8], xmm9
    mov [r13+16], rax
    mov [r13+24], rcx
    mov [r13+32], rdx
    mov [r13+40], rsi
    mov [r13+48], rdi
    mov [r13+56], rbx
    mov [r13+64], rbp
    mov [r13+72], r12
    movdqu xmm9, [rsp+0x10]
    add rsp, 0x38
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
