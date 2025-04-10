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
#include "NDS.h"


namespace melonDS::ARMInterpreter
{

template <bool bitfield>
inline bool ExecuteStage(ARM* cpu, u16 ilmask)
{
    if (cpu->CheckInterlock) { ((ARMv5*)cpu)->HandleInterlocksExecute<bitfield>(ilmask); return false;}
    return true;
}


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
    if (!(cpu->CurInstr & (1<<23))) offset = -offset; \
    u16 ilmask = 0;

#define A_WB_CALC_OFFSET_REG(shiftop) \
    u32 offset = cpu->R[cpu->CurInstr & 0xF]; \
    u32 shift = ((cpu->CurInstr>>7)&0x1F); \
    shiftop(offset, shift); \
    if (!(cpu->CurInstr & (1<<23))) offset = -offset; \
    u16 ilmask = 1 << (cpu->CurInstr & 0xF);

enum class Writeback
{
    None = 0,
    Pre,
    Post,
    Trans,
};

template<bool signextend, int size, Writeback writeback, bool multireg>
void LoadSingle(ARM* cpu, const u8 rd, const u8 rn, const s32 offset, const u16 ilmask)
{
    cpu->LDRFailedRegs = 0;
    static_assert((size == 8) || (size == 16) || (size == 32), "dummy this function only takes 8/16/32 for size!!!");
    
    if (!ExecuteStage<multireg>(cpu, (ilmask | (1<<rn)))) return;

    u32 addr;
    if constexpr (writeback < Writeback::Post) addr = offset + cpu->R[rn];
    else addr = cpu->R[rn];

    if constexpr (writeback == Writeback::Trans)
    {
        if (cpu->Num == 0)
            ((ARMv5*)cpu)->PU_Map = ((ARMv5*)cpu)->PU_UserMap;
    }

    u32 oldrd = cpu->R[rd];
    bool dabort;
    if constexpr (size == 8)  dabort = !cpu->DataRead8 (addr, rd);
    if constexpr (size == 16) dabort = !cpu->DataRead16(addr, rd);
    if constexpr (size == 32) dabort = !cpu->DataRead32(addr, rd);

    if constexpr (writeback == Writeback::Trans)
    {
        if (cpu->Num == 0 && (cpu->CPSR & 0x1F) != 0x10)
            ((ARMv5*)cpu)->PU_Map = ((ARMv5*)cpu)->PU_PrivMap;
    }

    cpu->AddCycles_CDI();
    if (dabort) [[unlikely]]
    {
        ((ARMv5*)cpu)->DataAbort();
        return;
    }

    if constexpr (size == 8 && signextend)
    {
        cpu->ExtReg = rd;
        if (cpu->Num == 0) ((ARMv5*)cpu)->QueueFunction(&ARMv5::SignExtend8);
        else               ((ARMv4*)cpu)->QueueFunction(&ARMv4::SignExtend8);
    }

    if constexpr (size == 16)
    {
        if (cpu->Num == 1)
        {
            cpu->ExtReg = rd;
            cpu->ExtROROffs = (addr & 0x1) * 8;
            ((ARMv4*)cpu)->QueueFunction(&ARMv4::ROR32); // unaligned 16 bit loads are ROR'd on arm7

            if constexpr (signextend)
            {
                if (addr&0x1) ((ARMv4*)cpu)->QueueFunction(&ARMv4::SignExtend8); // sign extend like an ldrsb if we ror'd the value.
                else          ((ARMv4*)cpu)->QueueFunction(&ARMv4::SignExtend16);
            }
        }
        else if constexpr (signextend)
        {
            cpu->ExtReg = rd;
            ((ARMv5*)cpu)->QueueFunction(&ARMv5::SignExtend16);
        }
    }

    if constexpr (size == 32)
    {
        cpu->ExtReg = rd;
        cpu->ExtROROffs = (addr & 0x3) * 8;
        if (cpu->Num == 0) ((ARMv5*)cpu)->QueueFunction(&ARMv5::ROR32);
        else               ((ARMv4*)cpu)->QueueFunction(&ARMv4::ROR32);
    }

    if constexpr (writeback >= Writeback::Post) addr += offset;
    if constexpr (writeback != Writeback::None) 
    {
        if (rn != 15) [[likely]] // r15 writeback fails on arm9
        {
            if (rd != rn) cpu->R[rn] = addr;
        }
        else if (cpu->Num == 1) // arm 7
        {
            cpu->R[rd] = oldrd; // note that at no point does it actually write the value it loaded into a register...
            cpu->LDRFailedRegs = 1<<rd;
            cpu->JumpTo((addr+4) & ~1); // +4 cause reasons
            return;
        }
    }

    if (rd == 15)
    {
        if (cpu->Num==0) ((ARMv5*)cpu)->ForceInterlock((size<32) || (addr&0x3));

        cpu->JumpTo(cpu->R[15], false, 1);
    }
    else if (cpu->Num == 0) ((ARMv5*)cpu)->SetupInterlock(rd, (size < 32) || (addr&0x3));
}

template<int size, Writeback writeback, bool multireg>
void StoreSingle(ARM* cpu, const u8 rd, const u8 rn, const s32 offset, const u16 ilmask)
{
    static_assert((size == 8) || (size == 16) || (size == 32), "dummy this function only takes 8/16/32 for size!!!");

    if (!ExecuteStage<multireg>(cpu, (ilmask | (1<<rn)))) return;

    u32 addr;
    if constexpr (writeback < Writeback::Post) addr = offset + cpu->R[rn];
    else addr = cpu->R[rn];

    u32 storeval = cpu->R[rd];
    if (rd == 15) storeval += 4;
    
    if constexpr (writeback == Writeback::Trans)
    {
        if (cpu->Num == 0)
            ((ARMv5*)cpu)->PU_Map = ((ARMv5*)cpu)->PU_UserMap;
    }

    if (cpu->Num == 0)
        ((ARMv5*)cpu)->HandleInterlocksMemory(rd);

    bool dabort;
    if constexpr (size == 8)  dabort = !cpu->DataWrite8 (addr, storeval, rd);
    if constexpr (size == 16) dabort = !cpu->DataWrite16(addr, storeval, rd);
    if constexpr (size == 32) dabort = !cpu->DataWrite32(addr, storeval, rd);

    if constexpr (writeback == Writeback::Trans)
    {
        if (cpu->Num == 0 && (cpu->CPSR & 0x1F) != 0x10)
            ((ARMv5*)cpu)->PU_Map = ((ARMv5*)cpu)->PU_PrivMap;
    }

    cpu->AddCycles_CD();
    if (dabort) [[unlikely]]
    {
        ((ARMv5*)cpu)->DataAbort();
        return;
    }
    
    if constexpr (writeback >= Writeback::Post) addr += offset;
    if constexpr (writeback != Writeback::None) 
    {
        if (rn != 15) [[likely]]
        {
            cpu->R[rn] = addr;
        }
        else if (cpu->Num == 1) // r15 writeback fails on arm9
        {
            cpu->JumpTo(addr & ~1);
        }
    }
}


#define A_STR \
    if (cpu->CurInstr & (1<<21)) StoreSingle<32, Writeback::Pre, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else StoreSingle<32, Writeback::None, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_STR_POST \
    if (cpu->CurInstr & (1<<21)) StoreSingle<32, Writeback::Trans, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else StoreSingle<32, Writeback::Post, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_STRB \
    if (cpu->CurInstr & (1<<21)) StoreSingle<8, Writeback::Pre, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else StoreSingle<8, Writeback::None, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_STRB_POST \
    if (cpu->CurInstr & (1<<21)) StoreSingle<8, Writeback::Trans, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else StoreSingle<8, Writeback::Post, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDR \
    if (cpu->CurInstr & (1<<21)) LoadSingle<false, 32, Writeback::Pre, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else LoadSingle<false, 32, Writeback::None, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDR_POST \
    if (cpu->CurInstr & (1<<21)) LoadSingle<false, 32, Writeback::Trans, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else LoadSingle<false, 32, Writeback::Post, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDRB \
    if (cpu->CurInstr & (1<<21)) LoadSingle<false, 8, Writeback::Pre, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else LoadSingle<false, 8, Writeback::None, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDRB_POST \
    if (cpu->CurInstr & (1<<21)) LoadSingle<false, 8, Writeback::Trans, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else LoadSingle<false, 8, Writeback::Post, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);



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
    if (!(cpu->CurInstr & (1<<23))) offset = -offset; \
    u16 ilmask = 0;

#define A_HD_CALC_OFFSET_REG \
    u32 offset = cpu->R[cpu->CurInstr & 0xF]; \
    if (!(cpu->CurInstr & (1<<23))) offset = -offset; \
    u16 ilmask = 1 << (cpu->CurInstr & 0xF);



#define A_STRH \
    if (cpu->CurInstr & (1<<21)) StoreSingle<16, Writeback::Pre, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else StoreSingle<16, Writeback::None, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_STRH_POST \
    StoreSingle<16, Writeback::Post, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

// TODO: CHECK LDRD/STRD TIMINGS!!

#define A_LDRD \
    if (cpu->Num != 0) return; \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    if (r&1) { A_UNK(cpu); return; } \
    cpu->LDRFailedRegs = 0; \
    if (!ExecuteStage<true>(cpu, ilmask | (1 << ((cpu->CurInstr>>16) & 0xF)))) return; \
    bool dabort = !cpu->DataRead32(offset, r); \
    u32 oldval = cpu->R[r+1]; dabort |= !cpu->DataRead32S(offset+4, r+1); \
    ((ARMv5*)cpu)->DelayIfITCM(2); \
    cpu->AddCycles_CDI(); \
    if (dabort) { \
        cpu->R[r+1] = oldval; \
        ((ARMv5*)cpu)->DataAbort(); \
        return; } \
    if (r+1 == 15) { \
        if (cpu->Num==0) ((ARMv5*)cpu)->ForceInterlock(); \
        cpu->JumpTo(cpu->R[15], cpu->CurInstr & (1<<22), 1); } /* restores cpsr presumably due to shared dna with ldm */ \
    else { \
        if (cpu->Num == 0) ((ARMv5*)cpu)->SetupInterlock(r+1); } \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_LDRD_POST \
    if (cpu->Num != 0) return; \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    if (r&1) { A_UNK(cpu); return; } \
    cpu->LDRFailedRegs = 0; \
    if (!ExecuteStage<true>(cpu, ilmask | (1 << ((cpu->CurInstr>>16) & 0xF)))) return; \
    bool dabort = !cpu->DataRead32(addr, r); \
    u32 oldval = cpu->R[r+1]; dabort |= !cpu->DataRead32S(addr+4, r+1); \
    ((ARMv5*)cpu)->DelayIfITCM(2); \
    cpu->AddCycles_CDI(); \
    if (dabort) { \
        cpu->R[r+1] = oldval; \
        ((ARMv5*)cpu)->DataAbort(); \
        return; } \
    if (r+1 == 15) { \
        if (cpu->Num==0) ((ARMv5*)cpu)->ForceInterlock(); ; \
        cpu->JumpTo(cpu->R[15], cpu->CurInstr & (1<<22), 1); } /* restores cpsr presumably due to shared dna with ldm */ \
    else { \
        if (cpu->Num == 0) ((ARMv5*)cpu)->SetupInterlock(r+1); } \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_STRD \
    if (cpu->Num != 0) return; \
    offset += cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    if (r&1) { A_UNK(cpu); return; } \
    if (!ExecuteStage<true>(cpu, ilmask | (1 << ((cpu->CurInstr>>16) & 0xF)))) return; \
    ((ARMv5*)cpu)->HandleInterlocksMemory(r); \
    bool dabort = !cpu->DataWrite32(offset, cpu->R[r], r); \
    u32 storeval = cpu->R[r+1]; if (r+1 == 15) storeval+=4; \
    dabort |= !cpu->DataWrite32S (offset+4, storeval, r+1); \
   ((ARMv5*)cpu)->DelayIfITCM(2); \
    cpu->AddCycles_CD(); \
    if (dabort) [[unlikely]] { \
        ((ARMv5*)cpu)->DataAbort(); \
        return; } \
    if (cpu->CurInstr & (1<<21)) cpu->R[(cpu->CurInstr>>16) & 0xF] = offset;

#define A_STRD_POST \
    if (cpu->Num != 0) return; \
    u32 addr = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 r = (cpu->CurInstr>>12) & 0xF; \
    if (r&1) { A_UNK(cpu); return; } \
    if (!ExecuteStage<true>(cpu, ilmask | (1 << ((cpu->CurInstr>>16) & 0xF)))) return; \
    ((ARMv5*)cpu)->HandleInterlocksMemory(r); \
    bool dabort = !cpu->DataWrite32(addr, cpu->R[r], r); \
    u32 storeval = cpu->R[r+1]; if (r+1 == 15) storeval+=4; \
    dabort |= !cpu->DataWrite32S (addr+4, storeval, r+1); \
    ((ARMv5*)cpu)->DelayIfITCM(2); \
    cpu->AddCycles_CD(); \
    if (dabort) [[unlikely]] { \
        ((ARMv5*)cpu)->DataAbort(); \
        return; } \
    cpu->R[(cpu->CurInstr>>16) & 0xF] += offset;

#define A_LDRH \
    if (cpu->CurInstr & (1<<21)) LoadSingle<false, 16, Writeback::Pre, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else LoadSingle<false, 16, Writeback::None, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDRH_POST \
    LoadSingle<false, 16, Writeback::Post, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDRSB \
    if (cpu->CurInstr & (1<<21)) LoadSingle<true, 8, Writeback::Pre, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else LoadSingle<true, 8, Writeback::None, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDRSB_POST \
    LoadSingle<true, 8, Writeback::Post, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDRSH \
    if (cpu->CurInstr & (1<<21)) LoadSingle<true, 16, Writeback::Pre, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask); \
    else LoadSingle<true, 16, Writeback::None, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);

#define A_LDRSH_POST \
    LoadSingle<true, 16, Writeback::Post, true>(cpu, ((cpu->CurInstr>>12) & 0xF), ((cpu->CurInstr>>16) & 0xF), offset, ilmask);


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



template<bool byte>
inline void SWP(ARM* cpu)
{
    if (!ExecuteStage<false>(cpu, ((cpu->CurInstr >> 16) & 0xF))) return;
    cpu->LDRFailedRegs = 0;
    u32 base = cpu->R[(cpu->CurInstr >> 16) & 0xF];
    u32 rd = (cpu->CurInstr >> 12) & 0xF;
    u32 rm = cpu->CurInstr & 0xF;
    u32 storeval = cpu->R[rm];
    if (rm == 15) storeval += 4;
    

    u32 oldrd = cpu->R[rd];

    if ((byte ? cpu->DataRead8 (base, rd)
              : cpu->DataRead32(base, rd))) [[likely]]
    {
        if ((byte ? cpu->DataWrite8 (base, storeval, rm)
                  : cpu->DataWrite32(base, storeval, rm))) [[likely]]
        {
            // rd only gets updated if both read and write succeed

            if constexpr (!byte)
            {
                cpu->ExtReg = rd;
                cpu->ExtROROffs = (base & 0x3) * 8;
                if (cpu->Num == 0) ((ARMv5*)cpu)->QueueFunction(&ARMv5::ROR32);
                else               ((ARMv4*)cpu)->QueueFunction(&ARMv4::ROR32);
            }
            cpu->AddCycles_CDI();

            if (rd != 15)
            {
                if (cpu->Num == 0) ((ARMv5*)cpu)->SetupInterlock(rd, byte || (base&0x3));
            }
            else if (cpu->Num==1) // for some reason these jumps don't seem to work on the arm 9?
            {
                cpu->JumpTo(cpu->R[rd], false, 1);
            }
            return;
        }
    }

    // data abort handling
    cpu->R[rd] = oldrd;
    cpu->LDRFailedRegs = 1<<rd;
    cpu->AddCycles_CDI();
    ((ARMv5*)cpu)->DataAbort();
}

void A_SWP(ARM* cpu)
{
    SWP<false>(cpu);
}

void A_SWPB(ARM* cpu)
{
    SWP<true>(cpu);
}

void EmptyRListLDMSTM(ARM* cpu, const u8 baseid, const u8 flags)
{
    enum // flags
    {
        load = (1<<0),
        writeback = (1<<1),
        decrement = (1<<2),
        preinc = (1<<3),
        restoreorthumb = (1<<4), // specifies restore cpsr for loads, thumb instr for stores
    };

    if (cpu->Num == 1)
    {
        u32 base = cpu->R[baseid];
        bool flagpreinc = flags & preinc;

        if (flags & decrement)
        {
            flagpreinc = !flagpreinc;
            base -= 0x40;
        }
        if (flagpreinc) base+=4;
        
        if (flags & load)
        {
            cpu->DataRead32(base, 15);

            cpu->AddCycles_CDI();
            
            cpu->JumpTo(cpu->R[15], flags & restoreorthumb, 1); // TODO: fix this not maintaining current mode properly
        }
        else
        {
            cpu->DataWrite32(base, cpu->R[15] + ((flags & restoreorthumb) ? 2 : 4), 15);

            cpu->AddCycles_CD();
        }
    }
    else
    {
        cpu->AddCycles_C(); // checkme
    }

    if (flags & writeback)
    {
        if (flags & decrement) cpu->R[baseid] -= 0x40;
        else                   cpu->R[baseid] += 0x40;
    }
}

void A_LDM(ARM* cpu)
{
    u32 baseid = (cpu->CurInstr >> 16) & 0xF;
    if (!ExecuteStage<false>(cpu, baseid)) return;

    cpu->LDRFailedRegs = 0;
    u32 base = cpu->R[baseid];
    u32 wbbase;
    u32 oldbase = base;
    u32 preinc = (cpu->CurInstr & (1<<24));
    bool first = true;
    bool dabort = false;
    
    if (!(cpu->CurInstr & 0xFFFF)) [[unlikely]]
    {
        EmptyRListLDMSTM(cpu, baseid, ((1 << 0) |                            // load
                                       (((cpu->CurInstr >> 21) & 1) << 1) |  // writeback
                                       ((!(cpu->CurInstr & (1<<23))) << 2) | // decrement
                                       ((preinc >> 24) << 3) |               // preinc
                                       (((cpu->CurInstr >> 22) & 1) << 4))); // restore
        return;
    }

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
    {
        cpu->UpdateMode(cpu->CPSR, (cpu->CPSR&~0x1F)|0x10, true);
        //if (cpu->MRTrack.Type != MainRAMType::Null) printf("AHA, DERES THE PROBLEM\n");
    }

    for (int i = 0; i < 15; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            if (preinc) base += 4;
            u32 oldval = cpu->R[i];
            dabort |= !(first ? cpu->DataRead32 (base, i)
                              : cpu->DataRead32S(base, i));
            if (dabort) [[unlikely]] { cpu->R[i] = oldval; cpu->LDRFailedRegs |= (1<<i); }

            first = false;
            if (!preinc) base += 4;
        }
    }

    if (cpu->CurInstr & (1<<15))
    {
        if (preinc) base += 4;
        u32 oldval = cpu->R[15];
        dabort |= !(first ? cpu->DataRead32 (base, 15)
                          : cpu->DataRead32S(base, 15));
        if (dabort) [[unlikely]] { cpu->R[15] = oldval; cpu->LDRFailedRegs |= (1<<15); }

        if (!preinc) base += 4;
    }

    if (__builtin_popcount(cpu->CurInstr & 0xFFFF) == 1) [[unlikely]] // single reg
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(1);
        cpu->AddCycles_CDI();
        if (cpu->Num == 0) ((ARMv5*)cpu)->ForceInterlock(); // on arm9 single reg ldm/stm cannot overlap memory and fetch stages
        else; // CHECKME: ARM7 timing behavior?
    }
    else
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(2);
        cpu->AddCycles_CDI();
    }

    // handle data aborts
    if (dabort) [[unlikely]]
    {
        if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
        {
            cpu->QueueMode[0] = (cpu->CPSR&~0x1F)|0x10;
            cpu->QueueMode[1] = cpu->CPSR;

            if (cpu->Num == 0) ((ARMv5*)cpu)->QueueFunction(&ARMv5::QueueUpdateMode);
            else               ((ARMv4*)cpu)->QueueFunction(&ARMv4::QueueUpdateMode);
        }

        ((ARMv5*)cpu)->DataAbort();
        return;
    }

    // writeback to base
    if (cpu->CurInstr & (1<<21) && (baseid != 15))
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
                    { cpu->R[baseid] = wbbase; cpu->LDRFailedRegs = 1<<baseid; }
            }
        }
        else
            cpu->R[baseid] = wbbase;
    }
    
    // switch back to previous regs
    if ((cpu->CurInstr & (1<<22)) && !(cpu->CurInstr & (1<<15)))
    {
        cpu->QueueMode[0] = (cpu->CPSR&~0x1F)|0x10;
        cpu->QueueMode[1] = cpu->CPSR;

        if (cpu->Num == 0) ((ARMv5*)cpu)->QueueFunction(&ARMv5::QueueUpdateMode);
        else               ((ARMv4*)cpu)->QueueFunction(&ARMv4::QueueUpdateMode);
    }

    // jump if pc got written
    if (cpu->CurInstr & (1<<15))
    {
        if (cpu->Num==0) ((ARMv5*)cpu)->ForceInterlock();
        cpu->JumpTo(cpu->R[15], cpu->CurInstr & (1<<22), 1);
    }
    else if (cpu->Num == 0)
    {
        u8 lastreg = 31 - __builtin_clz(cpu->CurInstr & 0x7FFF);
        ((ARMv5*)cpu)->SetupInterlock(lastreg);
    }
}

void A_STM(ARM* cpu)
{
    u32 baseid = (cpu->CurInstr >> 16) & 0xF;
    if (!ExecuteStage<false>(cpu, baseid)) return;

    u32 base = cpu->R[baseid];
    u32 oldbase = base;
    u32 preinc = (cpu->CurInstr & (1<<24));
    bool first = true;
    bool dabort = false;
    
    if (!(cpu->CurInstr & 0xFFFF)) [[unlikely]]
    {
        EmptyRListLDMSTM(cpu, baseid, ((0 << 0) |                            // load
                                       (((cpu->CurInstr >> 21) & 1) << 1) |  // writeback
                                       ((!(cpu->CurInstr & (1<<23))) << 2) | // decrement
                                       ((preinc >> 24) << 3) |               // preinc
                                       (0 << 4)));                           // thumb
        return;
    }

    if (!(cpu->CurInstr & (1<<23)))
    {
        for (u32 i = 0; i < 16; i++)
        {
            if (cpu->CurInstr & (1<<i))
                base -= 4;
        }

        if ((cpu->CurInstr & (1<<21)) && (baseid != 15))
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

    if (cpu->Num == 0) ((ARMv5*)cpu)->HandleInterlocksMemory(__builtin_ctz(cpu->CurInstr));

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

            dabort |= !(first ? cpu->DataWrite32 (base, val, i)
                              : cpu->DataWrite32S(base, val, i));

            first = false;

            if (!preinc) base += 4;
        }
    }

    if (cpu->CurInstr & (1<<22))
        cpu->UpdateMode((cpu->CPSR&~0x1F)|0x10, cpu->CPSR, true);

    if (__builtin_popcount(cpu->CurInstr & 0xFFFF) == 1) [[unlikely]] // single reg
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(1);
        cpu->AddCycles_CD();
        if (cpu->Num == 0) ((ARMv5*)cpu)->ForceInterlock(); // on arm9 single reg ldm/stm cannot overlap memory and fetch stages
        else; // CHECKME: ARM7 timing behavior?
    }
    else
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(2);
        cpu->AddCycles_CD();
    }

    // handle data aborts
    if (dabort) [[unlikely]]
    {
        // restore original value of base
        cpu->R[baseid] = oldbase;
        ((ARMv5*)cpu)->DataAbort();
        return;
    }

    if ((cpu->CurInstr & (1<<23)) && (cpu->CurInstr & (1<<21)) && (baseid != 15))
        cpu->R[baseid] = base;
}




// ---- THUMB -----------------------



void T_LDR_PCREL(ARM* cpu)
{
    if (!ExecuteStage<false>(cpu, 15)) return;

    cpu->LDRFailedRegs = 0;
    u32 addr = (cpu->R[15] & ~0x2) + ((cpu->CurInstr & 0xFF) << 2);
    bool dabort = !cpu->DataRead32(addr, (cpu->CurInstr >> 8) & 0x7);

    cpu->AddCycles_CDI();
    if (dabort) [[unlikely]] ((ARMv5*)cpu)->DataAbort();
    else if (cpu->Num == 0)
    {
        ((ARMv5*)cpu)->SetupInterlock((cpu->CurInstr >> 8) & 0x7);
    }
}


void T_STR_REG(ARM* cpu)
{
    StoreSingle<32, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), cpu->R[(cpu->CurInstr >> 6) & 0x7], (1 << ((cpu->CurInstr >> 6) & 0x7)));
}

void T_STRB_REG(ARM* cpu)
{
    StoreSingle<8, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), cpu->R[(cpu->CurInstr >> 6) & 0x7], (1 << ((cpu->CurInstr >> 6) & 0x7)));
}

void T_LDR_REG(ARM* cpu)
{
    LoadSingle<false, 32, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), cpu->R[(cpu->CurInstr >> 6) & 0x7], (1 << ((cpu->CurInstr >> 6) & 0x7)));
}

void T_LDRB_REG(ARM* cpu)
{
    LoadSingle<false, 8, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), cpu->R[(cpu->CurInstr >> 6) & 0x7], (1 << ((cpu->CurInstr >> 6) & 0x7)));
}


void T_STRH_REG(ARM* cpu)
{
    StoreSingle<16, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), cpu->R[(cpu->CurInstr >> 6) & 0x7], (1 << ((cpu->CurInstr >> 6) & 0x7)));
}

void T_LDRSB_REG(ARM* cpu)
{
    LoadSingle<true, 8, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), cpu->R[(cpu->CurInstr >> 6) & 0x7], (1 << ((cpu->CurInstr >> 6) & 0x7)));
}

void T_LDRH_REG(ARM* cpu)
{
    LoadSingle<false, 16, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), cpu->R[(cpu->CurInstr >> 6) & 0x7], (1 << ((cpu->CurInstr >> 6) & 0x7)));
}

void T_LDRSH_REG(ARM* cpu)
{
    LoadSingle<true, 16, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), cpu->R[(cpu->CurInstr >> 6) & 0x7], (1 << ((cpu->CurInstr >> 6) & 0x7)));
}


void T_STR_IMM(ARM* cpu)
{
    StoreSingle<32, Writeback::None, false>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), ((cpu->CurInstr >> 4) & 0x7C), 0);
}

void T_LDR_IMM(ARM* cpu)
{
    LoadSingle<false, 32, Writeback::None, true>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), ((cpu->CurInstr >> 4) & 0x7C), 0);
}

void T_STRB_IMM(ARM* cpu)
{
    StoreSingle<8, Writeback::None, false>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), ((cpu->CurInstr >> 6) & 0x1F), 0);
}

void T_LDRB_IMM(ARM* cpu)
{
    LoadSingle<false, 8, Writeback::None, false>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), ((cpu->CurInstr >> 6) & 0x1F), 0);
}


void T_STRH_IMM(ARM* cpu)
{
    StoreSingle<16, Writeback::None, false>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), ((cpu->CurInstr >> 5) & 0x3E), 0);
}

void T_LDRH_IMM(ARM* cpu)
{
    LoadSingle<false, 16, Writeback::None, false>(cpu, (cpu->CurInstr & 0x7), ((cpu->CurInstr >> 3) & 0x7), ((cpu->CurInstr >> 5) & 0x3E), 0);
}


void T_STR_SPREL(ARM* cpu)
{
    StoreSingle<32, Writeback::None, false>(cpu, ((cpu->CurInstr >> 8) & 0x7), 13, ((cpu->CurInstr << 2) & 0x3FC), 0);
}

void T_LDR_SPREL(ARM* cpu)
{
    LoadSingle<false, 32, Writeback::None, false>(cpu, ((cpu->CurInstr >> 8) & 0x7), 13, ((cpu->CurInstr << 2) & 0x3FC), 0);
}


void T_PUSH(ARM* cpu)
{
    if (!ExecuteStage<false>(cpu, 13)) return;

    int nregs = 0;
    bool first = true;
    bool dabort = false;

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
            nregs++;
    }

    if (cpu->CurInstr & (1<<8))
        nregs++;
        
    if (!nregs) [[unlikely]]
    {
        EmptyRListLDMSTM(cpu, 13, 0b11110);
        return;
    }

    u32 base = cpu->R[13];
    base -= (nregs<<2);
    u32 wbbase = base;

    if (cpu->Num == 0)
    {
        u8 firstreg = __builtin_ctz(cpu->CurInstr);
        if (firstreg == 8) firstreg = 14;
        ((ARMv5*)cpu)->HandleInterlocksMemory(firstreg);
    }

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            dabort |= !(first ? cpu->DataWrite32 (base, cpu->R[i], i)
                              : cpu->DataWrite32S(base, cpu->R[i], i));
            first = false;
            base += 4;
        }
    }

    if (cpu->CurInstr & (1<<8))
    {
        dabort |= !(first ? cpu->DataWrite32 (base, cpu->R[14], 14)
                          : cpu->DataWrite32S(base, cpu->R[14], 14));
    }

    if (__builtin_popcount(cpu->CurInstr & 0x1FF) == 1) [[unlikely]] // single reg
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(1);
        cpu->AddCycles_CD();
        if (cpu->Num == 0) ((ARMv5*)cpu)->ForceInterlock(); // on arm9 single reg ldm/stm cannot overlap memory and fetch stages
        else; // CHECKME: ARM7 timing behavior?
    }
    else
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(2);
        cpu->AddCycles_CD();
    }

    if (dabort) [[unlikely]]
    {
        ((ARMv5*)cpu)->DataAbort();
        return;
    }

    cpu->R[13] = wbbase;
}

void T_POP(ARM* cpu)
{
    if (!ExecuteStage<false>(cpu, 13)) return;

    cpu->LDRFailedRegs = 0;
    u32 base = cpu->R[13];
    bool first = true;
    bool dabort = false;

    if (!(cpu->CurInstr & 0x1FF)) [[unlikely]]
    {
        EmptyRListLDMSTM(cpu, 13, 0b00011);
        return;
    }

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            u32 oldval = cpu->R[i];
            dabort |= !(first ? cpu->DataRead32 (base, i)
                              : cpu->DataRead32S(base, i));
            if (dabort) [[unlikely]]  { cpu->R[i] = oldval; cpu->LDRFailedRegs |= (1<<i); }

            first = false;
            base += 4;
        }
    }

    if (cpu->CurInstr & (1<<8))
    {
        u32 oldval = cpu->R[15];
        dabort |= !(first ? cpu->DataRead32 (base, 15)
                          : cpu->DataRead32S(base, 15));

        if (__builtin_popcount(cpu->CurInstr & 0x1FF) == 1) [[unlikely]] // single reg
        {
            if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(1);
            cpu->AddCycles_CDI();
            if (cpu->Num == 0) ((ARMv5*)cpu)->ForceInterlock(); // on arm9 single reg ldm/stm cannot overlap memory and fetch stages
            else; // CHECKME: ARM7 timing behavior?
        }
        else
        {
            if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(2);
            cpu->AddCycles_CDI();
        }

        if (!dabort) [[likely]]
        {
            if (cpu->Num==0) ((ARMv5*)cpu)->ForceInterlock();

            cpu->JumpTo(cpu->R[15], false, 2);
            base += 4;
        }
        else [[unlikely]]
        {
            cpu->R[15] = oldval;
            cpu->LDRFailedRegs |= (1<<15);
            ((ARMv5*)cpu)->DataAbort();
            return;
        }
    }
    else
    {
        if (__builtin_popcount(cpu->CurInstr & 0x1FF) == 1) [[unlikely]] // single reg
        {
            if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(1);
            cpu->AddCycles_CDI();
            if (cpu->Num == 0) ((ARMv5*)cpu)->ForceInterlock(); // on arm9 single reg ldm/stm cannot overlap memory and fetch stages
            else; // CHECKME: ARM7 timing behavior?
        }
        else
        {
            if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(2);
            cpu->AddCycles_CDI();
        }

        if (cpu->Num == 0)
        {
            if (dabort) [[unlikely]]
            {
                ((ARMv5*)cpu)->DataAbort();
                return;
            }
            else
            {
                u8 lastreg = 31 - __builtin_clz(cpu->CurInstr & 0xFF);
                ((ARMv5*)cpu)->SetupInterlock(lastreg);
            }
        }
    }

    cpu->R[13] = base;
}

void T_STMIA(ARM* cpu)
{
    if (!ExecuteStage<false>(cpu, ((cpu->CurInstr >> 8) & 0x7))) return;

    u32 base = cpu->R[(cpu->CurInstr >> 8) & 0x7];
    bool first = true;
    bool dabort = false;

    if (!(cpu->CurInstr & 0xFF)) [[unlikely]]
    {
        EmptyRListLDMSTM(cpu, (cpu->CurInstr >> 8) & 0x7, 0b10010);
        return;
    }

    if (cpu->Num == 0)
    {
        u8 firstreg = __builtin_ctz(cpu->CurInstr);
        ((ARMv5*)cpu)->HandleInterlocksMemory(firstreg);
    }

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            dabort |= !(first ? cpu->DataWrite32 (base, cpu->R[i], i)
                              : cpu->DataWrite32S(base, cpu->R[i], i));
            first = false;
            base += 4;
        }
    }

    if (__builtin_popcount(cpu->CurInstr & 0xFF) == 1) [[unlikely]] // single reg
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(1);
        cpu->AddCycles_CD();
        if (cpu->Num == 0) ((ARMv5*)cpu)->ForceInterlock(); // on arm9 single reg ldm/stm cannot overlap memory and fetch stages
        else; // CHECKME: ARM7 timing behavior?
    }
    else
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(2);
        cpu->AddCycles_CD();
    }

    if (dabort) [[unlikely]]
    {
        ((ARMv5*)cpu)->DataAbort();
        return;
    }

    // TODO: check "Rb included in Rlist" case
    cpu->R[(cpu->CurInstr >> 8) & 0x7] = base;
}

void T_LDMIA(ARM* cpu)
{
    if (!ExecuteStage<false>(cpu, ((cpu->CurInstr >> 8) & 0x7))) return;

    u32 base = cpu->R[(cpu->CurInstr >> 8) & 0x7];
    bool first = true;
    bool dabort = false;

    if (!(cpu->CurInstr & 0xFF)) [[unlikely]]
    {
        EmptyRListLDMSTM(cpu, (cpu->CurInstr >> 8) & 0x7, 0b00011);
        return;
    }

    for (int i = 0; i < 8; i++)
    {
        if (cpu->CurInstr & (1<<i))
        {
            u32 oldval = cpu->R[i];
            dabort |= !(first ? cpu->DataRead32 (base, i)
                              : cpu->DataRead32S(base, i));
            if (dabort) [[unlikely]]  { cpu->R[i] = oldval; cpu->LDRFailedRegs |= (1<<i); }

            first = false;
            base += 4;
        }
    }

    if (__builtin_popcount(cpu->CurInstr & 0xFF) == 1) [[unlikely]] // single reg
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(1);
        cpu->AddCycles_CDI();
        if (cpu->Num == 0) ((ARMv5*)cpu)->ForceInterlock(); // on arm9 single reg ldm/stm cannot overlap memory and fetch stages
        else; // CHECKME: ARM7 timing behavior?
    }
    else
    {
        if (cpu->Num == 0) ((ARMv5*)cpu)->DelayIfITCM(2);
        cpu->AddCycles_CDI();
    }

    if (dabort) [[unlikely]]
    {
        ((ARMv5*)cpu)->DataAbort();
        return;
    }

    if (cpu->Num == 0)
    {
        u8 lastreg = 31 - __builtin_clz(cpu->CurInstr & 0xFF);
        ((ARMv5*)cpu)->SetupInterlock(lastreg);
    }

    if (!(cpu->CurInstr & (1<<((cpu->CurInstr >> 8) & 0x7))))
        cpu->R[(cpu->CurInstr >> 8) & 0x7] = base;
}


}

