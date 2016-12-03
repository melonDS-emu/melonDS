#include <stdio.h>
#include "NDS.h"
#include "ARMInterpreter.h"
#include "ARMInterpreter_ALU.h"
#include "ARMInterpreter_Branch.h"
#include "ARMInterpreter_LoadStore.h"


namespace ARMInterpreter
{


s32 A_UNK(ARM* cpu)
{
    printf("undefined ARM%d instruction %08X @ %08X\n", cpu->Num?7:9, cpu->CurInstr, cpu->R[15]-8);
    for (int i = 0; i < 16; i++) printf("R%d: %08X\n", i, cpu->R[i]);
    NDS::Halt();
    return 0x7FFFFFFF;
}

s32 T_UNK(ARM* cpu)
{
    printf("undefined THUMB%d instruction %04X @ %08X\n", cpu->Num?7:9, cpu->CurInstr, cpu->R[15]-4);
    NDS::Halt();
    return 0x7FFFFFFF;
}



s32 A_MSR_IMM(ARM* cpu)
{
    u32* psr;
    if (cpu->CurInstr & (1<<22))
    {
        switch (cpu->CPSR & 0x1F)
        {
            case 0x11: psr = &cpu->R_FIQ[7]; break;
            case 0x12: psr = &cpu->R_IRQ[2]; break;
            case 0x13: psr = &cpu->R_SVC[2]; break;
            case 0x17: psr = &cpu->R_ABT[2]; break;
            case 0x1B: psr = &cpu->R_UND[2]; break;
            default: printf("bad CPU mode %08X\n", cpu->CPSR); return 1;
        }
    }
    else
        psr = &cpu->CPSR;

    u32 mask = 0;
    if (cpu->CurInstr & (1<<16)) mask |= 0x000000DF;
    if (cpu->CurInstr & (1<<17)) mask |= 0x0000FF00;
    if (cpu->CurInstr & (1<<18)) mask |= 0x00FF0000;
    if (cpu->CurInstr & (1<<19)) mask |= 0xFF000000;

    if ((cpu->CPSR & 0x1F) == 0x10) mask &= 0xFFFFFF00;

    u32 val = ROR((cpu->CurInstr & 0xFF), ((cpu->CurInstr >> 7) & 0x1E));

    *psr &= ~mask;
    *psr |= (val & mask);

    return C_S(1);
}

s32 A_MSR_REG(ARM* cpu)
{
    u32* psr;
    if (cpu->CurInstr & (1<<22))
    {
        switch (cpu->CPSR & 0x1F)
        {
            case 0x11: psr = &cpu->R_FIQ[7]; break;
            case 0x12: psr = &cpu->R_IRQ[2]; break;
            case 0x13: psr = &cpu->R_SVC[2]; break;
            case 0x17: psr = &cpu->R_ABT[2]; break;
            case 0x1B: psr = &cpu->R_UND[2]; break;
            default: printf("bad CPU mode %08X\n", cpu->CPSR); return 1;
        }
    }
    else
        psr = &cpu->CPSR;

    u32 mask = 0;
    if (cpu->CurInstr & (1<<16)) mask |= 0x000000DF;
    if (cpu->CurInstr & (1<<17)) mask |= 0x0000FF00;
    if (cpu->CurInstr & (1<<18)) mask |= 0x00FF0000;
    if (cpu->CurInstr & (1<<19)) mask |= 0xFF000000;

    if ((cpu->CPSR & 0x1F) == 0x10) mask &= 0xFFFFFF00;

    u32 val = cpu->R[cpu->CurInstr & 0xF];

    *psr &= ~mask;
    *psr |= (val & mask);

    return C_S(1);
}

s32 A_MRS(ARM* cpu)
{
    u32 psr;
    if (cpu->CurInstr & (1<<22))
    {
        switch (cpu->CPSR & 0x1F)
        {
            case 0x11: psr = cpu->R_FIQ[7]; break;
            case 0x12: psr = cpu->R_IRQ[2]; break;
            case 0x13: psr = cpu->R_SVC[2]; break;
            case 0x17: psr = cpu->R_ABT[2]; break;
            case 0x1B: psr = cpu->R_UND[2]; break;
            default: printf("bad CPU mode %08X\n", cpu->CPSR); return 1;
        }
    }
    else
        psr = cpu->CPSR;

    cpu->R[(cpu->CurInstr>>12) & 0xF] = psr;

    return C_S(1);
}



#define INSTRFUNC_PROTO(x)  s32 (*x)(ARM* cpu)
#include "ARM_InstrTable.h"
#undef INSTRFUNC_PROTO

}
