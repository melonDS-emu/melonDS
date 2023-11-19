/*
    Copyright 2016-2023 melonDS team

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

#include "ARM_InstrInfo.h"

#include <stdio.h>

#include "ARMJIT.h"

namespace melonDS::ARMInstrInfo
{

#define ak(x) ((x) << 23)

enum {
    A_Read0             = 1 << 0,
    A_Read16            = 1 << 1,
    A_Read8             = 1 << 2,
    A_Read12            = 1 << 3,

    A_Write12           = 1 << 4,
    A_Write16           = 1 << 5,
    A_MemWriteback      = 1 << 6,

    A_BranchAlways      = 1 << 7,

    // for STRD/LDRD
    A_Read12Double      = 1 << 8,
    A_Write12Double     = 1 << 9,

    A_Link              = 1 << 10,

    A_UnkOnARM7         = 1 << 11,

    A_SetNZ             = 1 << 12,
    A_SetCV             = 1 << 13,
    A_SetMaybeC         = 1 << 14,
    A_MulFlags          = 1 << 15,
    A_ReadC             = 1 << 16,
    A_RRXReadC          = 1 << 17,
    A_StaticShiftSetC   = 1 << 18,
    A_SetC              = 1 << 19,
    A_SetCImm           = 1 << 20,

    A_WriteMem          = 1 << 21,
    A_LoadMem           = 1 << 22
};

#define A_BIOP A_Read16
#define A_MONOOP 0

#define A_ARITH_LSL_IMM A_SetCV
#define A_LOGIC_LSL_IMM A_StaticShiftSetC
#define A_ARITH_SHIFT_IMM A_SetCV
#define A_LOGIC_SHIFT_IMM A_SetC
#define A_ARITH_SHIFT_REG A_SetCV
#define A_LOGIC_SHIFT_REG A_SetMaybeC
#define A_ARITH_IMM A_SetCV
#define A_LOGIC_IMM A_SetCImm

#define A_IMPLEMENT_ALU_OP(x,k,a,c) \
    const u32 A_##x##_IMM = A_Write12 | c | A_##k | ak(ak_##x##_IMM); \
    const u32 A_##x##_REG_LSL_IMM = A_Write12 | c | A_##k | A_Read0 | ak(ak_##x##_REG_LSL_IMM); \
    const u32 A_##x##_REG_LSR_IMM = A_Write12 | c | A_##k | A_Read0 | ak(ak_##x##_REG_LSR_IMM); \
    const u32 A_##x##_REG_ASR_IMM = A_Write12 | c | A_##k | A_Read0 | ak(ak_##x##_REG_ASR_IMM); \
    const u32 A_##x##_REG_ROR_IMM = A_RRXReadC | A_Write12 | c | A_##k | A_Read0 | ak(ak_##x##_REG_ROR_IMM); \
    const u32 A_##x##_REG_LSL_REG = A_Write12 | c | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSL_REG); \
    const u32 A_##x##_REG_LSR_REG = A_Write12 | c | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSR_REG); \
    const u32 A_##x##_REG_ASR_REG = A_Write12 | c | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_ASR_REG); \
    const u32 A_##x##_REG_ROR_REG = A_Write12 | c | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_ROR_REG); \
    \
    const u32 A_##x##_IMM_S = A_SetNZ | c | A_##a##_IMM | A_Write12 | A_##k | ak(ak_##x##_IMM_S); \
    const u32 A_##x##_REG_LSL_IMM_S = A_SetNZ | c | A_##a##_LSL_IMM | A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_LSL_IMM_S); \
    const u32 A_##x##_REG_LSR_IMM_S = A_SetNZ | c | A_##a##_SHIFT_IMM | A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_LSR_IMM_S); \
    const u32 A_##x##_REG_ASR_IMM_S = A_SetNZ | c | A_##a##_SHIFT_IMM | A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_ASR_IMM_S); \
    const u32 A_##x##_REG_ROR_IMM_S = A_RRXReadC | A_SetNZ | c | A_##a##_SHIFT_IMM | A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_ROR_IMM_S); \
    const u32 A_##x##_REG_LSL_REG_S = A_SetNZ | c | A_##a##_SHIFT_REG | A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSL_REG_S); \
    const u32 A_##x##_REG_LSR_REG_S = A_SetNZ | c | A_##a##_SHIFT_REG | A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSR_REG_S); \
    const u32 A_##x##_REG_ASR_REG_S = A_SetNZ | c | A_##a##_SHIFT_REG | A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_ASR_REG_S); \
    const u32 A_##x##_REG_ROR_REG_S = A_SetNZ | c | A_##a##_SHIFT_REG | A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_ROR_REG_S);

A_IMPLEMENT_ALU_OP(AND,BIOP,LOGIC,0)
A_IMPLEMENT_ALU_OP(EOR,BIOP,LOGIC,0)
A_IMPLEMENT_ALU_OP(SUB,BIOP,ARITH,0)
A_IMPLEMENT_ALU_OP(RSB,BIOP,ARITH,0)
A_IMPLEMENT_ALU_OP(ADD,BIOP,ARITH,0)
A_IMPLEMENT_ALU_OP(ADC,BIOP,ARITH,A_ReadC)
A_IMPLEMENT_ALU_OP(SBC,BIOP,ARITH,A_ReadC)
A_IMPLEMENT_ALU_OP(RSC,BIOP,ARITH,A_ReadC)
A_IMPLEMENT_ALU_OP(ORR,BIOP,LOGIC,0)
A_IMPLEMENT_ALU_OP(MOV,MONOOP,LOGIC,0)
A_IMPLEMENT_ALU_OP(BIC,BIOP,LOGIC,0)
A_IMPLEMENT_ALU_OP(MVN,MONOOP,LOGIC,0)

const u32 A_MOV_REG_LSL_IMM_DBG = A_MOV_REG_LSL_IMM;

#define A_IMPLEMENT_ALU_TEST(x,a) \
    const u32 A_##x##_IMM = A_SetNZ | A_Read16 | A_##a##_IMM | ak(ak_##x##_IMM); \
    const u32 A_##x##_REG_LSL_IMM = A_SetNZ | A_Read16 | A_##a##_LSL_IMM | A_Read0 | ak(ak_##x##_REG_LSL_IMM); \
    const u32 A_##x##_REG_LSR_IMM = A_SetNZ | A_Read16 | A_##a##_SHIFT_IMM | A_Read0 | ak(ak_##x##_REG_LSR_IMM); \
    const u32 A_##x##_REG_ASR_IMM = A_SetNZ | A_Read16 | A_##a##_SHIFT_IMM | A_Read0 | ak(ak_##x##_REG_ASR_IMM); \
    const u32 A_##x##_REG_ROR_IMM = A_RRXReadC | A_SetNZ | A_Read16 | A_##a##_SHIFT_IMM | A_Read0 | ak(ak_##x##_REG_ROR_IMM); \
    const u32 A_##x##_REG_LSL_REG = A_SetNZ | A_Read16 | A_##a##_SHIFT_REG | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSL_REG); \
    const u32 A_##x##_REG_LSR_REG = A_SetNZ | A_Read16 | A_##a##_SHIFT_REG | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSR_REG); \
    const u32 A_##x##_REG_ASR_REG = A_SetNZ | A_Read16 | A_##a##_SHIFT_REG | A_Read0 | A_Read8 | ak(ak_##x##_REG_ASR_REG); \
    const u32 A_##x##_REG_ROR_REG = A_SetNZ | A_Read16 | A_##a##_SHIFT_REG | A_Read0 | A_Read8 | ak(ak_##x##_REG_ROR_REG);

A_IMPLEMENT_ALU_TEST(TST,LOGIC)
A_IMPLEMENT_ALU_TEST(TEQ,LOGIC)
A_IMPLEMENT_ALU_TEST(CMP,ARITH)
A_IMPLEMENT_ALU_TEST(CMN,ARITH)

const u32 A_MUL = A_MulFlags | A_Write16 | A_Read0 | A_Read8 | ak(ak_MUL);
const u32 A_MLA = A_MulFlags | A_Write16 | A_Read0 | A_Read8 | A_Read12 | ak(ak_MLA);
const u32 A_UMULL = A_MulFlags | A_Write16 | A_Write12 | A_Read0 | A_Read8 | ak(ak_UMULL);
const u32 A_UMLAL = A_MulFlags | A_Write16 | A_Write12 | A_Read16 | A_Read12 | A_Read0 | A_Read8 | ak(ak_UMLAL);
const u32 A_SMULL = A_MulFlags | A_Write16 | A_Write12 | A_Read0 | A_Read8 | ak(ak_SMULL);
const u32 A_SMLAL = A_MulFlags | A_Write16 | A_Write12 | A_Read16 | A_Read12 | A_Read0 | A_Read8 | ak(ak_SMLAL);
const u32 A_SMLAxy = A_Write16 | A_Read0 | A_Read8 | A_Read12 | ak(ak_SMLAxy);
const u32 A_SMLAWy = A_Write16 | A_Read0 | A_Read8 | A_Read12 | ak(ak_SMLAWy);
const u32 A_SMULWy = A_Write16 | A_Read0 | A_Read8 | ak(ak_SMULWy);
const u32 A_SMLALxy = A_Write16 | A_Write12 | A_Read16 | A_Read12 | A_Read0 | A_Read8 | ak(ak_SMLALxy);
const u32 A_SMULxy = A_Write16 | A_Read0 | A_Read8 | ak(ak_SMULxy);

const u32 A_CLZ = A_Write12 | A_Read0 | A_UnkOnARM7 | ak(ak_CLZ);

const u32 A_QADD = A_Write12 | A_Read0 | A_Read16 | A_UnkOnARM7 | ak(ak_QADD);
const u32 A_QSUB = A_Write12 | A_Read0 | A_Read16 | A_UnkOnARM7 | ak(ak_QSUB);
const u32 A_QDADD = A_Write12 | A_Read0 | A_Read16 | A_UnkOnARM7 | ak(ak_QDADD);
const u32 A_QDSUB = A_Write12 | A_Read0 | A_Read16 | A_UnkOnARM7 | ak(ak_QDSUB);

#define A_LDR A_Write12 | A_LoadMem
#define A_STR A_Read12 | A_WriteMem

#define A_IMPLEMENT_WB_LDRSTR(x,k) \
    const u32 A_##x##_IMM = A_##k | A_Read16 | A_MemWriteback | ak(ak_##x##_IMM); \
    const u32 A_##x##_REG_LSL = A_##k | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG_LSL); \
    const u32 A_##x##_REG_LSR = A_##k | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG_LSR); \
    const u32 A_##x##_REG_ASR = A_##k | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG_ASR); \
    const u32 A_##x##_REG_ROR = A_##k | A_RRXReadC | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG_ROR); \
    \
    const u32 A_##x##_POST_IMM = A_##k | A_Read16 | A_Write16 | ak(ak_##x##_POST_IMM); \
    const u32 A_##x##_POST_REG_LSL = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG_LSL); \
    const u32 A_##x##_POST_REG_LSR = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG_LSR); \
    const u32 A_##x##_POST_REG_ASR = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG_ASR); \
    const u32 A_##x##_POST_REG_ROR = A_##k | A_RRXReadC | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG_ROR);

A_IMPLEMENT_WB_LDRSTR(STR,STR)
A_IMPLEMENT_WB_LDRSTR(STRB,STR)
A_IMPLEMENT_WB_LDRSTR(LDR,LDR)
A_IMPLEMENT_WB_LDRSTR(LDRB,LDR)

#define A_LDRD A_Write12Double | A_LoadMem
#define A_STRD A_Read12Double | A_WriteMem

#define A_IMPLEMENT_HD_LDRSTR(x,k) \
    const u32 A_##x##_IMM = A_##k | A_Read16 | A_MemWriteback | ak(ak_##x##_IMM); \
    const u32 A_##x##_REG = A_##k | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG); \
    const u32 A_##x##_POST_IMM = A_##k | A_Read16 | A_Write16 | ak(ak_##x##_POST_IMM); \
    const u32 A_##x##_POST_REG = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG);

A_IMPLEMENT_HD_LDRSTR(STRH,STR)
A_IMPLEMENT_HD_LDRSTR(LDRD,LDRD)
A_IMPLEMENT_HD_LDRSTR(STRD,STRD)
A_IMPLEMENT_HD_LDRSTR(LDRH,LDR)
A_IMPLEMENT_HD_LDRSTR(LDRSB,LDR)
A_IMPLEMENT_HD_LDRSTR(LDRSH,LDR)

const u32 A_SWP = A_Write12 | A_Read16 | A_Read0 | A_LoadMem | A_WriteMem | ak(ak_SWP);
const u32 A_SWPB = A_Write12 | A_Read16 | A_Read0 | A_LoadMem | A_WriteMem | ak(ak_SWPB);

const u32 A_LDM = A_Read16 | A_MemWriteback | A_LoadMem | ak(ak_LDM);
const u32 A_STM = A_Read16 | A_MemWriteback | A_WriteMem | ak(ak_STM);

const u32 A_B = A_BranchAlways | ak(ak_B);
const u32 A_BL = A_BranchAlways | A_Link | ak(ak_BL);
const u32 A_BLX_IMM = A_BranchAlways | A_Link | ak(ak_BLX_IMM);
const u32 A_BX = A_BranchAlways | A_Read0 | ak(ak_BX);
const u32 A_BLX_REG = A_BranchAlways | A_Link | A_Read0 | ak(ak_BLX_REG);

const u32 A_UNK = A_BranchAlways | A_Link | ak(ak_UNK);
const u32 A_MSR_IMM = ak(ak_MSR_IMM);
const u32 A_MSR_REG = A_Read0 | ak(ak_MSR_REG);
const u32 A_MRS = A_Write12 | ak(ak_MRS);
const u32 A_MCR = A_Read12 | ak(ak_MCR);
const u32 A_MRC = A_Write12 | ak(ak_MRC);
const u32 A_SVC = A_BranchAlways | A_Link | ak(ak_SVC);

// THUMB

#define tk(x) ((x) << 22)

enum {
    T_Read0         = 1 << 0,
    T_Read3         = 1 << 1,
    T_Read6         = 1 << 2,
    T_Read8         = 1 << 3,

    T_Write0        = 1 << 4,
    T_Write8        = 1 << 5,

    T_ReadHi0       = 1 << 6,
    T_ReadHi3       = 1 << 7,
    T_WriteHi0      = 1 << 8,

    T_ReadR13       = 1 << 9,
    T_WriteR13      = 1 << 10,

    T_BranchAlways  = 1 << 12,
    T_ReadR14       = 1 << 13,
    T_WriteR14      = 1 << 14,

    T_SetNZ         = 1 << 15,
    T_SetCV         = 1 << 16,
    T_SetMaybeC     = 1 << 17,
    T_ReadC         = 1 << 18,
    T_SetC          = 1 << 19,

    T_WriteMem      = 1 << 20,
    T_LoadMem       = 1 << 21,
};

const u32 T_LSL_IMM = T_SetNZ | T_SetMaybeC | T_Write0 | T_Read3 | tk(tk_LSL_IMM);
const u32 T_LSR_IMM = T_SetNZ | T_SetC | T_Write0 | T_Read3 | tk(tk_LSR_IMM);
const u32 T_ASR_IMM = T_SetNZ | T_SetC | T_Write0 | T_Read3 | tk(tk_ASR_IMM);

const u32 T_ADD_REG_ = T_SetNZ | T_SetCV | T_Write0 | T_Read3 | T_Read6 | tk(tk_ADD_REG_);
const u32 T_SUB_REG_ = T_SetNZ | T_SetCV | T_Write0 | T_Read3 | T_Read6 | tk(tk_SUB_REG_);
const u32 T_ADD_IMM_ = T_SetNZ | T_SetCV | T_Write0 | T_Read3 | tk(tk_ADD_IMM_);
const u32 T_SUB_IMM_ = T_SetNZ | T_SetCV | T_Write0 | T_Read3 | tk(tk_SUB_IMM_);

const u32 T_MOV_IMM = T_SetNZ | T_Write8 | tk(tk_MOV_IMM);
const u32 T_CMP_IMM = T_SetNZ | T_SetCV | T_Read8 | tk(tk_CMP_IMM);
const u32 T_ADD_IMM = T_SetNZ | T_SetCV | T_Write8 | T_Read8 | tk(tk_ADD_IMM);
const u32 T_SUB_IMM = T_SetNZ | T_SetCV | T_Write8 | T_Read8 | tk(tk_SUB_IMM);

const u32 T_AND_REG = T_SetNZ | T_Write0 | T_Read0 | T_Read3 | tk(tk_AND_REG);
const u32 T_EOR_REG = T_SetNZ | T_Write0 | T_Read0 | T_Read3 | tk(tk_EOR_REG);
const u32 T_LSL_REG = T_SetNZ | T_SetMaybeC | T_Write0 | T_Read0 | T_Read3 | tk(tk_LSL_REG);
const u32 T_LSR_REG = T_SetNZ | T_SetMaybeC | T_Write0 | T_Read0 | T_Read3 | tk(tk_LSR_REG);
const u32 T_ASR_REG = T_SetNZ | T_SetMaybeC | T_Write0 | T_Read0 | T_Read3 | tk(tk_ASR_REG);
const u32 T_ADC_REG = T_ReadC | T_SetNZ | T_SetCV | T_Write0 | T_Read0 | T_Read3 | tk(tk_ADC_REG);
const u32 T_SBC_REG = T_ReadC | T_SetNZ | T_SetCV | T_Write0 | T_Read0 | T_Read3 | tk(tk_SBC_REG);
const u32 T_ROR_REG = T_SetNZ | T_SetMaybeC | T_Write0 | T_Read0 | T_Read3 | tk(tk_ROR_REG);
const u32 T_TST_REG = T_SetNZ | T_Read0 | T_Read3 | tk(tk_TST_REG);
const u32 T_NEG_REG = T_SetNZ | T_SetCV | T_Write0 | T_Read3 | tk(tk_NEG_REG);
const u32 T_CMP_REG = T_SetNZ | T_SetCV | T_Read0 | T_Read3 | tk(tk_CMP_REG);
const u32 T_CMN_REG = T_SetNZ | T_SetCV | T_Read0 | T_Read3 | tk(tk_CMN_REG);
const u32 T_ORR_REG = T_SetNZ | T_Write0 | T_Read0 | T_Read3 | tk(tk_ORR_REG);
const u32 T_MUL_REG = T_SetNZ | T_Write0 | T_Read0 | T_Read3 | tk(tk_MUL_REG);
const u32 T_BIC_REG = T_SetNZ | T_Write0 | T_Read0 | T_Read3 | tk(tk_BIC_REG);
const u32 T_MVN_REG = T_SetNZ | T_Write0 | T_Read3 | tk(tk_MVN_REG);

const u32 T_ADD_HIREG = T_WriteHi0 | T_ReadHi0 | T_ReadHi3 | tk(tk_ADD_HIREG);
const u32 T_CMP_HIREG = T_SetNZ | T_SetCV | T_ReadHi0 | T_ReadHi3 | tk(tk_CMP_HIREG);
const u32 T_MOV_HIREG = T_WriteHi0 | T_ReadHi3 | tk(tk_MOV_HIREG);

const u32 T_ADD_PCREL = T_Write8 | tk(tk_ADD_PCREL);
const u32 T_ADD_SPREL = T_Write8 | T_ReadR13 | tk(tk_ADD_SPREL);
const u32 T_ADD_SP = T_WriteR13 | T_ReadR13 | tk(tk_ADD_SP);

const u32 T_LDR_PCREL = T_Write8 | T_LoadMem | tk(tk_LDR_PCREL);

const u32 T_STR_REG = T_Read0 | T_Read3 | T_Read6 | T_WriteMem | tk(tk_STR_REG);
const u32 T_STRB_REG = T_Read0 | T_Read3 | T_Read6 | T_WriteMem | tk(tk_STRB_REG);
const u32 T_LDR_REG = T_Write0 | T_Read3 | T_Read6 | T_LoadMem | tk(tk_LDR_REG);
const u32 T_LDRB_REG = T_Write0 | T_Read3 | T_Read6 | T_LoadMem | tk(tk_LDRB_REG);
const u32 T_STRH_REG = T_Read0 | T_Read3 | T_Read6 | T_WriteMem | tk(tk_STRH_REG);
const u32 T_LDRSB_REG = T_Write0 | T_Read3 | T_Read6 | T_LoadMem | tk(tk_LDRSB_REG);
const u32 T_LDRH_REG = T_Write0 | T_Read3 | T_Read6 | T_LoadMem | tk(tk_LDRH_REG);
const u32 T_LDRSH_REG = T_Write0 | T_Read3 | T_Read6 | T_LoadMem | tk(tk_LDRSH_REG);

const u32 T_STR_IMM = T_Read0 | T_Read3 | T_WriteMem | tk(tk_STR_IMM);
const u32 T_LDR_IMM = T_Write0 | T_Read3 | T_LoadMem | tk(tk_LDR_IMM);
const u32 T_STRB_IMM = T_Read0 | T_Read3 | T_WriteMem | tk(tk_STRB_IMM);
const u32 T_LDRB_IMM = T_Write0 | T_Read3 | T_LoadMem | tk(tk_LDRB_IMM);
const u32 T_STRH_IMM = T_Read0 | T_Read3 | T_WriteMem | tk(tk_STRH_IMM);
const u32 T_LDRH_IMM = T_Write0 | T_Read3 | T_LoadMem | tk(tk_LDRH_IMM);

const u32 T_STR_SPREL = T_Read8 | T_ReadR13 | T_WriteMem | tk(tk_STR_SPREL);
const u32 T_LDR_SPREL = T_Write8 | T_ReadR13 | T_LoadMem | tk(tk_LDR_SPREL);

const u32 T_PUSH = T_ReadR13 | T_WriteR13 | T_WriteMem | tk(tk_PUSH);
const u32 T_POP = T_ReadR13 | T_WriteR13 | T_LoadMem | tk(tk_POP);

const u32 T_LDMIA = T_Read8 | T_Write8 | T_LoadMem | tk(tk_LDMIA);
const u32 T_STMIA = T_Read8 | T_Write8 | T_WriteMem | tk(tk_STMIA);

const u32 T_BCOND = T_BranchAlways | tk(tk_BCOND);
const u32 T_BX = T_BranchAlways | T_ReadHi3 | tk(tk_BX);
const u32 T_BLX_REG = T_BranchAlways | T_WriteR14 | T_ReadHi3 | tk(tk_BLX_REG);
const u32 T_B = T_BranchAlways | tk(tk_B);
const u32 T_BL_LONG_1 = T_WriteR14 | tk(tk_BL_LONG_1);
const u32 T_BL_LONG_2 = T_BranchAlways | T_ReadR14 | T_WriteR14 | tk(tk_BL_LONG_2);

const u32 T_UNK = T_BranchAlways | T_WriteR14 | tk(tk_UNK);
const u32 T_SVC = T_BranchAlways | T_WriteR14 | tk(tk_SVC);

#define INSTRFUNC_PROTO(x) u32 x
#include "ARM_InstrTable.h"
#undef INSTRFUNC_PROTO

Info Decode(bool thumb, u32 num, u32 instr, bool literaloptimizations)
{
    const u8 FlagsReadPerCond[7] = {
        flag_Z,
        flag_C,
        flag_N,
        flag_V,
        flag_C | flag_Z,
        flag_N | flag_V,
        flag_Z | flag_N | flag_V};

    Info res = {0};
    if (thumb)
    {
        u32 data = THUMBInstrTable[(instr >> 6) & 0x3FF];
        res.Kind = (data >> 22) & 0x3F;

        if (data & T_Read0)
            res.SrcRegs |= 1 << (instr & 0x7);
        if (data & T_Read3)
            res.SrcRegs |= 1 << ((instr >> 3) & 0x7);
        if (data & T_Read6)
            res.SrcRegs |= 1 << ((instr >> 6) & 0x7);
        if (data & T_Read8)
            res.SrcRegs |= 1 << ((instr >> 8) & 0x7);

        if (data & T_Write0)
            res.DstRegs |= 1 << (instr & 0x7);
        if (data & T_Write8)
            res.DstRegs |= 1 << ((instr >> 8) & 0x7);

        if (data & T_ReadHi0)
            res.SrcRegs |= 1 << ((instr & 0x7) | ((instr >> 4) & 0x8));
        if (data & T_ReadHi3)
            res.SrcRegs |= 1 << ((instr >> 3) & 0xF);
        if (data & T_WriteHi0)
            res.DstRegs |= 1 << ((instr & 0x7) | ((instr >> 4) & 0x8));

        if (data & T_ReadR13)
            res.SrcRegs |= (1 << 13);
        if (data & T_WriteR13)
            res.DstRegs |= (1 << 13);
        if (data & T_WriteR14)
            res.DstRegs |= (1 << 14);
        if (data & T_ReadR14)
            res.SrcRegs |= (1 << 14);

        if (data & T_BranchAlways)
            res.DstRegs |= (1 << 15);

        if (res.Kind == tk_POP && instr & (1 << 8))
            res.DstRegs |= 1 << 15;

        if (data & T_SetNZ)
            res.WriteFlags |= flag_N | flag_Z;
        if (data & T_SetCV)
            res.WriteFlags |= flag_C | flag_V;
        if (data & T_SetMaybeC)
            res.WriteFlags |= flag_C << 4;
        if (data & T_ReadC)
            res.ReadFlags |= flag_C;
        if (data & T_SetC)
            res.WriteFlags |= flag_C;

        if (data & T_WriteMem)
            res.SpecialKind = special_WriteMem;

        if (data & T_LoadMem)
        {
            if (res.Kind == tk_LDR_PCREL)
            {
                if (!literaloptimizations)
                    res.SrcRegs |= 1 << 15;
                res.SpecialKind = special_LoadLiteral;
            }
            else
            {
                res.SpecialKind = special_LoadMem;
            }
        }

        if (res.Kind == tk_LDMIA || res.Kind == tk_POP)
        {
            u32 set = (instr & 0xFF);
            res.NotStrictlyNeeded |= set & ~(res.DstRegs|res.SrcRegs);
            res.DstRegs |= set;
        }
        if (res.Kind == tk_STMIA || res.Kind == tk_PUSH)
        {
            u32 set = (instr & 0xFF);
            if (res.Kind == tk_PUSH && instr & (1 << 8))
                set |= (1 << 14);
            res.NotStrictlyNeeded |= set & ~(res.DstRegs|res.SrcRegs);
            res.SrcRegs |= set;
        }

        res.EndBlock |= res.Branches();

        if (res.Kind == tk_BCOND)
            res.ReadFlags |= FlagsReadPerCond[(instr >> 9) & 0x7];

        return res;
    }
    else
    {
        u32 data = ARMInstrTable[((instr >> 4) & 0xF) | ((instr >> 16) & 0xFF0)];
        if (num == 0 && (instr & 0xFE000000) == 0xFA000000)
            data = A_BLX_IMM;
        else if ((instr >> 28) == 0xF)
            data = ak(ak_Nop);

        if (data & A_UnkOnARM7 && num == 1)
            data = A_UNK;

        res.Kind = (data >> 23) & 0x1FF;

        if (res.Kind >= ak_SMLAxy && res.Kind <= ak_SMULxy && num == 1)
        {
            data = ak(ak_Nop);
            res.Kind = ak_Nop;
        }

        if (res.Kind == ak_MCR)
        {
            u32 cn = (instr >> 16) & 0xF;
            u32 cm = instr & 0xF;
            u32 cpinfo = (instr >> 5) & 0x7;
            u32 id = (cn<<8)|(cm<<4)|cpinfo;
            if (id == 0x704 || id == 0x782 || id == 0x750 || id == 0x751 || id == 0x752)
                res.EndBlock |= true;

            if (id == 0x704 || id == 0x782)
                res.SpecialKind = special_WaitForInterrupt;
        }
        if (res.Kind == ak_MCR || res.Kind == ak_MRC)
        {
            u32 cp = ((instr >> 8) & 0xF);
            if ((num == 0 && cp != 15) || (num == 1 && cp != 14))
            {
                data = A_UNK;
                res.Kind = ak_UNK;
            }
        }
        if (res.Kind == ak_MRS && !(instr & (1 << 22)))
            res.ReadFlags |= flag_N | flag_Z | flag_C | flag_V;
        if ((res.Kind == ak_MSR_IMM || res.Kind == ak_MSR_REG) && instr & (1 << 19))
            res.WriteFlags |= flag_N | flag_Z | flag_C | flag_V;

        if (data & A_Read0)
            res.SrcRegs |= 1 << (instr & 0xF);
        if (data & A_Read16)
            res.SrcRegs |= 1 << ((instr >> 16) & 0xF);
        if (data & A_Read8)
            res.SrcRegs |= 1 << ((instr >> 8) & 0xF);
        if (data & A_Read12)
            res.SrcRegs |= 1 << ((instr >> 12) & 0xF);

        if (data & A_Write12)
            res.DstRegs |= 1 << ((instr >> 12) & 0xF);
        if (data & A_Write16)
            res.DstRegs |= 1 << ((instr >> 16) & 0xF);

        if (data & A_MemWriteback && instr & (1 << 21))
            res.DstRegs |= 1 << ((instr >> 16) & 0xF);

        if (data & A_BranchAlways)
            res.DstRegs |= 1 << 15;

        if (data & A_Read12Double)
        {
            res.SrcRegs |= 1 << ((instr >> 12) & 0xF);
            res.SrcRegs |= 1 << (((instr >> 12) & 0xF) + 1);
        }
        if (data & A_Write12Double)
        {
            res.DstRegs |= 1 << ((instr >> 12) & 0xF);
            res.DstRegs |= 1 << (((instr >> 12) & 0xF) + 1);
        }

        if (data & A_Link)
            res.DstRegs |= 1 << 14;

        if (res.Kind == ak_LDM)
            res.DstRegs |= instr & (1 << 15); // this is right

        if (res.Kind == ak_STM)
            res.SrcRegs |= instr & (1 << 15);

        if (data & A_SetNZ)
            res.WriteFlags |= flag_N | flag_Z;
        if (data & A_SetCV)
            res.WriteFlags |= flag_C | flag_V;
        if (data & A_SetMaybeC)
            res.WriteFlags |= flag_C << 4;
        if ((data & A_MulFlags) && (instr & (1 << 20)))
            res.WriteFlags |= flag_N | flag_Z;
        if (data & A_ReadC)
            res.ReadFlags |= flag_C;
        if ((data & A_RRXReadC) && !((instr >> 7) & 0x1F))
            res.ReadFlags |= flag_C;
        if ((data & A_SetC)
            || ((data & A_StaticShiftSetC) && ((instr >> 7) & 0x1F))
            || ((data & A_SetCImm) && ((instr >> 7) & 0x1E)))
            res.WriteFlags |= flag_C;

        if (data & A_WriteMem)
            res.SpecialKind = special_WriteMem;

        if (data & A_LoadMem)
        {
            if (res.SrcRegs == (1 << 15))
                res.SpecialKind = special_LoadLiteral;
            else
                res.SpecialKind = special_LoadMem;
        }

        if (res.Kind == ak_LDM)
        {
            u16 set = (instr & 0xFFFF);
            res.NotStrictlyNeeded |= set & ~(res.SrcRegs|res.DstRegs|(1<<15));
            res.DstRegs |= set;
            // when the instruction is executed not in usermode a banked register in memory will be written to
            // but the unbanked register will still be allocated, so it is expected to carry the proper value
            // thus it is a source register
            if (instr & (1<<22))
                res.SrcRegs |= set & 0x7F00;
        }
        if (res.Kind == ak_STM)
        {
            u16 set = (instr & 0xFFFF);
            res.NotStrictlyNeeded |= set & ~(res.SrcRegs|res.DstRegs|(1<<15));
            res.SrcRegs |= set;
        }

        if ((instr >> 28) < 0xE)
        {
            // make non conditional flag sets conditional
            res.WriteFlags = (res.WriteFlags | (res.WriteFlags << 4)) & 0xF0;
            res.ReadFlags |= FlagsReadPerCond[instr >> 29];
        }

        res.EndBlock |= res.Branches();

        return res;
    }
}

}
