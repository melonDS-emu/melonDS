
#ifndef ARMINTERPRETER_H
#define ARMINTERPRETER_H

#include "types.h"
#include "ARM.h"

namespace ARMInterpreter
{

extern s32 (*ARMInstrTable[4096])(ARM* cpu);
extern s32 (*THUMBInstrTable[1024])(ARM* cpu);

}

#endif // ARMINTERPRETER_H
