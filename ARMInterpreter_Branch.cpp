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



}

