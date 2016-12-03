
#ifndef ARMINTERPRETER_H
#define ARMINTERPRETER_H

#include "types.h"
#include "ARM.h"

namespace ARMInterpreter
{

s32 A_MSR_IMM(ARM* cpu);
s32 A_MSR_REG(ARM* cpu);
s32 A_MRS(ARM* cpu);

extern s32 (*ARMInstrTable[4096])(ARM* cpu);
extern s32 (*THUMBInstrTable[1024])(ARM* cpu);

s32 A_BLX_IMM(ARM* cpu); // I'm a special one look at me

}

#endif // ARMINTERPRETER_H
