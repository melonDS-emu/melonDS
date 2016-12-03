#include "ARM.h"


namespace ARMInterpreter
{


// copypasta from ALU. bad
#define LSL_IMM(x, s) \
    x <<= s;

#define LSR_IMM(x, s) \
    if (s == 0) s = 32; \
    x >>= s;

#define ASR_IMM(x, s) \
    if (s == 0) s = 32; \
    x = ((s32)x) >> s;

#define ROR_IMM(x, s) \
    if (s == 0) \
    { \
        x = (x >> 1) | ((cpu->CPSR & 0x20000000) << 2); \
    } \
    else \
    { \
        x = ROR(x, s); \
    }



#define A_WB_CALC_OFFSET_IMM \
    u32 offset = (cpu->CurInstr & 0xFFF); \
    if (!(cpu->CurInstr & (1<<23))) offset = -offset;

#define A_WB_CALC_OFFSET_REG(shiftop) \
    u32 offset = cpu->R[cpu->CurInstr & 0xF]; \
    u32 shift = ((cpu->CurInstr>>7)&0x1F); \
    shiftop(offset, shift); \
    if (!(cpu->CurInstr & (1<<23))) offset = -offset;



#define A_STR \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->Write32(offset, cpu->R[(cpu->CurInstr>>12) & 0xF]); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    return C_N(2) + cpu->MemWaitstate(3, offset);

#define A_STR_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->Write32(addr, cpu->R[(cpu->CurInstr>>12) & 0xF], cpu->CurInstr & (1<<21)); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    return C_N(2) + cpu->MemWaitstate(3, addr);

#define A_STRB \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->Write8(offset, cpu->R[(cpu->CurInstr>>12) & 0xF]); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    return C_N(2) + cpu->MemWaitstate(3, offset);

#define A_STRB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->Write8(addr, cpu->R[(cpu->CurInstr>>12) & 0xF], cpu->CurInstr & (1<<21)); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    return C_N(2) + cpu->MemWaitstate(3, addr);

#define A_LDR \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val = ROR(cpu->Read32(offset), offset&0x3); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        if (cpu->Num==1) val &= ~0x1; \
        cpu->JumpTo(val); \
        return C_S(2) + C_N(2) + C_I(1) + cpu->MemWaitstate(3, offset); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
        return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, offset); \
    }

#define A_LDR_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val = ROR(cpu->Read32(addr, cpu->CurInstr & (1<<21)), addr&0x3); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        if (cpu->Num==1) val &= ~0x1; \
        cpu->JumpTo(val); \
        return C_S(2) + C_N(2) + C_I(1) + cpu->MemWaitstate(3, addr); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
        return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, addr); \
    }

#define A_LDRB \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val = cpu->Read8(offset); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, offset);

#define A_LDRB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val = cpu->Read8(addr, cpu->CurInstr & (1<<21)); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, addr);



#define A_IMPLEMENT_WB_LDRSTR(x) \
\
s32 A_##x##_IMM(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_IMM \
    A_##x \
} \
\
s32 A_##x##_REG_LSL(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(LSL_IMM) \
    A_##x \
} \
\
s32 A_##x##_REG_LSR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(LSR_IMM) \
    A_##x \
} \
\
s32 A_##x##_REG_ASR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(ASR_IMM) \
    A_##x \
} \
\
s32 A_##x##_REG_ROR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(ROR_IMM) \
    A_##x \
} \
\
s32 A_##x##_POST_IMM(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_IMM \
    A_##x##_POST \
} \
\
s32 A_##x##_POST_REG_LSL(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(LSL_IMM) \
    A_##x##_POST \
} \
\
s32 A_##x##_POST_REG_LSR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(LSR_IMM) \
    A_##x##_POST \
} \
\
s32 A_##x##_POST_REG_ASR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(ASR_IMM) \
    A_##x##_POST \
} \
\
s32 A_##x##_POST_REG_ROR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(ROR_IMM) \
    A_##x##_POST \
}

A_IMPLEMENT_WB_LDRSTR(STR)
A_IMPLEMENT_WB_LDRSTR(STRB)
A_IMPLEMENT_WB_LDRSTR(LDR)
A_IMPLEMENT_WB_LDRSTR(LDRB)



#define A_HD_CALC_OFFSET_IMM \
    u32 offset = (cpu->CurInstr & 0xF) | ((cpu->CurInstr >> 4) & 0xF0); \
    if (!(cpu->CurInstr & (1<<23))) offset = -offset;

#define A_HD_CALC_OFFSET_REG \
    u32 offset = cpu->R[cpu->CurInstr & 0xF]; \
    if (!(cpu->CurInstr & (1<<23))) offset = -offset;



#define A_STRH \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->Write16(offset, cpu->R[(cpu->CurInstr>>12) & 0xF]); \
    if (cpu->CurInstr & (1<<24)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \


}

