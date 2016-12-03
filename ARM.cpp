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
    for (int i = 0; i < 16; i++)
        R[i] = 0;

    CPSR = 0x000000D3;

    ExceptionBase = Num ? 0x00000000 : 0xFFFF0000;

    // zorp
    JumpTo(ExceptionBase);
}

void ARM::JumpTo(u32 addr)
{
    // pipeline shit

    if (addr&1)
    {
        addr &= ~1;
        NextInstr = Read16(addr);
        R[15] = addr+2;
        CPSR |= 0x20;
    }
    else
    {
        addr &= ~3;
        NextInstr = Read32(addr);
        R[15] = addr+4;
        CPSR &= ~0x20;
    }
}

void ARM::RestoreCPSR()
{
    printf("TODO: restore CPSR\n");
}

s32 ARM::Execute(s32 cycles)
{
    while (cycles > 0)
    {
        if (CPSR & 0x20) // THUMB
        {
            // prefetch
            CurInstr = NextInstr;
            NextInstr = Read16(R[15]);
            R[15] += 2;

            // actually execute
            u32 icode = (CurInstr >> 6);
            cycles -= ARMInterpreter::THUMBInstrTable[icode](this);
        }
        else
        {
            // prefetch
            CurInstr = NextInstr;
            NextInstr = Read32(R[15]);
            R[15] += 4;

            // actually execute
            if (CheckCondition(CurInstr >> 28))
            {
                u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
                cycles -= ARMInterpreter::ARMInstrTable[icode](this);
            }
            else if ((CurInstr & 0xFE000000) == 0xFA000000)
            {
                cycles -= ARMInterpreter::A_BLX_IMM(this);
            }
            else
            {
                // not executing it. oh well
                cycles -= 1; // 1S. todo: check
            }
        }
    }

    return cycles;
}
