#include <stdio.h>
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
    u32 val = ROR(cpu->Read32(offset), ((offset&0x3)<<3)); \
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
    u32 val = ROR(cpu->Read32(addr, cpu->CurInstr & (1<<21)), ((addr&0x3)<<3)); \
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
    return C_N(2) + cpu->MemWaitstate(2, offset);

#define A_STRH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->Write16(addr, cpu->R[(cpu->CurInstr>>12) & 0xF]); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    return C_N(2) + cpu->MemWaitstate(2, addr);

// TODO: CHECK LDRD/STRD TIMINGS!!

#define A_LDRD \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    cpu->R[r  ] = cpu->Read32(offset  ); \
    cpu->R[r+1] = cpu->Read32(offset+4); \
    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, offset);

#define A_LDRD_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    cpu->R[r  ] = cpu->Read32(addr  ); \
    cpu->R[r+1] = cpu->Read32(addr+4); \
    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, addr);

#define A_STRD \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    if (cpu->CurInstr & (1<<24)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    cpu->Write32(offset  , cpu->R[r  ]); \
    cpu->Write32(offset+4, cpu->R[r+1]); \
    return C_N(2) + cpu->MemWaitstate(3, offset);

#define A_STRD_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    cpu->Write32(offset  , cpu->R[r  ]); \
    cpu->Write32(offset+4, cpu->R[r+1]); \
    return C_N(2) + cpu->MemWaitstate(3, addr);

#define A_LDRH \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = cpu->Read16(offset); \
    if (cpu->CurInstr & (1<<24)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    return C_N(2) + cpu->MemWaitstate(2, offset);

#define A_LDRH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = cpu->Read16(addr); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    return C_N(2) + cpu->MemWaitstate(2, addr);

#define A_LDRSB \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = (s32)(s8)cpu->Read8(offset); \
    if (cpu->CurInstr & (1<<24)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    return C_N(2) + cpu->MemWaitstate(3, offset);

#define A_LDRSB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = (s32)(s8)cpu->Read8(addr); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    return C_N(2) + cpu->MemWaitstate(3, addr);

#define A_LDRSH \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = (s32)(s16)cpu->Read16(offset); \
    if (cpu->CurInstr & (1<<24)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    return C_N(2) + cpu->MemWaitstate(2, offset);

#define A_LDRSH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = (s32)(s16)cpu->Read16(addr); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    return C_N(2) + cpu->MemWaitstate(2, addr);


#define A_IMPLEMENT_HD_LDRSTR(x) \
\
s32 A_##x##_IMM(ARM* cpu) \
{ \
    A_HD_CALC_OFFSET_IMM \
    A_##x \
} \
\
s32 A_##x##_REG(ARM* cpu) \
{ \
    A_HD_CALC_OFFSET_REG \
    A_##x \
} \
s32 A_##x##_POST_IMM(ARM* cpu) \
{ \
    A_HD_CALC_OFFSET_IMM \
    A_##x##_POST \
} \
\
s32 A_##x##_POST_REG(ARM* cpu) \
{ \
    A_HD_CALC_OFFSET_REG \
    A_##x##_POST \
}

A_IMPLEMENT_HD_LDRSTR(STRH)
A_IMPLEMENT_HD_LDRSTR(LDRD)
A_IMPLEMENT_HD_LDRSTR(STRD)
A_IMPLEMENT_HD_LDRSTR(LDRH)
A_IMPLEMENT_HD_LDRSTR(LDRSB)
A_IMPLEMENT_HD_LDRSTR(LDRSH)



s32 A_LDM(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 16) & 0xF];
    u32 preinc = (cpu->CurInstr & (1<<24));

    if (!(cpu->CurInstr & (1<<23)))
    {
        for (int i = 0; i < 16; i++)
        {
            if (cpu->CurInstr & (1<<i))
                base -= 4;
        }

        if (cpu->CurInstr & (1<<21))
        {
            cpu->R[(cpu->CurInstr >> 16) & 0xF] = base;
            if (cpu->CurInstr & (1 << ((cpu->CurInstr >> 16) & 0xF)))
                printf("!! BAD LDM\n");
        }

        preinc = !preinc;
    }

    s32 cycles = C_N(1) + C_I(1);

    if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
        cpu->UpdateMode(cpu->CPSR, (cpu->CPSR&~0x1F)|0x10);

    for (int i = 0; i < 15; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (preinc) base += 4;
            cpu->R[i] = cpu->Read32(base);
            cycles += C_S(1) + cpu->MemWaitstate(3, base);
            if (!preinc) base += 4;
        }
    }

    if (cpu->CurInstr & (1<<15))
    {
        if (preinc) base += 4;
        u32 pc = cpu->Read32(base);
        cycles += C_S(2) + C_N(1) + cpu->MemWaitstate(3, base);
        if (!preinc) base += 4;

        if (cpu->Num == 1)
            pc &= ~0x1;

        cpu->JumpTo(pc);
        if (cpu->CurInstr & (1<<22)) cpu->RestoreCPSR();
    }

    if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
        cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR);

    if ((cpu->CurInstr & (1<<23)) && (cpu->CurInstr & (1<<21)))
    {
        cpu->R[(cpu->CurInstr >> 16) & 0xF] = base;
        if (cpu->CurInstr & (1 << ((cpu->CurInstr >> 16) & 0xF)))
            printf("!! BAD LDM\n");
    }

    return cycles;
}

s32 A_STM(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 16) & 0xF];
    u32 preinc = (cpu->CurInstr & (1<<24));

    if (!(cpu->CurInstr & (1<<23)))
    {
        for (int i = 0; i < 16; i++)
        {
            if (cpu->CurInstr & (1<<i))
                base -= 4;
        }

        if (cpu->CurInstr & (1<<21))
        {
            cpu->R[(cpu->CurInstr >> 16) & 0xF] = base;
            if (cpu->CurInstr & (1 << ((cpu->CurInstr >> 16) & 0xF)))
                printf("!! BAD STM\n");
        }

        preinc = !preinc;
    }

    s32 cycles = C_N(1) + C_I(1);

    if (cpu->CurInstr & (1<<22))
        cpu->UpdateMode(cpu->CPSR, (cpu->CPSR&~0x1F)|0x10);

    for (int i = 0; i < 16; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (preinc) base += 4;
            cpu->Write32(base, cpu->R[i]);
            cycles += C_S(1) + cpu->MemWaitstate(3, base);
            if (!preinc) base += 4;
        }
    }

    if (cpu->CurInstr & (1<<22))
        cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR);

    if ((cpu->CurInstr & (1<<23)) && (cpu->CurInstr & (1<<21)))
    {
        cpu->R[(cpu->CurInstr >> 16) & 0xF] = base;
        if (cpu->CurInstr & (1 << ((cpu->CurInstr >> 16) & 0xF)))
            printf("!! BAD STM\n");
    }

    return cycles;
}




// ---- THUMB -----------------------



s32 T_LDR_PCREL(ARM* cpu)
{
    u32 addr = cpu->R[15] + ((cpu->CurInstr & 0xFF) << 2);
    cpu->R[(cpu->CurInstr >> 8) & 0x7] = cpu->Read32(addr);

    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, addr);
}


s32 T_STR_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->Write32(addr, cpu->R[cpu->CurInstr & 0x7]);

    return C_N(2) + cpu->MemWaitstate(3, addr);
}

s32 T_STRB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->Write8(addr, cpu->R[cpu->CurInstr & 0x7]);

    return C_N(2) + cpu->MemWaitstate(3, addr);
}

s32 T_LDR_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = cpu->Read32(addr);

    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, addr);
}

s32 T_LDRB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = cpu->Read8(addr);

    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, addr);
}


s32 T_STRH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->Write16(addr, cpu->R[cpu->CurInstr & 0x7]);

    return C_N(2) + cpu->MemWaitstate(2, addr);
}

s32 T_LDRSB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = (s32)(s8)cpu->Read8(addr);

    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, addr);
}

s32 T_LDRH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = cpu->Read16(addr);

    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(2, addr);
}

s32 T_LDRSH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = (s32)(s16)cpu->Read16(addr);

    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(2, addr);
}


s32 T_STR_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 4) & 0x7C;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->Write32(offset, cpu->R[cpu->CurInstr & 0x7]);
    return C_N(2) + cpu->MemWaitstate(3, offset);
}

s32 T_LDR_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 4) & 0x7C;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->R[cpu->CurInstr & 0x7] = cpu->Read32(offset);
    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, offset);
}

s32 T_STRB_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 6) & 0x1F;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->Write8(offset, cpu->R[cpu->CurInstr & 0x7]);
    return C_N(2) + cpu->MemWaitstate(3, offset);
}

s32 T_LDRB_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 6) & 0x1F;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->R[cpu->CurInstr & 0x7] = cpu->Read8(offset);
    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, offset);
}


s32 T_STRH_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 5) & 0x3E;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->Write16(offset, cpu->R[cpu->CurInstr & 0x7]);
    return C_N(2) + cpu->MemWaitstate(2, offset);
}

s32 T_LDRH_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 5) & 0x3E;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->R[cpu->CurInstr & 0x7] = cpu->Read16(offset);
    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(2, offset);
}


s32 T_STR_SPREL(ARM* cpu)
{
    u32 offset = (cpu->CurInstr << 2) & 0x3FC;
    offset += cpu->R[13];

    cpu->Write32(offset, cpu->R[(cpu->CurInstr >> 8) & 0x7]);
    return C_N(2) + cpu->MemWaitstate(3, offset);
}

s32 T_LDR_SPREL(ARM* cpu)
{
    u32 offset = (cpu->CurInstr << 2) & 0x3FC;
    offset += cpu->R[13];

    cpu->R[(cpu->CurInstr >> 8) & 0x7] = cpu->Read32(offset);
    return C_S(1) + C_N(1) + C_I(1) + cpu->MemWaitstate(3, offset);
}


s32 T_PUSH(ARM* cpu)
{
    int nregs = 0;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
            nregs++;
    }

    if (cpu->CurInstr & (1<<8))
        nregs++;

    u32 base = cpu->R[13];
    base -= (nregs<<2);
    cpu->R[13] = base;

    int cycles = C_N(2);

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            cpu->Write32(base, cpu->R[i]);
            cycles += C_S(1) + cpu->MemWaitstate(3, base);
            base += 4;
        }
    }

    if (cpu->CurInstr & (1<<8))
    {
        cpu->Write32(base, cpu->R[14]);
        cycles += C_S(1) + cpu->MemWaitstate(3, base);
    }

    return cycles - C_S(1);
}

s32 T_POP(ARM* cpu)
{
    u32 base = cpu->R[13];

    int cycles = C_N(1) + C_I(1);

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            cpu->R[i] = cpu->Read32(base);
            cycles += C_S(1) + cpu->MemWaitstate(3, base);
            base += 4;
        }
    }

    if (cpu->CurInstr & (1<<8))
    {
        u32 pc = cpu->Read32(base);
        if (cpu->Num==1) pc |= 0x1;
        cpu->JumpTo(pc);
        cycles += C_S(2) + C_N(1) + cpu->MemWaitstate(3, base);
        base += 4;
    }

    cpu->R[13] = base;

    return cycles;
}

s32 T_STMIA(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 8) & 0x7];

    int cycles = C_N(2);

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            cpu->Write32(base, cpu->R[i]);
            cycles += C_S(1) + cpu->MemWaitstate(3, base);
            base += 4;
        }
    }

    cpu->R[(cpu->CurInstr >> 8) & 0x7] = base;

    return cycles - C_S(1);
}

s32 T_LDMIA(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 8) & 0x7];

    int cycles = C_N(1) + C_I(1);

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            cpu->R[i] = cpu->Read32(base);
            cycles += C_S(1) + cpu->MemWaitstate(3, base);
            base += 4;
        }
    }

    cpu->R[(cpu->CurInstr >> 8) & 0x7] = base;

    return cycles;
}


}

