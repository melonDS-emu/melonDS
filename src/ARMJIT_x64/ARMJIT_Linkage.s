.intel_syntax noprefix

#include "ARMJIT_Offsets.h"

.text

#define RCPU rbp
#define RCPSR r15d

#ifdef WIN64
#define ARG1_REG ecx
#define ARG2_REG edx
#define ARG3_REG r8d
#define ARG4_REG r9d
#define ARG1_REG64 rcx
#define ARG2_REG64 rdx
#define ARG3_REG64 r8
#define ARG4_REG64 r9
#else
#define ARG1_REG edi
#define ARG2_REG esi
#define ARG3_REG edx
#define ARG4_REG ecx
#define ARG1_REG64 rdi
#define ARG2_REG64 rsi
#define ARG3_REG64 rdx
#define ARG4_REG64 rcx
#endif

.p2align 4,,15

.global ARM_Dispatch
ARM_Dispatch:
#ifdef WIN64
    push rdi
    push rsi
#endif
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp

#ifdef WIN64
    sub rsp, 0x28
#else
    sub rsp, 0x8
#endif
    mov RCPU, ARG1_REG64
    mov RCPSR, [RCPU + ARM_CPSR_offset]

    jmp ARG2_REG64

.p2align 4,,15

.global ARM_Ret
ARM_Ret:
    mov [RCPU + ARM_CPSR_offset], RCPSR

#ifdef WIN64
    add rsp, 0x28
#else
    add rsp, 0x8
#endif

    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
#ifdef WIN64
    pop rsi
    pop rdi
#endif

    ret
