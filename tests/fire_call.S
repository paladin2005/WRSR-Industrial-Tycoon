.intel_syntax noprefix
.text
// RunFireStub(stub, building(rsi), frame(rbp), observed)
// observed in : [0] ebx crew, [8] r15 vehicle index, [16] r12 counter, [24] xmm7 bits
// observed out: [32] r11 path (1 normal, 2 virtual), [40] xmm1, [48] xmm7, [56] r10,
//               [64] r12, [72] rbx, [80] rsi, [88] r15, [96] rbp, [104] rdi, [112] rcx, [120] rdx
.global RunFireStub
RunFireStub:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 0x48
    movdqu [rsp+0x10], xmm6
    movdqu [rsp+0x20], xmm7
    mov rax, rcx
    mov rsi, rdx
    mov rbp, r8
    mov r13, r9
    mov ebx, [r13]
    mov r15, [r13+8]
    mov r12, [r13+16]
    movd xmm7, dword ptr [r13+24]
    pcmpeqd xmm1, xmm1
    mov rdi, 0x1111222233334444
    mov rcx, 0x5555666677778888
    mov rdx, 0x1234aaaabbbbcccc
    mov r10, -1
    xor r11d, r11d
    call rax
    mov [r13+32], r11
    movd dword ptr [r13+40], xmm1
    movd dword ptr [r13+48], xmm7
    mov [r13+56], r10
    mov [r13+64], r12
    mov [r13+72], rbx
    mov [r13+80], rsi
    mov [r13+88], r15
    mov [r13+96], rbp
    mov [r13+104], rdi
    mov [r13+112], rcx
    mov [r13+120], rdx
    movdqu xmm6, [rsp+0x10]
    movdqu xmm7, [rsp+0x20]
    add rsp, 0x48
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
