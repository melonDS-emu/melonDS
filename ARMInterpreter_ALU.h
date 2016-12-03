
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

}

#endif
