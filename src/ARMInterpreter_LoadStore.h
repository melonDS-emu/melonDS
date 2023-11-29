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

#ifndef ARMINTERPRETER_LOADSTORE_H
#define ARMINTERPRETER_LOADSTORE_H

namespace melonDS::ARMInterpreter
{

#define A_PROTO_WB_LDRSTR(x) \
\
void A_##x##_IMM(ARM* cpu); \
void A_##x##_REG_LSL(ARM* cpu); \
void A_##x##_REG_LSR(ARM* cpu); \
void A_##x##_REG_ASR(ARM* cpu); \
void A_##x##_REG_ROR(ARM* cpu); \
void A_##x##_POST_IMM(ARM* cpu); \
void A_##x##_POST_REG_LSL(ARM* cpu); \
void A_##x##_POST_REG_LSR(ARM* cpu); \
void A_##x##_POST_REG_ASR(ARM* cpu); \
void A_##x##_POST_REG_ROR(ARM* cpu);

A_PROTO_WB_LDRSTR(STR)
A_PROTO_WB_LDRSTR(STRB)
A_PROTO_WB_LDRSTR(LDR)
A_PROTO_WB_LDRSTR(LDRB)

#define A_PROTO_HD_LDRSTR(x) \
\
void A_##x##_IMM(ARM* cpu); \
void A_##x##_REG(ARM* cpu); \
void A_##x##_POST_IMM(ARM* cpu); \
void A_##x##_POST_REG(ARM* cpu);

A_PROTO_HD_LDRSTR(STRH)
A_PROTO_HD_LDRSTR(LDRD)
A_PROTO_HD_LDRSTR(STRD)
A_PROTO_HD_LDRSTR(LDRH)
A_PROTO_HD_LDRSTR(LDRSB)
A_PROTO_HD_LDRSTR(LDRSH)

void A_LDM(ARM* cpu);
void A_STM(ARM* cpu);

void A_SWP(ARM* cpu);
void A_SWPB(ARM* cpu);


void T_LDR_PCREL(ARM* cpu);

void T_STR_REG(ARM* cpu);
void T_STRB_REG(ARM* cpu);
void T_LDR_REG(ARM* cpu);
void T_LDRB_REG(ARM* cpu);

void T_STRH_REG(ARM* cpu);
void T_LDRSB_REG(ARM* cpu);
void T_LDRH_REG(ARM* cpu);
void T_LDRSH_REG(ARM* cpu);

void T_STR_IMM(ARM* cpu);
void T_LDR_IMM(ARM* cpu);
void T_STRB_IMM(ARM* cpu);
void T_LDRB_IMM(ARM* cpu);

void T_STRH_IMM(ARM* cpu);
void T_LDRH_IMM(ARM* cpu);

void T_STR_SPREL(ARM* cpu);
void T_LDR_SPREL(ARM* cpu);

void T_PUSH(ARM* cpu);
void T_POP(ARM* cpu);
void T_STMIA(ARM* cpu);
void T_LDMIA(ARM* cpu);

}

#endif

