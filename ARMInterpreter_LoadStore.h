
#ifndef ARMINTERPRETER_LOADSTORE_H
#define ARMINTERPRETER_LOADSTORE_H

namespace ARMInterpreter
{

#define A_PROTO_WB_LDRSTR(x) \
\
s32 A_##x##_IMM(ARM* cpu); \
s32 A_##x##_REG_LSL(ARM* cpu); \
s32 A_##x##_REG_LSR(ARM* cpu); \
s32 A_##x##_REG_ASR(ARM* cpu); \
s32 A_##x##_REG_ROR(ARM* cpu); \
s32 A_##x##_POST_IMM(ARM* cpu); \
s32 A_##x##_POST_REG_LSL(ARM* cpu); \
s32 A_##x##_POST_REG_LSR(ARM* cpu); \
s32 A_##x##_POST_REG_ASR(ARM* cpu); \
s32 A_##x##_POST_REG_ROR(ARM* cpu);

A_PROTO_WB_LDRSTR(STR)
A_PROTO_WB_LDRSTR(STRB)
A_PROTO_WB_LDRSTR(LDR)
A_PROTO_WB_LDRSTR(LDRB)

}

#endif

