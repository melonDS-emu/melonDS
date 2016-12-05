/*
    Copyright 2016-2017 StapleButter

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

#include <stdio.h>
#include "ARM.h"


namespace ARMInterpreter
{


s32 A_B(ARM* cpu)
{
    s32 offset = (s32)(cpu->CurInstr << 8) >> 6;
    cpu->JumpTo(cpu->R[15] + offset);

    return C_S(2) + C_N(1);
}

s32 A_BL(ARM* cpu)
{
    s32 offset = (s32)(cpu->CurInstr << 8) >> 6;
    cpu->R[14] = cpu->R[15] - 4;
    cpu->JumpTo(cpu->R[15] + offset);

    return C_S(2) + C_N(1);
}

s32 A_BLX_IMM(ARM* cpu)
{
    s32 offset = (s32)(cpu->CurInstr << 8) >> 6;
    if (cpu->CurInstr & 0x01000000) offset += 2;
    cpu->R[14] = cpu->R[15] - 4;
    cpu->JumpTo(cpu->R[15] + offset + 1);

    return C_S(2) + C_N(1);
}

s32 A_BX(ARM* cpu)
{
    cpu->JumpTo(cpu->R[cpu->CurInstr & 0xF]);

    return C_S(2) + C_N(1);
}

s32 A_BLX_REG(ARM* cpu)
{
    u32 lr = cpu->R[15] - 4;
    cpu->JumpTo(cpu->R[cpu->CurInstr & 0xF]);
    cpu->R[14] = lr;

    return C_S(2) + C_N(1);
}



s32 T_BCOND(ARM* cpu)
{
    if (cpu->CheckCondition((cpu->CurInstr >> 8) & 0xF))
    {
        s32 offset = (s32)(cpu->CurInstr << 24) >> 23;
        cpu->JumpTo(cpu->R[15] + offset + 1);
        return C_S(2) + C_N(1);
    }
    else
        return C_S(1);
}

s32 T_BX(ARM* cpu)
{
    cpu->JumpTo(cpu->R[(cpu->CurInstr >> 3) & 0xF]);

    return C_S(2) + C_N(1);
}

s32 T_BLX_REG(ARM* cpu)
{
    if (cpu->Num==1)
    {
        printf("!! THUMB BLX_REG ON ARM7\n");
        return 1;
    }

    u32 lr = cpu->R[15] - 1;
    cpu->JumpTo(cpu->R[(cpu->CurInstr >> 3) & 0xF]);
    cpu->R[14] = lr;

    return C_S(2) + C_N(1);
}

s32 T_B(ARM* cpu)
{
    s32 offset = (s32)((cpu->CurInstr & 0x7FF) << 21) >> 20;
    cpu->JumpTo(cpu->R[15] + offset + 1);

    return C_S(2) + C_N(1);
}

s32 T_BL_LONG_1(ARM* cpu)
{
    s32 offset = (s32)((cpu->CurInstr & 0x7FF) << 21) >> 9;
    cpu->R[14] = cpu->R[15] + offset;

    return C_S(1);
}

s32 T_BL_LONG_2(ARM* cpu)
{
    s32 offset = (cpu->CurInstr & 0x7FF) << 1;
    u32 pc = cpu->R[14] + offset;
    cpu->R[14] = (cpu->R[15] - 2) | 1;

    if ((cpu->Num==1) || (cpu->CurInstr & (1<<12)))
        pc |= 1;

    cpu->JumpTo(pc);
    return C_S(2) + C_N(1);
}



}

