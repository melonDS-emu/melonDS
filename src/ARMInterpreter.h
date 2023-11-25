/*
    Copyright 2016-2023 melonDS team

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

#ifndef ARMINTERPRETER_H
#define ARMINTERPRETER_H

#include "types.h"
#include "ARM.h"

namespace melonDS
{
namespace ARMInterpreter
{

extern void (*ARMInstrTable[4096])(ARM* cpu);
extern void (*THUMBInstrTable[1024])(ARM* cpu);

void A_MSR_IMM(ARM* cpu);
void A_MSR_REG(ARM* cpu);
void A_MRS(ARM* cpu);
void A_MCR(ARM* cpu);
void A_MRC(ARM* cpu);
void A_SVC(ARM* cpu);

void T_SVC(ARM* cpu);

void A_BLX_IMM(ARM* cpu); // I'm a special one look at me

}

}
#endif // ARMINTERPRETER_H
