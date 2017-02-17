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
#include "GPU3D.h"


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

    for (int i = 0; i < 16; i++)
    {
        Waitstates[0][i] = 1;
        Waitstates[1][i] = 1;
        Waitstates[2][i] = 1;
        Waitstates[3][i] = 1;
    }

    if (!num)
    {
        // ARM9
        Waitstates[0][0x2] = 1; // main RAM timing, assuming cache hit
        Waitstates[0][0x3] = 4;
        Waitstates[0][0x4] = 4;
        Waitstates[0][0x5] = 5;
        Waitstates[0][0x6] = 5;
        Waitstates[0][0x7] = 4;
        Waitstates[0][0x8] = 19;
        Waitstates[0][0x9] = 19;
        Waitstates[0][0xF] = 4;

        Waitstates[1][0x2] = 1;
        Waitstates[1][0x3] = 8;
        Waitstates[1][0x4] = 8;
        Waitstates[1][0x5] = 10;
        Waitstates[1][0x6] = 10;
        Waitstates[1][0x7] = 8;
        Waitstates[1][0x8] = 38;
        Waitstates[1][0x9] = 38;
        Waitstates[1][0xF] = 8;

        Waitstates[2][0x2] = 1;
        Waitstates[2][0x3] = 2;
        Waitstates[2][0x4] = 2;
        Waitstates[2][0x5] = 2;
        Waitstates[2][0x6] = 2;
        Waitstates[2][0x7] = 2;
        Waitstates[2][0x8] = 12;
        Waitstates[2][0x9] = 12;
        Waitstates[2][0xA] = 20;
        Waitstates[2][0xF] = 2;

        Waitstates[3][0x2] = 1;
        Waitstates[3][0x3] = 2;
        Waitstates[3][0x4] = 2;
        Waitstates[3][0x5] = 4;
        Waitstates[3][0x6] = 4;
        Waitstates[3][0x7] = 2;
        Waitstates[3][0x8] = 24;
        Waitstates[3][0x9] = 24;
        Waitstates[3][0xA] = 20;
        Waitstates[3][0xF] = 2;
    }
    else
    {
        // ARM7
        Waitstates[0][0x0] = 1;
        Waitstates[0][0x2] = 1;
        Waitstates[0][0x3] = 1;
        Waitstates[0][0x4] = 1;
        Waitstates[0][0x6] = 1;
        Waitstates[0][0x8] = 6;
        Waitstates[0][0x9] = 6;

        Waitstates[1][0x0] = 1;
        Waitstates[1][0x2] = 2;
        Waitstates[1][0x3] = 1;
        Waitstates[1][0x4] = 1;
        Waitstates[1][0x6] = 2;
        Waitstates[1][0x8] = 12;
        Waitstates[1][0x9] = 12;

        Waitstates[2][0x0] = 1;
        Waitstates[2][0x2] = 1;
        Waitstates[2][0x3] = 1;
        Waitstates[2][0x4] = 1;
        Waitstates[2][0x6] = 1;
        Waitstates[2][0x8] = 6;
        Waitstates[2][0x9] = 6;
        Waitstates[2][0xA] = 10;

        Waitstates[3][0x0] = 1;
        Waitstates[3][0x2] = 2;
        Waitstates[3][0x3] = 1;
        Waitstates[3][0x4] = 1;
        Waitstates[3][0x6] = 2;
        Waitstates[3][0x8] = 12;
        Waitstates[3][0x9] = 12;
        Waitstates[3][0xA] = 10;
    }
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
        RestoreCPSR();

        if (CPSR & 0x20)    addr |= 0x1;
        else                addr &= ~0x1;
    }

    if (addr & 0x1)
    {
        addr &= ~0x1;
        R[15] = addr+2;
        NextInstr[0] = CodeRead16(addr);
        NextInstr[1] = CodeRead16(addr+2);
        CPSR |= 0x20;
    }
    else
    {
        addr &= ~0x3;
        R[15] = addr+4;
        NextInstr[0] = CodeRead32(addr);
        NextInstr[1] = CodeRead32(addr+4);
        CPSR &= ~0x20;
    }
}

void ARM::RestoreCPSR()
{
    u32 oldcpsr = CPSR;

    switch (CPSR & 0x1F)
    {
    case 0x11:
        CPSR = R_FIQ[7];
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
        printf("!! attempt to restore CPSR under bad mode %02X, %08X\n", CPSR&0x1F, R[15]);
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

s32 ARM::Execute()
{
    if (Halted)
    {
        if (NDS::HaltInterrupted(Num))
        {
            Halted = 0;
            if (NDS::IME[Num]&1)
                TriggerIRQ();
        }
        else
        {
            Cycles = CyclesToRun;
            GPU3D::Run(CyclesToRun >> 1);
            return Cycles;
        }
    }

    Cycles = 0;
    s32 lastcycles = 0;
    u32 addr = R[15] - (CPSR&0x20 ? 4:8);
    u32 cpsr = CPSR;

    while (Cycles < CyclesToRun)
    {
        //if(Num==1)printf("%08X %08X\n",  R[15] - (CPSR&0x20 ? 4:8), NextInstr);

        if (CPSR & 0x20) // THUMB
        {
            // prefetch
            R[15] += 2;
            CurInstr = NextInstr[0];
            NextInstr[0] = NextInstr[1];
            NextInstr[1] = CodeRead16(R[15]);

            // actually execute
            u32 icode = (CurInstr >> 6);
            ARMInterpreter::THUMBInstrTable[icode](this);
        }
        else
        {
            // prefetch
            R[15] += 4;
            CurInstr = NextInstr[0];
            NextInstr[0] = NextInstr[1];
            NextInstr[1] = CodeRead32(R[15]);

            // actually execute
            if (CheckCondition(CurInstr >> 28))
            {
                u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
                ARMInterpreter::ARMInstrTable[icode](this);
            }
            else if ((CurInstr & 0xFE000000) == 0xFA000000)
            {
                ARMInterpreter::A_BLX_IMM(this);
            }
        }

        //if (R[15]==0x037F9364) printf("R8=%08X R9=%08X\n", R[8], R[9]);

        // gross hack
        // TODO, though: move timer code here too?
        // quick testing shows that moving this to the NDS loop doesn't really slow things down
        if (Num==0)
        {
            s32 diff = Cycles - lastcycles;
            GPU3D::Run(diff >> 1);
            lastcycles = Cycles - (diff&1);
        }

        // TODO optimize this shit!!!
        if (Halted)
        {
            if (Halted == 1)
                Cycles = CyclesToRun;
            break;
        }
        if (NDS::HaltInterrupted(Num))
        {
            if (NDS::IME[Num]&1)
                TriggerIRQ();
        }

        // temp. debug cruft
        addr = R[15] - (CPSR&0x20 ? 4:8);
        cpsr = CPSR;
    }

    if (Halted == 2)
        Halted = 0;

    return Cycles;
}
