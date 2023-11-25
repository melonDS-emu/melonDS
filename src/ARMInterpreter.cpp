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

#include <stdio.h>
#include "NDS.h"
#include "ARMInterpreter.h"
#include "ARMInterpreter_ALU.h"
#include "ARMInterpreter_Branch.h"
#include "ARMInterpreter_LoadStore.h"
#include "Platform.h"

#ifdef GDBSTUB_ENABLED
#include "debug/GdbStub.h"
#endif

namespace melonDS::ARMInterpreter
{
    using Platform::Log;
    using Platform::LogLevel;


void A_UNK(ARM* cpu)
{
    Log(LogLevel::Warn, "undefined ARM%d instruction %08X @ %08X\n", cpu->Num?7:9, cpu->CurInstr, cpu->R[15]-8);
#ifdef GDBSTUB_ENABLED
    cpu->GdbStub.Enter(true, Gdb::TgtStatus::FaultInsn, cpu->R[15]-8);
#endif
    //for (int i = 0; i < 16; i++) printf("R%d: %08X\n", i, cpu->R[i]);
    //NDS::Halt();
    u32 oldcpsr = cpu->CPSR;
    cpu->CPSR &= ~0xBF;
    cpu->CPSR |= 0x9B;
    cpu->UpdateMode(oldcpsr, cpu->CPSR);

    cpu->R_UND[2] = oldcpsr;
    cpu->R[14] = cpu->R[15] - 4;
    cpu->JumpTo(cpu->ExceptionBase + 0x04);
}

void T_UNK(ARM* cpu)
{
    Log(LogLevel::Warn, "undefined THUMB%d instruction %04X @ %08X\n", cpu->Num?7:9, cpu->CurInstr, cpu->R[15]-4);
#ifdef GDBSTUB_ENABLED
    cpu->GdbStub.Enter(true, Gdb::TgtStatus::FaultInsn, cpu->R[15]-4);
#endif
    //NDS::Halt();
    u32 oldcpsr = cpu->CPSR;
    cpu->CPSR &= ~0xBF;
    cpu->CPSR |= 0x9B;
    cpu->UpdateMode(oldcpsr, cpu->CPSR);

    cpu->R_UND[2] = oldcpsr;
    cpu->R[14] = cpu->R[15] - 2;
    cpu->JumpTo(cpu->ExceptionBase + 0x04);
}



void A_MSR_IMM(ARM* cpu)
{
    u32* psr;
    if (cpu->CurInstr & (1<<22))
    {
        switch (cpu->CPSR & 0x1F)
        {
            case 0x11: psr = &cpu->R_FIQ[7]; break;
            case 0x12: psr = &cpu->R_IRQ[2]; break;
            case 0x13: psr = &cpu->R_SVC[2]; break;
            case 0x14:
            case 0x15:
            case 0x16:
            case 0x17: psr = &cpu->R_ABT[2]; break;
            case 0x18:
            case 0x19:
            case 0x1A:
            case 0x1B: psr = &cpu->R_UND[2]; break;
            default:
                cpu->AddCycles_C();
                return;
        }
    }
    else
        psr = &cpu->CPSR;

    u32 oldpsr = *psr;

    u32 mask = 0;
    if (cpu->CurInstr & (1<<16)) mask |= 0x000000FF;
    if (cpu->CurInstr & (1<<17)) mask |= 0x0000FF00;
    if (cpu->CurInstr & (1<<18)) mask |= 0x00FF0000;
    if (cpu->CurInstr & (1<<19)) mask |= 0xFF000000;

    if (!(cpu->CurInstr & (1<<22)))
        mask &= 0xFFFFFFDF;

    if ((cpu->CPSR & 0x1F) == 0x10) mask &= 0xFFFFFF00;

    u32 val = ROR((cpu->CurInstr & 0xFF), ((cpu->CurInstr >> 7) & 0x1E));

    // bit4 is forced to 1
    val |= 0x00000010;

    *psr &= ~mask;
    *psr |= (val & mask);

    if (!(cpu->CurInstr & (1<<22)))
        cpu->UpdateMode(oldpsr, cpu->CPSR);

    cpu->AddCycles_C();
}

void A_MSR_REG(ARM* cpu)
{
    u32* psr;
    if (cpu->CurInstr & (1<<22))
    {
        switch (cpu->CPSR & 0x1F)
        {
            case 0x11: psr = &cpu->R_FIQ[7]; break;
            case 0x12: psr = &cpu->R_IRQ[2]; break;
            case 0x13: psr = &cpu->R_SVC[2]; break;
            case 0x14:
            case 0x15:
            case 0x16:
            case 0x17: psr = &cpu->R_ABT[2]; break;
            case 0x18:
            case 0x19:
            case 0x1A:
            case 0x1B: psr = &cpu->R_UND[2]; break;
            default:
                cpu->AddCycles_C();
                return;
        }
    }
    else
        psr = &cpu->CPSR;

    u32 oldpsr = *psr;

    u32 mask = 0;
    if (cpu->CurInstr & (1<<16)) mask |= 0x000000FF;
    if (cpu->CurInstr & (1<<17)) mask |= 0x0000FF00;
    if (cpu->CurInstr & (1<<18)) mask |= 0x00FF0000;
    if (cpu->CurInstr & (1<<19)) mask |= 0xFF000000;

    if (!(cpu->CurInstr & (1<<22)))
        mask &= 0xFFFFFFDF;

    if ((cpu->CPSR & 0x1F) == 0x10) mask &= 0xFFFFFF00;

    u32 val = cpu->R[cpu->CurInstr & 0xF];

    // bit4 is forced to 1
    val |= 0x00000010;

    *psr &= ~mask;
    *psr |= (val & mask);

    if (!(cpu->CurInstr & (1<<22)))
        cpu->UpdateMode(oldpsr, cpu->CPSR);

    cpu->AddCycles_C();
}

void A_MRS(ARM* cpu)
{
    u32 psr;
    if (cpu->CurInstr & (1<<22))
    {
        switch (cpu->CPSR & 0x1F)
        {
            case 0x11: psr = cpu->R_FIQ[7]; break;
            case 0x12: psr = cpu->R_IRQ[2]; break;
            case 0x13: psr = cpu->R_SVC[2]; break;
            case 0x14:
            case 0x15:
            case 0x16:
            case 0x17: psr = cpu->R_ABT[2]; break;
            case 0x18:
            case 0x19:
            case 0x1A:
            case 0x1B: psr = cpu->R_UND[2]; break;
            default: psr = cpu->CPSR; break;
        }
    }
    else
        psr = cpu->CPSR;

    cpu->R[(cpu->CurInstr>>12) & 0xF] = psr;
    cpu->AddCycles_C();
}


void A_MCR(ARM* cpu)
{
    if ((cpu->CPSR & 0x1F) == 0x10)
        return A_UNK(cpu);

    u32 cp = (cpu->CurInstr >> 8) & 0xF;
    //u32 op = (cpu->CurInstr >> 21) & 0x7;
    u32 cn = (cpu->CurInstr >> 16) & 0xF;
    u32 cm = cpu->CurInstr & 0xF;
    u32 cpinfo = (cpu->CurInstr >> 5) & 0x7;

    if (cpu->Num==0 && cp==15)
    {
        ((ARMv5*)cpu)->CP15Write((cn<<8)|(cm<<4)|cpinfo, cpu->R[(cpu->CurInstr>>12)&0xF]);
    }
    else if (cpu->Num==1 && cp==14)
    {
        Log(LogLevel::Debug, "MCR p14,%d,%d,%d on ARM7\n", cn, cm, cpinfo);
    }
    else
    {
        Log(LogLevel::Warn, "bad MCR opcode p%d,%d,%d,%d on ARM%d\n", cp, cn, cm, cpinfo, cpu->Num?7:9);
        return A_UNK(cpu); // TODO: check what kind of exception it really is
    }

    cpu->AddCycles_CI(1 + 1); // TODO: checkme
}

void A_MRC(ARM* cpu)
{
    if ((cpu->CPSR & 0x1F) == 0x10)
        return A_UNK(cpu);

    u32 cp = (cpu->CurInstr >> 8) & 0xF;
    //u32 op = (cpu->CurInstr >> 21) & 0x7;
    u32 cn = (cpu->CurInstr >> 16) & 0xF;
    u32 cm = cpu->CurInstr & 0xF;
    u32 cpinfo = (cpu->CurInstr >> 5) & 0x7;

    if (cpu->Num==0 && cp==15)
    {
        cpu->R[(cpu->CurInstr>>12)&0xF] = ((ARMv5*)cpu)->CP15Read((cn<<8)|(cm<<4)|cpinfo);
    }
    else if (cpu->Num==1 && cp==14)
    {
        Log(LogLevel::Debug, "MRC p14,%d,%d,%d on ARM7\n", cn, cm, cpinfo);
    }
    else
    {
        Log(LogLevel::Warn, "bad MRC opcode p%d,%d,%d,%d on ARM%d\n", cp, cn, cm, cpinfo, cpu->Num?7:9);
        return A_UNK(cpu); // TODO: check what kind of exception it really is
    }

    cpu->AddCycles_CI(2 + 1); // TODO: checkme
}



void A_SVC(ARM* cpu)
{
    u32 oldcpsr = cpu->CPSR;
    cpu->CPSR &= ~0xBF;
    cpu->CPSR |= 0x93;
    cpu->UpdateMode(oldcpsr, cpu->CPSR);

    cpu->R_SVC[2] = oldcpsr;
    cpu->R[14] = cpu->R[15] - 4;
    cpu->JumpTo(cpu->ExceptionBase + 0x08);
}

void T_SVC(ARM* cpu)
{
    u32 oldcpsr = cpu->CPSR;
    cpu->CPSR &= ~0xBF;
    cpu->CPSR |= 0x93;
    cpu->UpdateMode(oldcpsr, cpu->CPSR);

    cpu->R_SVC[2] = oldcpsr;
    cpu->R[14] = cpu->R[15] - 2;
    cpu->JumpTo(cpu->ExceptionBase + 0x08);
}



#define INSTRFUNC_PROTO(x)  void (*x)(ARM* cpu)
#include "ARM_InstrTable.h"
#undef INSTRFUNC_PROTO

}
