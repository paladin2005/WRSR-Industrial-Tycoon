.intel_syntax noprefix
.text
// RunWorkStub(stub, building (rbx and rsi), game (r14), observed)
// observed in : [0] work bits, loaded into both xmm13 and xmm6
// observed out: [8] xmm13 bits, [16] xmm6 bits, [24] r11 = flags captured by the continuation,
//               [32] rbx, [40] rsi, [48] r14, [56] r12, [64] rcx, [72] rdx, [80] rsp delta
.global RunWorkStub
RunWorkStub:
    push rbx
    push rsi
    push r12
    push r13
    push r14
    sub rsp, 0x30
    movdqu [rsp+0x10], xmm13
    movdqu [rsp+0x20], xmm6
    mov rax, rcx
    mov rbx, rdx
    mov rsi, rdx
    mov r14, r8
    mov r13, r9
    movd xmm13, dword ptr [r13]
    movd xmm6, dword ptr [r13]
    xor r12d, r12d
    mov rcx, 0x1111222233334444
    mov rdx, 0x5555666677778888
    xor r11d, r11d
    mov [r13+80], rsp
    call rax
    sub [r13+80], rsp
    movd dword ptr [r13+8], xmm13
    movd dword ptr [r13+16], xmm6
    mov [r13+24], r11
    mov [r13+32], rbx
    mov [r13+40], rsi
    mov [r13+48], r14
    mov [r13+56], r12
    mov [r13+64], rcx
    mov [r13+72], rdx
    movdqu xmm6, [rsp+0x20]
    movdqu xmm13, [rsp+0x10]
    add rsp, 0x30
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret

// RunWorkStub2(stub, building (rbx and rsi), game (rbp and r14), observed)
// For the control tower (rbx, xmm8, r14d people, cmp [rbp+0x5bc],r15d) and the water well (rsi, xmm15,
// [rsp+0x74] copy, cmp [r14+0x5bc],r15d). The stub's [rsp+0x74] is [rsp+0x6c] here.
// observed in : [0] work bits, loaded into both xmm8 and xmm15
// observed out: [8] xmm8, [16] xmm15, [24] r11 = flags, [32] rbx, [40] rsi, [48] r14, [56] rbp, [64] r15,
//               [72] rcx, [80] rdx, [88] dword at the stub's [rsp+0x74] (preset 0xDEADBEEF), [96] rsp delta
.global RunWorkStub2
RunWorkStub2:
    push rbx
    push rsi
    push rdi
    push rbp
    push r12
    push r13
    push r14
    push r15
    sub rsp, 0x98
    movdqu [rsp+0x80], xmm8
    movdqu [rsp+0x70], xmm15
    mov rax, rcx
    mov rbx, rdx
    mov rsi, rdx
    mov rbp, r8
    mov r14, r8
    mov r13, r9
    movd xmm8, dword ptr [r13]
    movd xmm15, dword ptr [r13]
    xor r15d, r15d
    mov dword ptr [rsp+0x6c], 0xdeadbeef
    mov rcx, 0x1111222233334444
    mov rdx, 0x5555666677778888
    xor r11d, r11d
    mov [r13+96], rsp
    call rax
    sub [r13+96], rsp
    movd dword ptr [r13+8], xmm8
    movd dword ptr [r13+16], xmm15
    mov [r13+24], r11
    mov [r13+32], rbx
    mov [r13+40], rsi
    mov [r13+48], r14
    mov [r13+56], rbp
    mov [r13+64], r15
    mov [r13+72], rcx
    mov [r13+80], rdx
    mov eax, dword ptr [rsp+0x6c]
    mov [r13+88], rax
    movdqu xmm8, [rsp+0x80]
    movdqu xmm15, [rsp+0x70]
    add rsp, 0x98
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
