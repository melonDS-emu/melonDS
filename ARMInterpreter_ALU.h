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

#ifndef ARMINTERPRETER_ALU_H
#define ARMINTERPRETER_ALU_H

namespace ARMInterpreter
{

#define A_PROTO_ALU_OP(x) \
\
s32 A_##x##_IMM(ARM* cpu); \
s32 A_##x##_REG_LSL_IMM(ARM* cpu); \
s32 A_##x##_REG_LSR_IMM(ARM* cpu); \
s32 A_##x##_REG_ASR_IMM(ARM* cpu); \
s32 A_##x##_REG_ROR_IMM(ARM* cpu); \
s32 A_##x##_REG_LSL_REG(ARM* cpu); \
s32 A_##x##_REG_LSR_REG(ARM* cpu); \
s32 A_##x##_REG_ASR_REG(ARM* cpu); \
s32 A_##x##_REG_ROR_REG(ARM* cpu); \
s32 A_##x##_IMM_S(ARM* cpu); \
s32 A_##x##_REG_LSL_IMM_S(ARM* cpu); \
s32 A_##x##_REG_LSR_IMM_S(ARM* cpu); \
s32 A_##x##_REG_ASR_IMM_S(ARM* cpu); \
s32 A_##x##_REG_ROR_IMM_S(ARM* cpu); \
s32 A_##x##_REG_LSL_REG_S(ARM* cpu); \
s32 A_##x##_REG_LSR_REG_S(ARM* cpu); \
s32 A_##x##_REG_ASR_REG_S(ARM* cpu); \
s32 A_##x##_REG_ROR_REG_S(ARM* cpu);

#define A_PROTO_ALU_TEST(x) \
\
s32 A_##x##_IMM(ARM* cpu); \
s32 A_##x##_REG_LSL_IMM(ARM* cpu); \
s32 A_##x##_REG_LSR_IMM(ARM* cpu); \
s32 A_##x##_REG_ASR_IMM(ARM* cpu); \
s32 A_##x##_REG_ROR_IMM(ARM* cpu); \
s32 A_##x##_REG_LSL_REG(ARM* cpu); \
s32 A_##x##_REG_LSR_REG(ARM* cpu); \
s32 A_##x##_REG_ASR_REG(ARM* cpu); \
s32 A_##x##_REG_ROR_REG(ARM* cpu);

A_PROTO_ALU_OP(AND)
A_PROTO_ALU_OP(EOR)
A_PROTO_ALU_OP(SUB)
A_PROTO_ALU_OP(RSB)
A_PROTO_ALU_OP(ADD)
A_PROTO_ALU_OP(ADC)
A_PROTO_ALU_OP(SBC)
A_PROTO_ALU_OP(RSC)
A_PROTO_ALU_TEST(TST)
A_PROTO_ALU_TEST(TEQ)
A_PROTO_ALU_TEST(CMP)
A_PROTO_ALU_TEST(CMN)
A_PROTO_ALU_OP(ORR)
A_PROTO_ALU_OP(MOV)
A_PROTO_ALU_OP(BIC)
A_PROTO_ALU_OP(MVN)


s32 T_LSL_IMM(ARM* cpu);
s32 T_LSR_IMM(ARM* cpu);
s32 T_ASR_IMM(ARM* cpu);

s32 T_ADD_REG_(ARM* cpu);
s32 T_SUB_REG_(ARM* cpu);
s32 T_ADD_IMM_(ARM* cpu);
s32 T_SUB_IMM_(ARM* cpu);

s32 T_MOV_IMM(ARM* cpu);
s32 T_CMP_IMM(ARM* cpu);
s32 T_ADD_IMM(ARM* cpu);
s32 T_SUB_IMM(ARM* cpu);

s32 T_AND_REG(ARM* cpu);
s32 T_EOR_REG(ARM* cpu);
s32 T_LSL_REG(ARM* cpu);
s32 T_LSR_REG(ARM* cpu);
s32 T_ASR_REG(ARM* cpu);
s32 T_ADC_REG(ARM* cpu);
s32 T_SBC_REG(ARM* cpu);
s32 T_ROR_REG(ARM* cpu);
s32 T_TST_REG(ARM* cpu);
s32 T_NEG_REG(ARM* cpu);
s32 T_CMP_REG(ARM* cpu);
s32 T_CMN_REG(ARM* cpu);
s32 T_ORR_REG(ARM* cpu);
s32 T_MUL_REG(ARM* cpu);
s32 T_BIC_REG(ARM* cpu);
s32 T_MVN_REG(ARM* cpu);

s32 T_ADD_HIREG(ARM* cpu);
s32 T_CMP_HIREG(ARM* cpu);
s32 T_MOV_HIREG(ARM* cpu);

s32 T_ADD_PCREL(ARM* cpu);
s32 T_ADD_SPREL(ARM* cpu);
s32 T_ADD_SP(ARM* cpu);

}

#endif
