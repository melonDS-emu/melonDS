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
#include "NDS.h"
#include "ARM.h"
#include "ARMInterpreter.h"


u32 ARM::ConditionTable[16] =
{
    0xF0F0, // EQ
    0x0F0F, // NE
    0xCCCC, // CS
    0x3333, // CC
    0xFF00, // MI
    0x00FF, // PL
    0xAAAA, // VS
    0x5555, // VC
    0x0C0C, // HI
    0xF3F3, // LS
    0xAA55, // GE
    0x55AA, // LT
    0x0A05, // GT
    0xF5FA, // LE
    0xFFFF, // AL
    0x0000  // NE
};


ARM::ARM(u32 num)
{
    // well uh
    Num = num;
}

ARM::~ARM()
{
    // dorp
}

void ARM::Reset()
{
    Cycles = 0;
    Halted = 0;

    for (int i = 0; i < 16; i++)
        R[i] = 0;

    CPSR = 0x000000D3;

    ExceptionBase = Num ? 0x00000000 : 0xFFFF0000;

    // zorp
    JumpTo(ExceptionBase);
}

void ARM::JumpTo(u32 addr, bool restorecpsr)
{
    if (restorecpsr)
    {
        //if (Num==1 && (CPSR&0x1F)==0x12)
        //    printf("return from IRQ %08X -> %08X, SP=%08X, %08X\n", R[15], addr, R[13], Read32(0x0380FF7C));
        RestoreCPSR();

        if (CPSR & 0x20)    addr |= 0x1;
        else                addr &= ~0x1;
    }

    if (addr & 0x1)
    {
        addr &= ~0x1;
        R[15] = addr+2;
        NextInstr = Read16(addr);
        CPSR |= 0x20;
    }
    else
    {
        addr &= ~0x3;
        R[15] = addr+4;
        NextInstr = Read32(addr);
        CPSR &= ~0x20;
    }
}

void ARM::RestoreCPSR()
{
    u32 oldcpsr = CPSR;

    switch (CPSR & 0x1F)
    {
    case 0x11:
        CPSR = R_FIQ[8];
        break;

    case 0x12:
        CPSR = R_IRQ[2];
        break;

    case 0x13:
        CPSR = R_SVC[2];
        break;

    case 0x17:
        CPSR = R_ABT[2];
        break;

    case 0x1B:
        CPSR = R_UND[2];
        break;

    default:
        printf("!! attempt to restore CPSR under bad mode %02X\n", CPSR&0x1F);
        break;
    }

    UpdateMode(oldcpsr, CPSR);
}

void ARM::UpdateMode(u32 oldmode, u32 newmode)
{
    u32 temp;
    #define SWAP(a, b)  temp = a; a = b; b = temp;

    if ((oldmode & 0x1F) == (newmode & 0x1F)) return;

    switch (oldmode & 0x1F)
    {
    case 0x11:
        SWAP(R[8], R_FIQ[0]);
        SWAP(R[9], R_FIQ[1]);
        SWAP(R[10], R_FIQ[2]);
        SWAP(R[11], R_FIQ[3]);
        SWAP(R[12], R_FIQ[4]);
        SWAP(R[13], R_FIQ[5]);
        SWAP(R[14], R_FIQ[6]);
        break;

    case 0x12:
        SWAP(R[13], R_IRQ[0]);
        SWAP(R[14], R_IRQ[1]);
        break;

    case 0x13:
        SWAP(R[13], R_SVC[0]);
        SWAP(R[14], R_SVC[1]);
        break;

    case 0x17:
        SWAP(R[13], R_ABT[0]);
        SWAP(R[14], R_ABT[1]);
        break;

    case 0x1B:
        SWAP(R[13], R_UND[0]);
        SWAP(R[14], R_UND[1]);
        break;
    }

    switch (newmode & 0x1F)
    {
    case 0x11:
        SWAP(R[8], R_FIQ[0]);
        SWAP(R[9], R_FIQ[1]);
        SWAP(R[10], R_FIQ[2]);
        SWAP(R[11], R_FIQ[3]);
        SWAP(R[12], R_FIQ[4]);
        SWAP(R[13], R_FIQ[5]);
        SWAP(R[14], R_FIQ[6]);
        break;

    case 0x12:
        SWAP(R[13], R_IRQ[0]);
        SWAP(R[14], R_IRQ[1]);
        break;

    case 0x13:
        SWAP(R[13], R_SVC[0]);
        SWAP(R[14], R_SVC[1]);
        break;

    case 0x17:
        SWAP(R[13], R_ABT[0]);
        SWAP(R[14], R_ABT[1]);
        break;

    case 0x1B:
        SWAP(R[13], R_UND[0]);
        SWAP(R[14], R_UND[1]);
        break;
    }

    #undef SWAP
}

void ARM::TriggerIRQ()
{
    if (CPSR & 0x80)
        return;

    u32 oldcpsr = CPSR;
    CPSR &= ~0xFF;
    CPSR |= 0xD2;
    UpdateMode(oldcpsr, CPSR);

    R_IRQ[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 2 : 0);
    JumpTo(ExceptionBase + 0x18);
}

s32 ARM::Execute(s32 cycles)
{
    if (Halted)
    {
        if (NDS::HaltInterrupted(Num))
            Halted = 0;
        else
            return cycles;
    }

    s32 cyclesrun = 0;
    u32 addr = R[15] - (CPSR&0x20 ? 4:8);
    u32 cpsr = CPSR;

    while (cyclesrun < cycles)
    {
        //if(Num==1)printf("%08X %08X\n",  R[15] - (CPSR&0x20 ? 4:8), NextInstr);

        if (CPSR & 0x20) // THUMB
        {
            // prefetch
            CurInstr = NextInstr;
            NextInstr = Read16(R[15]);
            //cyclesrun += MemWaitstate(0, R[15]);
            R[15] += 2;

            Cycles = cyclesrun;

            // actually execute
            u32 icode = (CurInstr >> 6);
            cyclesrun += ARMInterpreter::THUMBInstrTable[icode](this);
        }
        else
        {
            // prefetch
            CurInstr = NextInstr;
            NextInstr = Read32(R[15]);
            //cyclesrun += MemWaitstate(1, R[15]);
            R[15] += 4;

            Cycles = cyclesrun;

            // actually execute
            if (CheckCondition(CurInstr >> 28))
            {
                u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
                cyclesrun += ARMInterpreter::ARMInstrTable[icode](this);
            }
            else if ((CurInstr & 0xFE000000) == 0xFA000000)
            {
                cyclesrun += ARMInterpreter::A_BLX_IMM(this);
            }
            else
            {
                // not executing it. oh well
                cyclesrun += 1; // 1S. todo: check
            }
        }

        // TODO optimize this shit!!!
        if (Halted) return cycles;
        if (NDS::HaltInterrupted(Num))
        {
            if (NDS::IME[Num]&1)
                TriggerIRQ();
        }

        // temp. debug cruft
        addr = R[15] - (CPSR&0x20 ? 4:8);
        cpsr = CPSR;
    }

    return cyclesrun;
}
