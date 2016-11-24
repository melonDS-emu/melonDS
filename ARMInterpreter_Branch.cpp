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


}

