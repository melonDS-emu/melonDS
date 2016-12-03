
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

#define A_PROTO_HD_LDRSTR(x) \
\
s32 A_##x##_IMM(ARM* cpu); \
s32 A_##x##_REG(ARM* cpu); \
s32 A_##x##_POST_IMM(ARM* cpu); \
s32 A_##x##_POST_REG(ARM* cpu);

A_PROTO_HD_LDRSTR(STRH)
A_PROTO_HD_LDRSTR(LDRD)
A_PROTO_HD_LDRSTR(STRD)
A_PROTO_HD_LDRSTR(LDRH)
A_PROTO_HD_LDRSTR(LDRSB)
A_PROTO_HD_LDRSTR(LDRSH)


s32 T_LDR_PCREL(ARM* cpu);

s32 T_STR_REG(ARM* cpu);
s32 T_STRB_REG(ARM* cpu);
s32 T_LDR_REG(ARM* cpu);
s32 T_LDRB_REG(ARM* cpu);

}

#endif

