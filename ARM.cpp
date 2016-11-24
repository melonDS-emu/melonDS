#include <stdio.h>
#include "NDS.h"
#include "ARM.h"
#include "ARMInterpreter.h"


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

    ExceptionBase = Num ? 0x00000000 : 0xFFFF0000;

    // zorp
    JumpTo(ExceptionBase);
}

void ARM::JumpTo(u32 addr)
{
    // pipeline shit

    // TODO: THUMB!!
    if (addr&1) printf("!!! THUMB JUMP\n");

    NextInstr = Read32(addr);
    R[15] = addr+4;
}

s32 ARM::Execute(s32 cycles)
{
    while (cycles > 0)
    {
        // TODO THUM SHIT ASGAFDGSUHAJISGFYAUISAGY

        // prefetch
        CurInstr = NextInstr;
        NextInstr = Read32(R[15]);
        R[15] += 4;

        // actually execute
        if ((CurInstr & 0xF0000000) != 0xE0000000) printf("well shit\n");
        u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
        cycles -= ARMInterpreter::ARMInstrTable[icode](this);
    }

    return cycles;
}
