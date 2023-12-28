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

#ifndef ARMINSTRINFO_H
#define ARMINSTRINFO_H

#include "types.h"

namespace melonDS::ARMInstrInfo
{

// Instruction kinds, for faster dispatch

#define ak_ALU(n) \
    ak_##n##_REG_LSL_IMM, \
    ak_##n##_REG_LSR_IMM, \
    ak_##n##_REG_ASR_IMM, \
    ak_##n##_REG_ROR_IMM, \
    \
    ak_##n##_REG_LSL_REG, \
    ak_##n##_REG_LSR_REG, \
    ak_##n##_REG_ASR_REG, \
    ak_##n##_REG_ROR_REG, \
    \
    ak_##n##_IMM, \
    \
    ak_##n##_REG_LSL_IMM_S, \
    ak_##n##_REG_LSR_IMM_S, \
    ak_##n##_REG_ASR_IMM_S, \
    ak_##n##_REG_ROR_IMM_S, \
    \
    ak_##n##_REG_LSL_REG_S, \
    ak_##n##_REG_LSR_REG_S, \
    ak_##n##_REG_ASR_REG_S, \
    ak_##n##_REG_ROR_REG_S, \
    \
    ak_##n##_IMM_S \

#define ak_Test(n) \
    ak_##n##_REG_LSL_IMM, \
    ak_##n##_REG_LSR_IMM, \
    ak_##n##_REG_ASR_IMM, \
    ak_##n##_REG_ROR_IMM, \
    \
    ak_##n##_REG_LSL_REG, \
    ak_##n##_REG_LSR_REG, \
    ak_##n##_REG_ASR_REG, \
    ak_##n##_REG_ROR_REG, \
    \
    ak_##n##_IMM

#define ak_WB_LDRSTR(n) \
    ak_##n##_REG_LSL, \
    ak_##n##_REG_LSR, \
    ak_##n##_REG_ASR, \
    ak_##n##_REG_ROR, \
    \
    ak_##n##_IMM, \
    \
    ak_##n##_POST_REG_LSL, \
    ak_##n##_POST_REG_LSR, \
    ak_##n##_POST_REG_ASR, \
    ak_##n##_POST_REG_ROR, \
    \
    ak_##n##_POST_IMM

#define ak_HD_LDRSTR(n) \
    ak_##n##_REG, \
    ak_##n##_IMM, \
    \
    ak_##n##_POST_REG, \
    ak_##n##_POST_IMM

enum
{
    ak_ALU(AND),
    ak_ALU(EOR),
    ak_ALU(SUB),
    ak_ALU(RSB),
    ak_ALU(ADD),
    ak_ALU(ADC),
    ak_ALU(SBC),
    ak_ALU(RSC),
    ak_ALU(ORR),
    ak_ALU(MOV),
    ak_ALU(BIC),
    ak_ALU(MVN),

    ak_Test(TST),
    ak_Test(TEQ),
    ak_Test(CMP),
    ak_Test(CMN),

    ak_MUL,
    ak_MLA,
    ak_UMULL,
    ak_UMLAL,
    ak_SMULL,
    ak_SMLAL,
    ak_SMLAxy,
    ak_SMLAWy,
    ak_SMULWy,
    ak_SMLALxy,
    ak_SMULxy,

    ak_CLZ,

    ak_QADD,
    ak_QSUB,
    ak_QDADD,
    ak_QDSUB,

    ak_WB_LDRSTR(STR),
    ak_WB_LDRSTR(STRB),
    ak_WB_LDRSTR(LDR),
    ak_WB_LDRSTR(LDRB),

    ak_HD_LDRSTR(STRH),
    ak_HD_LDRSTR(LDRD),
    ak_HD_LDRSTR(STRD),
    ak_HD_LDRSTR(LDRH),
    ak_HD_LDRSTR(LDRSB),
    ak_HD_LDRSTR(LDRSH),

    ak_SWP,
    ak_SWPB,

    ak_LDM,
    ak_STM,

    ak_B,
    ak_BL,
    ak_BLX_IMM,
    ak_BX,
    ak_BLX_REG,

    ak_UNK,
    ak_MSR_IMM,
    ak_MSR_REG,
    ak_MRS,
    ak_MCR,
    ak_MRC,
    ak_SVC,

    ak_Nop,

    ak_Count,

    tk_LSL_IMM = 0,
    tk_LSR_IMM,
    tk_ASR_IMM,

    tk_ADD_REG_,
    tk_SUB_REG_,
    tk_ADD_IMM_,
    tk_SUB_IMM_,

    tk_MOV_IMM,
    tk_CMP_IMM,
    tk_ADD_IMM,
    tk_SUB_IMM,

    tk_AND_REG,
    tk_EOR_REG,
    tk_LSL_REG,
    tk_LSR_REG,
    tk_ASR_REG,
    tk_ADC_REG,
    tk_SBC_REG,
    tk_ROR_REG,
    tk_TST_REG,
    tk_NEG_REG,
    tk_CMP_REG,
    tk_CMN_REG,
    tk_ORR_REG,
    tk_MUL_REG,
    tk_BIC_REG,
    tk_MVN_REG,

    tk_ADD_HIREG,
    tk_CMP_HIREG,
    tk_MOV_HIREG,

    tk_ADD_PCREL,
    tk_ADD_SPREL,
    tk_ADD_SP,

    tk_LDR_PCREL,
    tk_STR_REG,
    tk_STRB_REG,
    tk_LDR_REG,
    tk_LDRB_REG,
    tk_STRH_REG,
    tk_LDRSB_REG,
    tk_LDRH_REG,
    tk_LDRSH_REG,
    tk_STR_IMM,
    tk_LDR_IMM,
    tk_STRB_IMM,
    tk_LDRB_IMM,
    tk_STRH_IMM,
    tk_LDRH_IMM,
    tk_STR_SPREL,
    tk_LDR_SPREL,

    tk_PUSH,
    tk_POP,
    tk_LDMIA,
    tk_STMIA,

    tk_BCOND,
    tk_BX,
    tk_BLX_REG,
    tk_B,
    tk_BL_LONG_1,
    tk_BL_LONG_2,
    tk_UNK,
    tk_SVC,

    // not a real instruction
    tk_BL_LONG,

    tk_Count
};

enum
{
    flag_N = 1 << 3,
    flag_Z = 1 << 2,
    flag_C = 1 << 1,
    flag_V = 1 << 0,
};

enum
{
    special_NotSpecialAtAll = 0,
    special_WriteMem,
    special_LoadMem,
    special_WaitForInterrupt,
    special_LoadLiteral
};

struct Info
{
    u16 DstRegs, SrcRegs, NotStrictlyNeeded;
    u16 Kind;

    u8 SpecialKind;

    u8 ReadFlags;
    // lower 4 bits - set always
    // upper 4 bits - might set flag
    u8 WriteFlags;

    bool EndBlock;
    bool Branches() const
    {
        return DstRegs & (1 << 15);
    }
};

Info Decode(bool thumb, u32 num, u32 instr, bool literaloptimizations);

}

#endif
