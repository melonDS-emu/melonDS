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
    if (cpu->CheckInterlock) return;
    cpu->AddCycles_C();
    Log(LogLevel::Warn, "undefined ARM%d instruction %08X @ %08X\n", cpu->Num?7:9, cpu->CurInstr, cpu->R[15]-8);
#ifdef GDBSTUB_ENABLED
    cpu->GdbStub.Enter(cpu->GdbStub.IsConnected(), Gdb::TgtStatus::FaultInsn, cpu->R[15]-8);
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
    if (cpu->CheckInterlock) return;
    cpu->AddCycles_C();
    Log(LogLevel::Warn, "undefined THUMB%d instruction %04X @ %08X\n", cpu->Num?7:9, cpu->CurInstr, cpu->R[15]-4);
#ifdef GDBSTUB_ENABLED
    cpu->GdbStub.Enter(cpu->GdbStub.IsConnected(), Gdb::TgtStatus::FaultInsn, cpu->R[15]-4);
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

void A_BKPT(ARM* cpu)
{
    if (cpu->CheckInterlock) return;
    if (cpu->Num == 1) return A_UNK(cpu); // checkme
    
    Log(LogLevel::Warn, "BKPT: "); // combine with the prefetch abort warning message
    ((ARMv5*)cpu)->PrefetchAbort();
}



void A_MSR_IMM(ARM* cpu)
{
    if (cpu->CheckInterlock) return;
    if ((cpu->Num != 1) && (cpu->CurInstr & ((0x7<<16)|(1<<22)))) cpu->AddCycles_CI(2); // arm9 cpsr_sxc & spsr
    else cpu->AddCycles_C();

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
                return;
        }
    }
    else
        psr = &cpu->CPSR;

    u32 oldpsr = *psr;

    u32 mask = 0;
    if (cpu->CurInstr & (1<<16)) mask |= 0x000000FF;
    //if (cpu->CurInstr & (1<<17)) mask |= 0x0000FF00; // unused by arm 7 & 9
    //if (cpu->CurInstr & (1<<18)) mask |= 0x00FF0000; // unused by arm 7 & 9
    if (cpu->CurInstr & (1<<19)) mask |= ((cpu->Num==1) ? 0xF0000000 : 0xF8000000);

    if ((cpu->CPSR & 0x1F) == 0x10) mask &= 0xFFFFFF00;

    u32 val = ROR((cpu->CurInstr & 0xFF), ((cpu->CurInstr >> 7) & 0x1E));

    // bit4 is forced to 1
    val |= 0x00000010;

    *psr &= ~mask;
    *psr |= (val & mask);

    if (!(cpu->CurInstr & (1<<22)))
        cpu->UpdateMode(oldpsr, cpu->CPSR);

    if (cpu->CPSR & 0x20) [[unlikely]]
    {
        if (cpu->Num == 0)
        {
            cpu->R[15] += 2; // pc should actually increment by 4 one more time after switching to thumb mode without a pipeline flush, this gets the same effect.
            ((ARMv5*)cpu)->StartExec = &ARMv5::StartExecTHUMB;
            if (cpu->MRTrack.Type == MainRAMType::Null) ((ARMv5*)cpu)->FuncQueue[0] = ((ARMv5*)cpu)->StartExec;
        }
        else
        {
            Platform::Log(Platform::LogLevel::Warn, "UNIMPLEMENTED: MSR REG T bit change on ARM7\n");
            cpu->CPSR &= ~0x20; // keep it from crashing the emulator at least
        }
    }
}

void A_MSR_REG(ARM* cpu)
{
    if (cpu->CheckInterlock) return ((ARMv5*)cpu)->HandleInterlocksExecute<false>(cpu->CurInstr & 0xF);

    if ((cpu->Num != 1) && (cpu->CurInstr & ((0x7<<16)|(1<<22)))) cpu->AddCycles_CI(2); // arm9 cpsr_sxc & spsr
    else cpu->AddCycles_C();

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
                return;
        }
    }
    else
        psr = &cpu->CPSR;

    u32 oldpsr = *psr;

    u32 mask = 0;
    if (cpu->CurInstr & (1<<16)) mask |= 0x000000FF;
    //if (cpu->CurInstr & (1<<17)) mask |= 0x0000FF00; // unused by arm 7 & 9
    //if (cpu->CurInstr & (1<<18)) mask |= 0x00FF0000; // unused by arm 7 & 9
    if (cpu->CurInstr & (1<<19)) mask |= ((cpu->Num==1) ? 0xF0000000 : 0xF8000000);

    if ((cpu->CPSR & 0x1F) == 0x10) mask &= 0xFFFFFF00;

    u32 val = cpu->R[cpu->CurInstr & 0xF];

    // bit4 is forced to 1
    val |= 0x00000010;

    *psr &= ~mask;
    *psr |= (val & mask);

    if (!(cpu->CurInstr & (1<<22)))
        cpu->UpdateMode(oldpsr, cpu->CPSR);

    if (cpu->CPSR & 0x20) [[unlikely]]
    {
        if (cpu->Num == 0)
        {
            cpu->R[15] += 2; // pc should actually increment by 4 one more time after switching to thumb mode without a pipeline flush, this gets the same effect.
            ((ARMv5*)cpu)->StartExec = &ARMv5::StartExecTHUMB;
            if (cpu->MRTrack.Type == MainRAMType::Null) ((ARMv5*)cpu)->FuncQueue[0] = ((ARMv5*)cpu)->StartExec;
        }
        else
        {
            Platform::Log(Platform::LogLevel::Warn, "UNIMPLEMENTED: MSR REG T bit change on ARM7\n");
            cpu->CPSR &= ~0x20; // keep it from crashing the emulator at least
        }
    }
}

void A_MRS(ARM* cpu)
{
    if (cpu->CheckInterlock) return;
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

    if (cpu->Num != 1) // arm9
    {
        cpu->AddCycles_CI(2); // 1 X
        ((ARMv5*)cpu)->AddCycles_MW(2); // 2 M
    }
    else cpu->AddCycles_C(); // arm7

    if (((cpu->CurInstr>>12) & 0xF) == 15)
    {
        if (cpu->Num == 1) // doesn't seem to jump on the arm9? checkme
            cpu->JumpTo(psr & ~0x1); // checkme: this shouldn't be able to switch to thumb?
    }
    else cpu->R[(cpu->CurInstr>>12) & 0xF] = psr;
}


void A_MCR(ARM* cpu)
{
    if ((cpu->CPSR & 0x1F) == 0x10)
        return A_UNK(cpu);

    if (cpu->CheckInterlock) return ((ARMv5*)cpu)->HandleInterlocksExecute<false>((cpu->CurInstr>>12)&0xF);

    u32 cp = (cpu->CurInstr >> 8) & 0xF;
    u32 op = (cpu->CurInstr >> 21) & 0x7;
    u32 cn = (cpu->CurInstr >> 16) & 0xF;
    u32 cm = cpu->CurInstr & 0xF;
    u32 cpinfo = (cpu->CurInstr >> 5) & 0x7;
    u32 val = cpu->R[(cpu->CurInstr>>12)&0xF];
    if (((cpu->CurInstr>>12) & 0xF) == 15) val += 4;

    if (cpu->Num==0 && cp==15)
    {
        ((ARMv5*)cpu)->CP15Write((cn<<8)|(cm<<4)|cpinfo|(op<<12), val);
    }
    else if (cpu->Num==1 && cp==14)
    {
        Log(LogLevel::Debug, "MCR p14,%d,%d,%d on ARM7\n", cn, cm, cpinfo);
    }
    else
    {
        Log(LogLevel::Warn, "bad MCR opcode p%d, %d, reg, c%d, c%d, %d on ARM%d\n", cp, op, cn, cm, cpinfo, cpu->Num?7:9);
        return A_UNK(cpu); // TODO: check what kind of exception it really is
    }
    
    if (cpu->Num==0) cpu->AddCycles_CI(5); // checkme
    else /* ARM7 */  cpu->AddCycles_CI(1 + 1); // TODO: checkme
}

void A_MRC(ARM* cpu)
{
    if ((cpu->CPSR & 0x1F) == 0x10)
        return A_UNK(cpu);

    if (cpu->CheckInterlock) return ((ARMv5*)cpu)->HandleInterlocksExecute<false>((cpu->CurInstr>>12)&0xF);

    u32 cp = (cpu->CurInstr >> 8) & 0xF;
    u32 op = (cpu->CurInstr >> 21) & 0x7;
    u32 cn = (cpu->CurInstr >> 16) & 0xF;
    u32 cm = cpu->CurInstr & 0xF;
    u32 cpinfo = (cpu->CurInstr >> 5) & 0x7;
    u32 rd = (cpu->CurInstr>>12) & 0xF;

    if (cpu->Num==0 && cp==15)
    {
        if (rd != 15) cpu->R[rd] = ((ARMv5*)cpu)->CP15Read((cn<<8)|(cm<<4)|cpinfo|(op<<12));
        else
        {
            // r15 updates the top 4 bits of the cpsr, done to "allow for conditional branching based on coprocessor status"
            u32 flags = ((ARMv5*)cpu)->CP15Read((cn<<8)|(cm<<4)|cpinfo|(op<<12)) & 0xF0000000;
            cpu->CPSR = (cpu->CPSR & ~0xF0000000) | flags;
        }
    }
    else if (cpu->Num==1 && cp==14)
    {
        Log(LogLevel::Debug, "MRC p14,%d,%d,%d on ARM7\n", cn, cm, cpinfo);
    }
    else
    {
        Log(LogLevel::Warn, "bad MRC opcode p%d, %d, reg, c%d, c%d, %d on ARM%d\n", cp, op, cn, cm, cpinfo, cpu->Num?7:9);
        return A_UNK(cpu); // TODO: check what kind of exception it really is
    }

    if (cpu->Num != 1)
    {
        cpu->AddCycles_CI(2); // 1 Execute cycle
        ((ARMv5*)cpu)->AddCycles_MW(2); // 2 Memory cycles
        ((ARMv5*)cpu)->SetupInterlock((cpu->CurInstr >> 12) & 0xF);
    }
    else cpu->AddCycles_CI(2 + 1); // TODO: checkme
}



void A_SVC(ARM* cpu) // A_SWI
{
    if (cpu->CheckInterlock) return;
    cpu->AddCycles_C();
    u32 oldcpsr = cpu->CPSR;
    cpu->CPSR &= ~0xBF;
    cpu->CPSR |= 0x93;
    cpu->UpdateMode(oldcpsr, cpu->CPSR);

    cpu->R_SVC[2] = oldcpsr;
    cpu->R[14] = cpu->R[15] - 4;

    cpu->JumpTo(cpu->ExceptionBase + 0x08);
}

void T_SVC(ARM* cpu) // T_SWI
{
    if (cpu->CheckInterlock) return;
    cpu->AddCycles_C();
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
