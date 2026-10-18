.intel_syntax noprefix
.text
// RunRepairStub(stub, station(rbx), game(rbp), observed)
// The stub is entered with the flags of the game's `cmp edi,eax`.
// observed in : [0] edi first person, [8] rax people present, [16] esi people per job
// observed out: [24] r11 path (1 loop, 2 factor, 3 done), [32] xmm7, [40] rbx, [48] rbp,
//               [56] rsi, [64] rdi, [72] rdx, [80] r12, [88] r14, [96] r8, [104] xmm6
.global RunRepairStub
RunRepairStub:
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
    mov r10, rcx
    mov rbx, rdx
    mov rbp, r8
    mov r13, r9
    mov edi, [r13]
    mov rax, [r13+8]
    mov esi, [r13+16]
    xorps xmm7, xmm7
    mov r8, 0x0badc0de0badc0de
    movq xmm6, r8
    mov rdx, 0x1111222233334444
    mov r12, 0x5555666677778888
    mov r14, 0x1234aaaabbbbcccc
    xor r11d, r11d
    cmp edi, eax
    call r10
    mov [r13+24], r11
    movd dword ptr [r13+32], xmm7
    mov [r13+40], rbx
    mov [r13+48], rbp
    mov [r13+56], rsi
    mov [r13+64], rdi
    mov [r13+72], rdx
    mov [r13+80], r12
    mov [r13+88], r14
    mov [r13+96], r8
    movq [r13+104], xmm6
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
