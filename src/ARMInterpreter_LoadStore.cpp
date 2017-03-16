/*
    Copyright 2016-2017 StapleButter

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

#include <stdio.h>
#include "ARM.h"


namespace ARMInterpreter
{


// copypasta from ALU. bad
#define LSL_IMM(x, s) \
    x <<= s;

#define LSR_IMM(x, s) \
    if (s == 0) x = 0; \
    else        x >>= s;

#define ASR_IMM(x, s) \
    if (s == 0) x = ((s32)x) >> 31; \
    else        x = ((s32)x) >> s;

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
    cpu->DataWrite32(offset, cpu->R[(cpu->CurInstr>>12) & 0xF]); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_STR_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->DataWrite32(addr, cpu->R[(cpu->CurInstr>>12) & 0xF], cpu->CurInstr & (1<<21)); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_STRB \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->DataWrite8(offset, cpu->R[(cpu->CurInstr>>12) & 0xF]); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_STRB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->DataWrite8(addr, cpu->R[(cpu->CurInstr>>12) & 0xF], cpu->CurInstr & (1<<21)); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_LDR \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val = cpu->DataRead32(offset); val = ROR(val, ((offset&0x3)<<3)); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    cpu->Cycles += 1; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        if (cpu->Num==1) val &= ~0x1; \
        cpu->JumpTo(val); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    }

#define A_LDR_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val = cpu->DataRead32(addr, cpu->CurInstr & (1<<21)); val = ROR(val, ((addr&0x3)<<3)); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    cpu->Cycles += 1; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        if (cpu->Num==1) val &= ~0x1; \
        cpu->JumpTo(val); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    }

#define A_LDRB \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val = cpu->DataRead8(offset); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    cpu->Cycles += 1; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) printf("!! LDRB PC %08X\n", cpu->R[15]); \

#define A_LDRB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val = cpu->DataRead8(addr, cpu->CurInstr & (1<<21)); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    cpu->Cycles += 1; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) printf("!! LDRB PC %08X\n", cpu->R[15]); \



#define A_IMPLEMENT_WB_LDRSTR(x) \
\
void A_##x##_IMM(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_IMM \
    A_##x \
} \
\
void A_##x##_REG_LSL(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(LSL_IMM) \
    A_##x \
} \
\
void A_##x##_REG_LSR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(LSR_IMM) \
    A_##x \
} \
\
void A_##x##_REG_ASR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(ASR_IMM) \
    A_##x \
} \
\
void A_##x##_REG_ROR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(ROR_IMM) \
    A_##x \
} \
\
void A_##x##_POST_IMM(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_IMM \
    A_##x##_POST \
} \
\
void A_##x##_POST_REG_LSL(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(LSL_IMM) \
    A_##x##_POST \
} \
\
void A_##x##_POST_REG_LSR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(LSR_IMM) \
    A_##x##_POST \
} \
\
void A_##x##_POST_REG_ASR(ARM* cpu) \
{ \
    A_WB_CALC_OFFSET_REG(ASR_IMM) \
    A_##x##_POST \
} \
\
void A_##x##_POST_REG_ROR(ARM* cpu) \
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
    cpu->DataWrite16(offset, cpu->R[(cpu->CurInstr>>12) & 0xF]); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \

#define A_STRH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->DataWrite16(addr, cpu->R[(cpu->CurInstr>>12) & 0xF]); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \

// TODO: CHECK LDRD/STRD TIMINGS!! also, ARM9-only

#define A_LDRD \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    cpu->Cycles += 1; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    cpu->R[r  ] = cpu->DataRead32(offset  ); \
    cpu->R[r+1] = cpu->DataRead32(offset+4); \

#define A_LDRD_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    cpu->Cycles += 1; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    cpu->R[r  ] = cpu->DataRead32(addr  ); \
    cpu->R[r+1] = cpu->DataRead32(addr+4); \

#define A_STRD \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    cpu->DataWrite32(offset  , cpu->R[r  ]); \
    cpu->DataWrite32(offset+4, cpu->R[r+1]); \

#define A_STRD_POST \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    cpu->DataWrite32(offset  , cpu->R[r  ]); \
    cpu->DataWrite32(offset+4, cpu->R[r+1]); \

#define A_LDRH \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = cpu->DataRead16(offset); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) printf("!! LDRH PC %08X\n", cpu->R[15]); \

#define A_LDRH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = cpu->DataRead16(addr); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) printf("!! LDRH PC %08X\n", cpu->R[15]); \

#define A_LDRSB \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = (s32)(s8)cpu->DataRead8(offset); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) printf("!! LDRSB PC %08X\n", cpu->R[15]); \

#define A_LDRSB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = (s32)(s8)cpu->DataRead8(addr); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) printf("!! LDRSB PC %08X\n", cpu->R[15]); \

#define A_LDRSH \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = (s32)(s16)cpu->DataRead16(offset); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) printf("!! LDRSH PC %08X\n", cpu->R[15]); \

#define A_LDRSH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    cpu->R[(cpu->CurInstr>>12) & 0xF] = (s32)(s16)cpu->DataRead16(addr); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) printf("!! LDRSH PC %08X\n", cpu->R[15]); \


#define A_IMPLEMENT_HD_LDRSTR(x) \
\
void A_##x##_IMM(ARM* cpu) \
{ \
    A_HD_CALC_OFFSET_IMM \
    A_##x \
} \
\
void A_##x##_REG(ARM* cpu) \
{ \
    A_HD_CALC_OFFSET_REG \
    A_##x \
} \
void A_##x##_POST_IMM(ARM* cpu) \
{ \
    A_HD_CALC_OFFSET_IMM \
    A_##x##_POST \
} \
\
void A_##x##_POST_REG(ARM* cpu) \
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



void A_SWP(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 16) & 0xF];
    u32 rm = cpu->R[cpu->CurInstr & 0xF];

    u32 val = cpu->DataRead32(base);
    cpu->R[(cpu->CurInstr >> 12) & 0xF] = ROR(val, 8*(base&0x3));

    cpu->DataWrite32(base, rm);

    cpu->Cycles += 1;
}

void A_SWPB(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 16) & 0xF];
    u32 rm = cpu->R[cpu->CurInstr & 0xF] & 0xFF;

    cpu->R[(cpu->CurInstr >> 12) & 0xF] = cpu->DataRead8(base);

    cpu->DataWrite8(base, rm);

    cpu->Cycles += 1;
}



void A_LDM(ARM* cpu)
{
    u32 baseid = (cpu->CurInstr >> 16) & 0xF;
    u32 base = cpu->R[baseid];
    u32 wbbase;
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
            // pre writeback
            wbbase = base;
        }

        preinc = !preinc;
    }

    cpu->Cycles += 1;

    if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
        cpu->UpdateMode(cpu->CPSR, (cpu->CPSR&~0x1F)|0x10);

    for (int i = 0; i < 15; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (preinc) base += 4;
            cpu->R[i] = cpu->DataRead32(base);
            if (!preinc) base += 4;
        }
    }

    if (cpu->CurInstr & (1<<15))
    {
        if (preinc) base += 4;
        u32 pc = cpu->DataRead32(base);
        if (!preinc) base += 4;

        if (cpu->Num == 1)
            pc &= ~0x1;

        cpu->JumpTo(pc, cpu->CurInstr & (1<<22));
    }

    if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
        cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR);

    if (cpu->CurInstr & (1<<21))
    {
        // post writeback
        if (cpu->CurInstr & (1<<23))
            wbbase = base;

        if (cpu->CurInstr & (1 << baseid))
        {
            if (cpu->Num == 0)
            {
                u32 rlist = cpu->CurInstr & 0xFFFF;
                if ((!(rlist & ~(1 << baseid))) || (rlist & ~((2 << baseid) - 1)))
                    cpu->R[baseid] = wbbase;
            }
        }
        else
            cpu->R[baseid] = wbbase;
    }
}

void A_STM(ARM* cpu)
{
    u32 baseid = (cpu->CurInstr >> 16) & 0xF;
    u32 base = cpu->R[baseid];
    u32 oldbase = base;
    u32 preinc = (cpu->CurInstr & (1<<24));

    if (!(cpu->CurInstr & (1<<23)))
    {
        for (u32 i = 0; i < 16; i++)
        {
            if (cpu->CurInstr & (1<<i))
                base -= 4;
        }

        if (cpu->CurInstr & (1<<21))
            cpu->R[baseid] = base;

        preinc = !preinc;
    }

    bool isbanked = false;
    if (cpu->CurInstr & (1<<22))
    {
        u32 mode = (cpu->CPSR & 0x1F);
        if (mode == 0x11)
            isbanked = (baseid >= 8 && baseid < 15);
        else if (mode != 0x10 && mode != 0x1F)
            isbanked = (baseid >= 13 && baseid < 15);

        cpu->UpdateMode(cpu->CPSR, (cpu->CPSR&~0x1F)|0x10);
    }

    for (u32 i = 0; i < 16; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (preinc) base += 4;

            if (i == baseid && !isbanked)
            {
                if ((cpu->Num == 0) || (!(cpu->CurInstr & (i-1))))
                    cpu->DataWrite32(base, oldbase);
                else
                    cpu->DataWrite32(base, base); // checkme
            }
            else
                cpu->DataWrite32(base, cpu->R[i]);

            if (!preinc) base += 4;
        }
    }

    if (cpu->CurInstr & (1<<22))
        cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR);

    if ((cpu->CurInstr & (1<<23)) && (cpu->CurInstr & (1<<21)))
        cpu->R[baseid] = base;
}




// ---- THUMB -----------------------



void T_LDR_PCREL(ARM* cpu)
{
    u32 addr = (cpu->R[15] & ~0x2) + ((cpu->CurInstr & 0xFF) << 2);
    cpu->R[(cpu->CurInstr >> 8) & 0x7] = cpu->DataRead32(addr);

    cpu->Cycles += 1;
}


void T_STR_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->DataWrite32(addr, cpu->R[cpu->CurInstr & 0x7]);
}

void T_STRB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->DataWrite8(addr, cpu->R[cpu->CurInstr & 0x7]);
}

void T_LDR_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];

    u32 val = cpu->DataRead32(addr);
    cpu->R[cpu->CurInstr & 0x7] = ROR(val, 8*(addr&0x3));

    cpu->Cycles += 1;
}

void T_LDRB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = cpu->DataRead8(addr);

    cpu->Cycles += 1;
}


void T_STRH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->DataWrite16(addr, cpu->R[cpu->CurInstr & 0x7]);
}

void T_LDRSB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = (s32)(s8)cpu->DataRead8(addr);

    cpu->Cycles += 1;
}

void T_LDRH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = cpu->DataRead16(addr);

    cpu->Cycles += 1;
}

void T_LDRSH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->R[cpu->CurInstr & 0x7] = (s32)(s16)cpu->DataRead16(addr);

    cpu->Cycles += 1;
}


void T_STR_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 4) & 0x7C;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->DataWrite32(offset, cpu->R[cpu->CurInstr & 0x7]);
}

void T_LDR_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 4) & 0x7C;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    u32 val = cpu->DataRead32(offset);
    cpu->R[cpu->CurInstr & 0x7] = ROR(val, 8*(offset&0x3));
    cpu->Cycles += 1;
}

void T_STRB_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 6) & 0x1F;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->DataWrite8(offset, cpu->R[cpu->CurInstr & 0x7]);
}

void T_LDRB_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 6) & 0x1F;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->R[cpu->CurInstr & 0x7] = cpu->DataRead8(offset);
    cpu->Cycles += 1;
}


void T_STRH_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 5) & 0x3E;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->DataWrite16(offset, cpu->R[cpu->CurInstr & 0x7]);
}

void T_LDRH_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 5) & 0x3E;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->R[cpu->CurInstr & 0x7] = cpu->DataRead16(offset);
    cpu->Cycles += 1;
}


void T_STR_SPREL(ARM* cpu)
{
    u32 offset = (cpu->CurInstr << 2) & 0x3FC;
    offset += cpu->R[13];

    cpu->DataWrite32(offset, cpu->R[(cpu->CurInstr >> 8) & 0x7]);
}

void T_LDR_SPREL(ARM* cpu)
{
    u32 offset = (cpu->CurInstr << 2) & 0x3FC;
    offset += cpu->R[13];

    cpu->R[(cpu->CurInstr >> 8) & 0x7] = cpu->DataRead32(offset);
    cpu->Cycles += 1;
}


void T_PUSH(ARM* cpu)
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

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            cpu->DataWrite32(base, cpu->R[i]);
            base += 4;
        }
    }

    if (cpu->CurInstr & (1<<8))
    {
        cpu->DataWrite32(base, cpu->R[14]);
    }
}

void T_POP(ARM* cpu)
{
    u32 base = cpu->R[13];

    cpu->Cycles += 1;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            cpu->R[i] = cpu->DataRead32(base);
            base += 4;
        }
    }

    if (cpu->CurInstr & (1<<8))
    {
        u32 pc = cpu->DataRead32(base);
        if (cpu->Num==1) pc |= 0x1;
        cpu->JumpTo(pc);
        base += 4;
    }

    cpu->R[13] = base;
}

void T_STMIA(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 8) & 0x7];

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            cpu->DataWrite32(base, cpu->R[i]);
            base += 4;
        }
    }

    // TODO: check "Rb included in Rlist" case
    cpu->R[(cpu->CurInstr >> 8) & 0x7] = base;
}

void T_LDMIA(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 8) & 0x7];

    cpu->Cycles += 1;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            cpu->R[i] = cpu->DataRead32(base);
            base += 4;
        }
    }

    if (!(cpu->CurInstr & (1<<((cpu->CurInstr >> 8) & 0x7))))
        cpu->R[(cpu->CurInstr >> 8) & 0x7] = base;
}


}

