
#ifndef ARMINTERPRETER_BRANCH_H
#define ARMINTERPRETER_BRANCH_H

namespace ARMInterpreter
{

s32 A_B(ARM* cpu);
s32 A_BL(ARM* cpu);
s32 A_BX(ARM* cpu);
s32 A_BLX_REG(ARM* cpu);

s32 T_BCOND(ARM* cpu);
s32 T_BX(ARM* cpu);
s32 T_BLX_REG(ARM* cpu);

}

#endif
