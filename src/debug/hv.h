
#ifndef DEBUG_HV_H
#define DEBUG_HV_H

#include <stdint.h>

#include "../ARM.h"

namespace debug
{
void swi(ARM* cpu, bool thumb, uint32_t scnum);

void snoop_insn(ARM* cpu, bool thumb, u32 insn);
}

#endif

