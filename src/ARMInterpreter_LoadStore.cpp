/*
    Copyright 2016-2024 melonDS team

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


namespace melonDS::ARMInterpreter
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
    u32 storeval = cpu->R[(cpu->CurInstr>>12) & 0xF]; \
    if (((cpu->CurInstr>>12) & 0xF) == 0xF) \
        storeval += 4; \
    bool dataabort = !cpu->DataWrite32(offset, storeval); \
    cpu->AddCycles_CD_STR(); \
    if (dataabort) return; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

// TODO: user mode (bit21)
#define A_STR_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 storeval = cpu->R[(cpu->CurInstr>>12) & 0xF]; \
    if (((cpu->CurInstr>>12) & 0xF) == 0xF) \
        storeval += 4; \
    bool dataabort = !cpu->DataWrite32(addr, storeval); \
    cpu->AddCycles_CD_STR(); \
    if (dataabort) return; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_STRB \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 storeval = cpu->R[(cpu->CurInstr>>12) & 0xF]; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) storeval+=4; \
    bool dataabort = !cpu->DataWrite8(offset, storeval); \
    cpu->AddCycles_CD_STR(); \
    if (dataabort) return; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

// TODO: user mode (bit21)
#define A_STRB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 storeval = cpu->R[(cpu->CurInstr>>12) & 0xF]; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) storeval+=4; \
    bool dataabort = !cpu->DataWrite8(addr, storeval); \
    cpu->AddCycles_CD_STR(); \
    if (dataabort) return; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_LDR \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead32(offset, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    val = ROR(val, ((offset&0x3)<<3)); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        if (cpu->Num==1) val &= ~0x1; \
        cpu->JumpTo(val); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    }

// TODO: user mode
#define A_LDR_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead32(addr, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    val = ROR(val, ((addr&0x3)<<3)); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
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
    u32 val; bool dataabort = !cpu->DataRead8(offset, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) cpu->JumpTo8_16Bit(val); \
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = val;

// TODO: user mode
#define A_LDRB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead8(addr, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) cpu->JumpTo8_16Bit(val); \
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = val;



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
    u32 storeval = cpu->R[(cpu->CurInstr>>12) & 0xF]; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) storeval+=4; \
    bool dataabort = !cpu->DataWrite16(offset, storeval); \
    cpu->AddCycles_CD_STR(); \
    if (dataabort) return; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_STRH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 storeval = cpu->R[(cpu->CurInstr>>12) & 0xF]; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) storeval+=4; \
    bool dataabort = !cpu->DataWrite16(addr, storeval); \
    cpu->AddCycles_CD_STR(); \
    if (dataabort) return; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

// TODO: CHECK LDRD/STRD TIMINGS!!

#define A_LDRD \
    if (cpu->Num != 0) return; \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    if (r&1) { A_UNK(cpu); return; } \
    if (!cpu->DataRead32 (offset  , &cpu->R[r  ])) {cpu->AddCycles_CDI(); return;} \
    u32 val; if (!cpu->DataRead32S(offset+4, &val)) {cpu->AddCycles_CDI(); return;} \
    if (r == 14) cpu->JumpTo(((((ARMv5*)cpu)->CP15Control & (1<<15)) ? (val & ~0x1) : val), cpu->CurInstr & (1<<22)); /* restores cpsr presumably due to shared dna with ldm */ \
    else cpu->R[r+1] = val; \
    cpu->AddCycles_CDI_LDM(); \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_LDRD_POST \
    if (cpu->Num != 0) return; \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    if (r&1) { A_UNK(cpu); return; } \
    if (!cpu->DataRead32 (addr  , &cpu->R[r  ])) {cpu->AddCycles_CDI(); return;} \
    u32 val; if (!cpu->DataRead32S(addr+4, &val)) {cpu->AddCycles_CDI(); return;} \
    if (r == 14) cpu->JumpTo(((((ARMv5*)cpu)->CP15Control & (1<<15)) ? (val & ~0x1) : val), cpu->CurInstr & (1<<22)); /* restores cpsr presumably due to shared dna with ldm */ \
    else cpu->R[r+1] = val; \
    cpu->AddCycles_CDI_LDM(); \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_STRD \
    if (cpu->Num != 0) return; \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    if (r&1) { A_UNK(cpu); return; } \
    bool dataabort = !cpu->DataWrite32(offset, cpu->R[r]); /* yes, this data abort behavior is on purpose */ \
    u32 storeval = cpu->R[r+1]; if (r == 14) storeval+=4; \
    dataabort |= !cpu->DataWrite32S (offset+4, storeval, dataabort); /* no, i dont understand it either */ \
    cpu->AddCycles_CD_STM(); \
    if (dataabort) return; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_STRD_POST \
    if (cpu->Num != 0) return; \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    if (r&1) { A_UNK(cpu); return; } \
    bool dataabort = !cpu->DataWrite32(addr, cpu->R[r]); \
    u32 storeval = cpu->R[r+1]; if (r == 14) storeval+=4; \
    dataabort |= !cpu->DataWrite32S (addr+4, storeval, dataabort); \
    cpu->AddCycles_CD_STM(); \
    if (dataabort) return; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_LDRH \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead16(offset, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) cpu->JumpTo8_16Bit(val); \
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_LDRH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead16(addr, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) cpu->JumpTo8_16Bit(val); \
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_LDRSB \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead8(offset, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    val = (s32)(s8)val; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) cpu->JumpTo8_16Bit(val); \
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_LDRSB_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead8(addr, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    val = (s32)(s8)val; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) cpu->JumpTo8_16Bit(val); \
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_LDRSH \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead16(offset, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    val = (s32)(s16)val; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) cpu->JumpTo8_16Bit(val); \
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_LDRSH_POST \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 val; bool dataabort = !cpu->DataRead16(addr, &val); \
    cpu->AddCycles_CDI_LDR(); \
    if (dataabort) return; \
    val = (s32)(s16)val; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) cpu->JumpTo8_16Bit(val); \
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = val; \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;


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
    if ((cpu->CurInstr & 0xF) == 15) rm += 4;

    u32 val;
    if (cpu->DataRead32(base, &val))
    {
        u32 numD = cpu->DataCycles;
        if (cpu->DataWrite32(base, rm))
        {
            // rd only gets updated if both read and write succeed
            u32 rd = (cpu->CurInstr >> 12) & 0xF;
            if (rd != 15) cpu->R[rd] = ROR(val, 8*(base&0x3));
            else if (cpu->Num==1) cpu->JumpTo(ROR(val, 8*(base&0x3)) & ~1); // for some reason these jumps don't work on the arm 9?
        }
        cpu->DataCycles += numD;
    }
    cpu->AddCycles_CDI_SWP();
}

void A_SWPB(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 16) & 0xF];
    u32 rm = cpu->R[cpu->CurInstr & 0xF] & 0xFF;
    if ((cpu->CurInstr & 0xF) == 15) rm += 4;

    u32 val;
    if (cpu->DataRead8(base, &val))
    {
        u32 numD = cpu->DataCycles;
        if (cpu->DataWrite8(base, rm))
        {
            // rd only gets updated if both read and write succeed
            u32 rd = (cpu->CurInstr >> 12) & 0xF;
            if (rd != 15) cpu->R[rd] = val;
            else if (cpu->Num==1) cpu->JumpTo(val & ~1); // for some reason these jumps don't work on the arm 9?
        }
        cpu->DataCycles += numD;
    }
    cpu->AddCycles_CDI_SWP();
}



void A_LDM(ARM* cpu)
{
    u32 baseid = (cpu->CurInstr >> 16) & 0xF;
    u32 base = cpu->R[baseid];
    u32 wbbase;
    u32 oldbase = base;
    u32 preinc = (cpu->CurInstr & (1<<24));
    bool first = true;

    if (!(cpu->CurInstr & (1<<23))) // decrement
    {
        // decrement is actually an increment starting from the end address
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

    // switch to user mode regs
    if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
        cpu->UpdateMode(cpu->CPSR, (cpu->CPSR&~0x1F)|0x10, true);

    for (int i = 0; i < 15; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (preinc) base += 4;
            if (!(first ? cpu->DataRead32 (base, &cpu->R[i])
                        : cpu->DataRead32S(base, &cpu->R[i])))
            {
                goto dataabort;
            }

            first = false;
            if (!preinc) base += 4;
        }
    }

    u32 pc;
    if ((cpu->CurInstr & (1<<15)))
    {
        if (preinc) base += 4;
        if (!(first ? cpu->DataRead32 (base, &pc)
                    : cpu->DataRead32S(base, &pc)))
        {
            goto dataabort;
        }

        if (!preinc) base += 4;

        if (cpu->Num == 1)
            pc &= ~0x1;
    }

    // switch back to previous regs
    if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
        cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR, true);

    // writeback to base
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
        
    // jump if pc got written
    if (cpu->CurInstr & (1<<15))
        cpu->JumpTo(pc, cpu->CurInstr & (1<<22));

    // jump here if a data abort occurred; writeback is ignored, and any jumps were aborted
    if (false)
    {
        dataabort:

        // switch back to original set of regs
        if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
            cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR, true);

        // restore original value of base in case the reg got written to
        cpu->R[baseid] = oldbase;
    }

    cpu->AddCycles_CDI_LDM();
}

void A_STM(ARM* cpu)
{
    u32 baseid = (cpu->CurInstr >> 16) & 0xF;
    u32 base = cpu->R[baseid];
    u32 oldbase = base;
    u32 preinc = (cpu->CurInstr & (1<<24));
    bool first = true;

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

        cpu->UpdateMode(cpu->CPSR, (cpu->CPSR&~0x1F)|0x10, true);
    }

    for (u32 i = 0; i < 16; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (preinc) base += 4;

            u32 val;
            if (i == baseid && !isbanked)
            {
                if ((cpu->Num == 0) || (!(cpu->CurInstr & ((1<<i)-1))))
                    val = oldbase;
                else val = base;
            }
            else val = cpu->R[i];

            if (i == 15) val+=4;

            if (!(first ? cpu->DataWrite32 (base, val)
                        : cpu->DataWrite32S(base, val)))
            {
                goto dataabort;
            }

            first = false;

            if (!preinc) base += 4;
        }
    }

    if (cpu->CurInstr & (1<<22))
        cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR, true);

    if ((cpu->CurInstr & (1<<23)) && (cpu->CurInstr & (1<<21)))
        cpu->R[baseid] = base;

    // jump here if a data abort occurred
    if (false)
    {
        dataabort:

        if (cpu->CurInstr & (1<<22))
            cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR, true);

        // restore original value of base
        cpu->R[baseid] = oldbase;
    }

    cpu->AddCycles_CD_STM();
}




// ---- THUMB -----------------------



void T_LDR_PCREL(ARM* cpu)
{
    u32 addr = (cpu->R[15] & ~0x2) + ((cpu->CurInstr & 0xFF) << 2);
    cpu->DataRead32(addr, &cpu->R[(cpu->CurInstr >> 8) & 0x7]);

    cpu->AddCycles_CDI_LDR();
}


void T_STR_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->DataWrite32(addr, cpu->R[cpu->CurInstr & 0x7]);

    cpu->AddCycles_CD_STR();
}

void T_STRB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->DataWrite8(addr, cpu->R[cpu->CurInstr & 0x7]);

    cpu->AddCycles_CD_STR();
}

void T_LDR_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];

    u32 val;
    if (cpu->DataRead32(addr, &val))
        cpu->R[cpu->CurInstr & 0x7] = ROR(val, 8*(addr&0x3));

    cpu->AddCycles_CDI_LDR();
}

void T_LDRB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->DataRead8(addr, &cpu->R[cpu->CurInstr & 0x7]);

    cpu->AddCycles_CDI_LDR();
}


void T_STRH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->DataWrite16(addr, cpu->R[cpu->CurInstr & 0x7]);

    cpu->AddCycles_CD_STR();
}

void T_LDRSB_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    if (cpu->DataRead8(addr, &cpu->R[cpu->CurInstr & 0x7]))
        cpu->R[cpu->CurInstr & 0x7] = (s32)(s8)cpu->R[cpu->CurInstr & 0x7];

    cpu->AddCycles_CDI_LDR();
}

void T_LDRH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    cpu->DataRead16(addr, &cpu->R[cpu->CurInstr & 0x7]);

    cpu->AddCycles_CDI_LDR();
}

void T_LDRSH_REG(ARM* cpu)
{
    u32 addr = cpu->R[(cpu->CurInstr >> 3) & 0x7] + cpu->R[(cpu->CurInstr >> 6) & 0x7];
    if (cpu->DataRead16(addr, &cpu->R[cpu->CurInstr & 0x7]))
        cpu->R[cpu->CurInstr & 0x7] = (s32)(s16)cpu->R[cpu->CurInstr & 0x7];

    cpu->AddCycles_CDI_LDR();
}


void T_STR_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 4) & 0x7C;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->DataWrite32(offset, cpu->R[cpu->CurInstr & 0x7]);
    cpu->AddCycles_CD_LDR();
}

void T_LDR_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 4) & 0x7C;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    u32 val;
    if (cpu->DataRead32(offset, &val))
        cpu->R[cpu->CurInstr & 0x7] = ROR(val, 8*(offset&0x3));
    cpu->AddCycles_CDI_LDR();
}

void T_STRB_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 6) & 0x1F;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->DataWrite8(offset, cpu->R[cpu->CurInstr & 0x7]);
    cpu->AddCycles_CD_STR();
}

void T_LDRB_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 6) & 0x1F;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->DataRead8(offset, &cpu->R[cpu->CurInstr & 0x7]);
    cpu->AddCycles_CDI_LDR();
}


void T_STRH_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 5) & 0x3E;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->DataWrite16(offset, cpu->R[cpu->CurInstr & 0x7]);
    cpu->AddCycles_CD_STR();
}

void T_LDRH_IMM(ARM* cpu)
{
    u32 offset = (cpu->CurInstr >> 5) & 0x3E;
    offset += cpu->R[(cpu->CurInstr >> 3) & 0x7];

    cpu->DataRead16(offset, &cpu->R[cpu->CurInstr & 0x7]);
    cpu->AddCycles_CDI_LDR();
}


void T_STR_SPREL(ARM* cpu)
{
    u32 offset = (cpu->CurInstr << 2) & 0x3FC;
    offset += cpu->R[13];

    cpu->DataWrite32(offset, cpu->R[(cpu->CurInstr >> 8) & 0x7]);
    cpu->AddCycles_CD_STR();
}

void T_LDR_SPREL(ARM* cpu)
{
    u32 offset = (cpu->CurInstr << 2) & 0x3FC;
    offset += cpu->R[13];

    cpu->DataRead32(offset, &cpu->R[(cpu->CurInstr >> 8) & 0x7]);
    cpu->AddCycles_CDI_LDR();
}


void T_PUSH(ARM* cpu)
{
    int nregs = 0;
    bool first = true;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
            nregs++;
    }

    if (cpu->CurInstr & (1<<8))
        nregs++;

    u32 base = cpu->R[13];
    base -= (nregs<<2);
    u32 wbbase = base;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (!(first ? cpu->DataWrite32 (base, cpu->R[i])
                        : cpu->DataWrite32S(base, cpu->R[i])))
            {
                goto dataabort;
            }
            first = false;
            base += 4;
        }
    }

    if (cpu->CurInstr & (1<<8))
    {
        if (!(first ? cpu->DataWrite32 (base, cpu->R[14])
                    : cpu->DataWrite32S(base, cpu->R[14])))
        {
            goto dataabort;
        }
    }

    cpu->R[13] = wbbase;

    dataabort:
    cpu->AddCycles_CD_STM();
}

void T_POP(ARM* cpu)
{
    u32 base = cpu->R[13];
    bool first = true;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (!(first ? cpu->DataRead32 (base, &cpu->R[i])
                        : cpu->DataRead32S(base, &cpu->R[i])))
            {
                goto dataabort;
            }
            first = false;
            base += 4;
        }
    }

    if (cpu->CurInstr & (1<<8))
    {
        u32 pc;
        if (!(first ? cpu->DataRead32 (base, &pc)
                    : cpu->DataRead32S(base, &pc)))
        {
            goto dataabort;
        }
        if (cpu->Num==1) pc |= 0x1;
        cpu->JumpTo(pc);
        base += 4;
    }

    cpu->R[13] = base;

    dataabort:
    cpu->AddCycles_CDI_LDM();
}

void T_STMIA(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 8) & 0x7];
    bool first = true;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (!(first ? cpu->DataWrite32 (base, cpu->R[i])
                        : cpu->DataWrite32S(base, cpu->R[i])))
            {
                goto dataabort;
            }
            first = false;
            base += 4;
        }
    }

    // TODO: check "Rb included in Rlist" case
    cpu->R[(cpu->CurInstr >> 8) & 0x7] = base;
    dataabort:
    cpu->AddCycles_CD_STM();
}

void T_LDMIA(ARM* cpu)
{
    u32 base = cpu->R[(cpu->CurInstr >> 8) & 0x7];
    bool first = true;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (!(first ? cpu->DataRead32 (base, &cpu->R[i])
                        : cpu->DataRead32S(base, &cpu->R[i])))
            {
                goto dataabort;
            }
            first = false;
            base += 4;
        }
    }

    if (!(cpu->CurInstr & (1<<((cpu->CurInstr >> 8) & 0x7))))
        cpu->R[(cpu->CurInstr >> 8) & 0x7] = base;

    dataabort:
    cpu->AddCycles_CDI_LDM();
}


}

