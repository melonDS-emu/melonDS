#include "ARM_InstrInfo.h"

#include <stdio.h>

namespace ARMInstrInfo
{

#define ak(x) ((x) << 13)

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

    A_LDMSTM            = 1 << 11,

    A_ARM9Only          = 1 << 12,
};

#define A_BIOP A_Read16
#define A_MONOOP 0

#define A_IMPLEMENT_ALU_OP(x,k) \
    const u32 A_##x##_IMM = A_Write12 | A_##k | ak(ak_##x##_IMM); \
    const u32 A_##x##_REG_LSL_IMM = A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_LSL_IMM); \
    const u32 A_##x##_REG_LSR_IMM = A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_LSR_IMM); \
    const u32 A_##x##_REG_ASR_IMM = A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_ASR_IMM); \
    const u32 A_##x##_REG_ROR_IMM = A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_ROR_IMM); \
    const u32 A_##x##_REG_LSL_REG = A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSL_REG); \
    const u32 A_##x##_REG_LSR_REG = A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSR_REG); \
    const u32 A_##x##_REG_ASR_REG = A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_ASR_REG); \
    const u32 A_##x##_REG_ROR_REG = A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_ROR_REG); \
    \
    const u32 A_##x##_IMM_S = A_Write12 | A_##k | ak(ak_##x##_IMM_S); \
    const u32 A_##x##_REG_LSL_IMM_S = A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_LSL_IMM_S); \
    const u32 A_##x##_REG_LSR_IMM_S = A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_LSR_IMM_S); \
    const u32 A_##x##_REG_ASR_IMM_S = A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_ASR_IMM_S); \
    const u32 A_##x##_REG_ROR_IMM_S = A_Write12 | A_##k | A_Read0 | ak(ak_##x##_REG_ROR_IMM_S); \
    const u32 A_##x##_REG_LSL_REG_S = A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSL_REG_S); \
    const u32 A_##x##_REG_LSR_REG_S = A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSR_REG_S); \
    const u32 A_##x##_REG_ASR_REG_S = A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_ASR_REG_S); \
    const u32 A_##x##_REG_ROR_REG_S = A_Write12 | A_##k | A_Read0 | A_Read8 | ak(ak_##x##_REG_ROR_REG_S);

A_IMPLEMENT_ALU_OP(AND,BIOP)
A_IMPLEMENT_ALU_OP(EOR,BIOP)
A_IMPLEMENT_ALU_OP(SUB,BIOP)
A_IMPLEMENT_ALU_OP(RSB,BIOP)
A_IMPLEMENT_ALU_OP(ADD,BIOP)
A_IMPLEMENT_ALU_OP(ADC,BIOP)
A_IMPLEMENT_ALU_OP(SBC,BIOP)
A_IMPLEMENT_ALU_OP(RSC,BIOP)
A_IMPLEMENT_ALU_OP(ORR,BIOP)
A_IMPLEMENT_ALU_OP(MOV,MONOOP)
A_IMPLEMENT_ALU_OP(BIC,BIOP)
A_IMPLEMENT_ALU_OP(MVN,MONOOP)

const u32 A_MOV_REG_LSL_IMM_DBG = A_MOV_REG_LSL_IMM;

#define A_IMPLEMENT_ALU_TEST(x) \
    const u32 A_##x##_IMM = A_Read16 | A_Read0 | ak(ak_##x##_IMM); \
    const u32 A_##x##_REG_LSL_IMM = A_Read16 | A_Read0 | ak(ak_##x##_REG_LSL_IMM); \
    const u32 A_##x##_REG_LSR_IMM = A_Read16 | A_Read0 | ak(ak_##x##_REG_LSR_IMM); \
    const u32 A_##x##_REG_ASR_IMM = A_Read16 | A_Read0 | ak(ak_##x##_REG_ASR_IMM); \
    const u32 A_##x##_REG_ROR_IMM = A_Read16 | A_Read0 | ak(ak_##x##_REG_ROR_IMM); \
    const u32 A_##x##_REG_LSL_REG = A_Read16 | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSL_REG); \
    const u32 A_##x##_REG_LSR_REG = A_Read16 | A_Read0 | A_Read8 | ak(ak_##x##_REG_LSR_REG); \
    const u32 A_##x##_REG_ASR_REG = A_Read16 | A_Read0 | A_Read8 | ak(ak_##x##_REG_ASR_REG); \
    const u32 A_##x##_REG_ROR_REG = A_Read16 | A_Read0 | A_Read8 | ak(ak_##x##_REG_ROR_REG);

A_IMPLEMENT_ALU_TEST(TST)
A_IMPLEMENT_ALU_TEST(TEQ)
A_IMPLEMENT_ALU_TEST(CMP)
A_IMPLEMENT_ALU_TEST(CMN)

const u32 A_MUL = A_Write16 | A_Read0 | A_Read8 | ak(ak_MUL);
const u32 A_MLA = A_Write16 | A_Read0 | A_Read8 | A_Read12 | ak(ak_MLA);
const u32 A_UMULL = A_Write16 | A_Write12 | A_Read0 | A_Read8 | ak(ak_UMULL);
const u32 A_UMLAL = A_Write16 | A_Write12 | A_Read16 | A_Read12 | A_Read0 | A_Read8 | ak(ak_UMLAL);
const u32 A_SMULL = A_Write16 | A_Write12 | A_Read0 | A_Read8 | ak(ak_SMULL);
const u32 A_SMLAL = A_Write16 | A_Write12 | A_Read16 | A_Read12 | A_Read0 | A_Read8 | ak(ak_SMLAL);
const u32 A_SMLAxy = A_Write16 | A_Read0 | A_Read8 | A_Read12 | ak(ak_SMLALxy);
const u32 A_SMLAWy = A_Write16 | A_Read0 | A_Read8 | A_Read12 | ak(ak_SMLAWy);
const u32 A_SMULWy = A_Write16 | A_Read0 | A_Read8 | ak(ak_SMULWy);
const u32 A_SMLALxy = A_Write16 | A_Write12 | A_Read16 | A_Read12 | A_Read0 | A_Read8 | ak(ak_SMLALxy);
const u32 A_SMULxy = A_Write16 | A_Read0 | A_Read8 | ak(ak_SMULxy);

const u32 A_CLZ = A_Write12 | A_Read0 | A_ARM9Only | ak(ak_CLZ);

const u32 A_QADD = A_Write12 | A_Read0 | A_Read16 | A_ARM9Only | ak(ak_QADD);
const u32 A_QSUB = A_Write12 | A_Read0 | A_Read16 | A_ARM9Only | ak(ak_QSUB);
const u32 A_QDADD = A_Write12 | A_Read0 | A_Read16 | A_ARM9Only | ak(ak_QDADD);
const u32 A_QDSUB = A_Write12 | A_Read0 | A_Read16 | A_ARM9Only | ak(ak_QDSUB);

#define A_LDR A_Write12
#define A_STR A_Read12

#define A_IMPLEMENT_WB_LDRSTR(x,k) \
    const u32 A_##x##_IMM = A_##k | A_Read16 | A_MemWriteback | ak(ak_##x##_IMM); \
    const u32 A_##x##_REG_LSL = A_##k | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG_LSL); \
    const u32 A_##x##_REG_LSR = A_##k | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG_LSR); \
    const u32 A_##x##_REG_ASR = A_##k | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG_ASR); \
    const u32 A_##x##_REG_ROR = A_##k | A_Read16 | A_MemWriteback | A_Read0 | ak(ak_##x##_REG_ROR); \
    \
    const u32 A_##x##_POST_IMM = A_##k | A_Read16 | A_Write16 | ak(ak_##x##_POST_IMM); \
    const u32 A_##x##_POST_REG_LSL = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG_LSL); \
    const u32 A_##x##_POST_REG_LSR = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG_LSR); \
    const u32 A_##x##_POST_REG_ASR = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG_ASR); \
    const u32 A_##x##_POST_REG_ROR = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG_ROR);

A_IMPLEMENT_WB_LDRSTR(STR,STR)
A_IMPLEMENT_WB_LDRSTR(STRB,STR)
A_IMPLEMENT_WB_LDRSTR(LDR,LDR)
A_IMPLEMENT_WB_LDRSTR(LDRB,LDR)

#define A_LDRD A_Write12Double
#define A_STRD A_Read12Double

#define A_IMPLEMENT_HD_LDRSTR(x,k) \
    const u32 A_##x##_IMM = A_##k | A_Read16 | A_Write16 | ak(ak_##x##_IMM); \
    const u32 A_##x##_REG = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_REG); \
    const u32 A_##x##_POST_IMM = A_##k | A_Read16 | A_Write16 | ak(ak_##x##_POST_IMM); \
    const u32 A_##x##_POST_REG = A_##k | A_Read16 | A_Write16 | A_Read0 | ak(ak_##x##_POST_REG);

A_IMPLEMENT_HD_LDRSTR(STRH,STR)
A_IMPLEMENT_HD_LDRSTR(LDRD,LDRD)
A_IMPLEMENT_HD_LDRSTR(STRD,STRD)
A_IMPLEMENT_HD_LDRSTR(LDRH,LDR)
A_IMPLEMENT_HD_LDRSTR(LDRSB,LDR)
A_IMPLEMENT_HD_LDRSTR(LDRSH,LDR)

const u32 A_SWP = A_Write12 | A_Read16 | A_Read0 | ak(ak_SWP);
const u32 A_SWPB = A_Write12 | A_Read16 | A_Read0 | ak(ak_SWPB);

const u32 A_LDM = A_Read16 | A_LDMSTM | ak(ak_LDM);
const u32 A_STM = A_Read16 | A_LDMSTM | ak(ak_STM);

const u32 A_B = A_BranchAlways | ak(ak_B);
const u32 A_BL = A_BranchAlways | A_Link | ak(ak_BL);
const u32 A_BLX_IMM = A_BranchAlways | A_Link | ak(ak_BLX_IMM);
const u32 A_BX = A_BranchAlways | A_Read0 | ak(ak_BX);
const u32 A_BLX_REG = A_BranchAlways | A_Link | A_Read0 | ak(ak_BLX_REG);

const u32 A_UNK = A_BranchAlways | A_Link | ak(ak_UNK);
const u32 A_MSR_IMM = A_ARM9Only | ak(ak_MSR_IMM);
const u32 A_MSR_REG = A_Read0 | A_ARM9Only | ak(ak_MSR_REG);
const u32 A_MRS = A_Write12 | A_ARM9Only | ak(ak_MRS);
const u32 A_MCR = A_Read12 | A_ARM9Only | ak(ak_MCR);
const u32 A_MRC = A_Write12 | A_ARM9Only | ak(ak_MRC);
const u32 A_SVC = A_BranchAlways | A_Link | ak(ak_SVC);

// THUMB

#define tk(x) ((x) << 16)

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
    T_ReadR15       = 1 << 11,

    T_BranchAlways  = 1 << 12,
    T_ReadR14       = 1 << 13,
    T_WriteR14      = 1 << 14,

    T_PopPC         = 1 << 15
};

const u32 T_LSL_IMM = T_Write0 | T_Read3 | tk(tk_LSL_IMM);
const u32 T_LSR_IMM = T_Write0 | T_Read3 | tk(tk_LSR_IMM);
const u32 T_ASR_IMM = T_Write0 | T_Read3 | tk(tk_ASR_IMM);

const u32 T_ADD_REG_ = T_Write0 | T_Read3 | T_Read6 | tk(tk_ADD_REG_);
const u32 T_SUB_REG_ = T_Write0 | T_Read3 | T_Read6 | tk(tk_SUB_REG_);
const u32 T_ADD_IMM_ = T_Write0 | T_Read3 | tk(tk_ADD_IMM_);
const u32 T_SUB_IMM_ = T_Write0 | T_Read3 | tk(tk_SUB_IMM_);

const u32 T_MOV_IMM = T_Write8 | tk(tk_MOV_IMM);
const u32 T_CMP_IMM = T_Write8 | tk(tk_CMP_IMM);
const u32 T_ADD_IMM = T_Write8 | T_Read8 | tk(tk_ADD_IMM);
const u32 T_SUB_IMM = T_Write8 | T_Read8 | tk(tk_SUB_IMM);

const u32 T_AND_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_AND_REG);
const u32 T_EOR_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_EOR_REG);
const u32 T_LSL_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_LSL_REG);
const u32 T_LSR_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_LSR_REG);
const u32 T_ASR_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_ASR_REG);
const u32 T_ADC_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_ADC_REG);
const u32 T_SBC_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_SBC_REG);
const u32 T_ROR_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_ROR_REG);
const u32 T_TST_REG = T_Read0 | T_Read3 | tk(tk_TST_REG);
const u32 T_NEG_REG = T_Write0 | T_Read3 | tk(tk_NEG_REG);
const u32 T_CMP_REG = T_Read0 | T_Read3 | tk(tk_CMP_REG);
const u32 T_CMN_REG = T_Read0 | T_Read3 | tk(tk_CMN_REG);
const u32 T_ORR_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_ORR_REG);
const u32 T_MUL_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_MUL_REG);
const u32 T_BIC_REG = T_Write0 | T_Read0 | T_Read3 | tk(tk_BIC_REG);
const u32 T_MVN_REG = T_Write0 | T_Read3 | tk(tk_MVN_REG);

const u32 T_ADD_HIREG = T_WriteHi0 | T_ReadHi0 | T_ReadHi3 | tk(tk_ADD_HIREG);
const u32 T_CMP_HIREG = T_ReadHi0 | T_ReadHi3 | tk(tk_CMP_HIREG);
const u32 T_MOV_HIREG = T_WriteHi0 | T_ReadHi3 | tk(tk_MOV_HIREG);

const u32 T_ADD_PCREL = T_Write8 | T_ReadR15 | tk(tk_ADD_PCREL);
const u32 T_ADD_SPREL = T_Write8 | T_ReadR13 | tk(tk_ADD_SPREL);
const u32 T_ADD_SP = T_WriteR13 | tk(tk_ADD_SP);

const u32 T_LDR_PCREL = T_Write8 | tk(tk_LDR_PCREL);

const u32 T_STR_REG = T_Read0 | T_Read3 | T_Read6 | tk(tk_STR_REG);
const u32 T_STRB_REG = T_Read0 | T_Read3 | T_Read6 | tk(tk_STRB_REG);
const u32 T_LDR_REG = T_Write0 | T_Read3 | T_Read6 | tk(tk_LDR_REG);
const u32 T_LDRB_REG = T_Write0 | T_Read3 | T_Read6 | tk(tk_LDRB_REG);
const u32 T_STRH_REG = T_Read0 | T_Read3 | T_Read6 | tk(tk_STRH_REG);
const u32 T_LDRSB_REG = T_Write0 | T_Read3 | T_Read6 | tk(tk_LDRSB_REG);
const u32 T_LDRH_REG = T_Write0 | T_Read3 | T_Read6 | tk(tk_LDRH_REG);
const u32 T_LDRSH_REG = T_Write0 | T_Read3 | T_Read6 | tk(tk_LDRSH_REG);

const u32 T_STR_IMM = T_Read0 | T_Read3 | tk(tk_STR_IMM);
const u32 T_LDR_IMM = T_Write0 | T_Read3 | tk(tk_LDR_IMM);
const u32 T_STRB_IMM = T_Read0 | T_Read3 | tk(tk_STRB_IMM);
const u32 T_LDRB_IMM = T_Write0 | T_Read3 | tk(tk_LDRB_IMM);
const u32 T_STRH_IMM = T_Read0 | T_Read3 | tk(tk_STRH_IMM);
const u32 T_LDRH_IMM = T_Write0 | T_Read3 | tk(tk_LDRH_IMM);

const u32 T_STR_SPREL = T_Read8 | T_ReadR13 | tk(tk_STR_SPREL);
const u32 T_LDR_SPREL = T_Write8 | T_ReadR13 | tk(tk_LDR_SPREL);

const u32 T_PUSH = T_ReadR15 | T_ReadR13 | T_WriteR13 | tk(tk_PUSH);
const u32 T_POP = T_PopPC | T_ReadR13 | T_WriteR13 | tk(tk_POP);

const u32 T_LDMIA = T_Read8 | T_Write8 | tk(tk_LDMIA);
const u32 T_STMIA = T_Read8 | T_Write8 | tk(tk_STMIA);

const u32 T_BCOND = T_BranchAlways | tk(tk_BCOND);
const u32 T_BX = T_BranchAlways | T_ReadHi3 | tk(tk_BX);
const u32 T_BLX_REG = T_BranchAlways | T_ReadR15 | T_WriteR14 | T_ReadHi3 | tk(tk_BLX_REG);
const u32 T_B = T_BranchAlways | tk(tk_B);
const u32 T_BL_LONG_1 = T_WriteR14 | T_ReadR15 | tk(tk_BL_LONG_1);
const u32 T_BL_LONG_2 = T_BranchAlways | T_ReadR14 | T_WriteR14 | T_ReadR15 | tk(tk_BL_LONG_2);

const u32 T_UNK = T_BranchAlways | T_WriteR14 | tk(tk_UNK);
const u32 T_SVC = T_BranchAlways | T_WriteR14 | T_ReadR15 | tk(tk_SVC);

#define INSTRFUNC_PROTO(x) u32 x
#include "ARM_InstrTable.h"
#undef INSTRFUNC_PROTO

Info Decode(bool thumb, u32 num, u32 instr)
{
    Info res = {0};
    if (thumb)
    {
        u32 data = THUMBInstrTable[(instr >> 6) & 0x3FF];

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
        if (data & T_ReadR15)
            res.SrcRegs |= (1 << 15);

        if (data & T_BranchAlways)
            res.DstRegs |= (1 << 15);

        if (data & T_PopPC && instr & (1 << 8))
            res.DstRegs |= 1 << 15;

        res.Kind = (data >> 16) & 0x3F;

        return res;
    }
    else
    {
        u32 data = ARMInstrTable[((instr >> 4) & 0xF) | ((instr >> 16) & 0xFF0)];
        if ((instr & 0xFE000000) == 0xFA000000)
            data = A_BLX_IMM;

        if (data & A_ARM9Only && num != 0)
            data |= A_BranchAlways | A_Link;

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
        {
            res.DstRegs |= 1 << 14;
            res.SrcRegs |= 1 << 15;
        }

        if (data & A_LDMSTM)
        {
            res.DstRegs |= instr & (!!(instr & (1 << 20)) << 15);
            if (instr & (1 << 21))
                res.DstRegs |= 1 << ((instr >> 16) & 0xF);
        }

        res.Kind = (data >> 13) & 0x1FF;

        return res;
    }
}

}
