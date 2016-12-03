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



}

