/*
    Copyright 2016-2022 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

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

#ifdef __APPLE__
.global _ARM_Dispatch
_ARM_Dispatch:
#else
.global ARM_Dispatch
ARM_Dispatch:
#endif
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

#ifdef __APPLE__
.global _ARM_Ret
_ARM_Ret:
#else
.global ARM_Ret
ARM_Ret:
#endif
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

#if !defined(__APPLE__) && !defined(WIN64)
.section .note.GNU-stack,"",@progbits
#endif

