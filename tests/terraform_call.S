.intel_syntax noprefix
.text
// RunTerraformStub(stub, game(rsi), observed)
// observed out: [0] rax (the replayed mov eax,0x55555556), [8] rsi, [16] rbx, [24] rbp, [32] rdi,
//               [40] r8, [48] r9, [56] r10, [64] r11, [72] r12, [80] r13, [88] rsp delta
//               (0 when the stack is balanced), [96] r14, [104] r15
.global RunTerraformStub
RunTerraformStub:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 0x28
    mov [rsp+0x18], rcx
    mov [rsp+0x20], r8
    mov [r8+88], rsp
    mov rsi, rdx
    mov rbx, 0x1111222233334444
    mov rbp, 0x5555666677778888
    mov rdi, 0x0FEDCBA987654321
    mov r8, 0x1234AAAABBBBCCCC
    mov r9, 0x0ABCDEF012345678
    mov r10, 0x2222333344445555
    mov r11, 0x7777666655554444
    mov r12, 0x3333444455556666
    mov r13, 0x6666555544443333
    mov r14, 0x4444333322221111
    mov r15, 0x5A5A5A5AA5A5A5A5
    mov rax, 0x9999888877776666
    mov rcx, 0x8888777766665555
    mov rdx, 0x7777000077770000
    call qword ptr [rsp+0x18]
    mov rcx, [rsp+0x20]
    sub [rcx+88], rsp
    mov [rcx], rax
    mov [rcx+8], rsi
    mov [rcx+16], rbx
    mov [rcx+24], rbp
    mov [rcx+32], rdi
    mov [rcx+40], r8
    mov [rcx+48], r9
    mov [rcx+56], r10
    mov [rcx+64], r11
    mov [rcx+72], r12
    mov [rcx+80], r13
    mov [rcx+96], r14
    mov [rcx+104], r15
    add rsp, 0x28
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
