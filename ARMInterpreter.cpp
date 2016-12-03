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



#define INSTRFUNC_PROTO(x)  s32 (*x)(ARM* cpu)
#include "ARM_InstrTable.h"
#undef INSTRFUNC_PROTO

}
