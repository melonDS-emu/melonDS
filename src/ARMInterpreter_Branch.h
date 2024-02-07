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

#ifndef ARMINTERPRETER_BRANCH_H
#define ARMINTERPRETER_BRANCH_H

namespace melonDS
{
namespace ARMInterpreter
{

void A_B(ARM* cpu);
void A_BL(ARM* cpu);
void A_BX(ARM* cpu);
void A_BLX_REG(ARM* cpu);

void T_BCOND(ARM* cpu);
void T_BX(ARM* cpu);
void T_BLX_REG(ARM* cpu);
void T_B(ARM* cpu);
void T_BL_LONG_1(ARM* cpu);
void T_BL_LONG_2(ARM* cpu);

}

}
#endif
