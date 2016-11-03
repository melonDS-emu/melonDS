#include "ARM.h"
#include "NDS.h"


ARM::ARM(u32 num)
{
    // well uh
    Num = num;

    for (int i = 0; i < 16; i++)
        R[i] = 0;

    ExceptionBase = num ? 0x00000000 : 0xFFFF0000;

    // zorp
    JumpTo(ExceptionBase);
}

ARM::~ARM()
{
    // dorp
}

void ARM::JumpTo(u32 addr)
{
    // pipeline shit

    // TODO: THUMB!!

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
        // er...
    }

    return cycles;
}
