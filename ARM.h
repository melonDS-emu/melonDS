// ARM shit

#ifndef ARM_H
#define ARM_H

#include "types.h"
#include "NDS.h"

// lame
#define C_S(x) x
#define C_N(x) x

class ARM
{
public:
    ARM(u32 num);
    ~ARM(); // destroy shit

    void Reset();

    void JumpTo(u32 addr);
    s32 Execute(s32 cycles);

    u32 Read32(u32 addr)
    {
        if (Num) return NDS::ARM7Read32(addr);
        else     return NDS::ARM9Read32(addr);
    }


    u32 Num;

    u32 R[16]; // heh
    u32 CPSR;
    u32 R_FIQ[8]; // holding SPSR too
    u32 R_SVC[3];
    u32 R_ABT[3];
    u32 R_IRQ[3];
    u32 R_UND[3];
    u32 CurInstr;
    u32 NextInstr;

    u32 ExceptionBase;
};

#endif // ARM_H
