#include "ARM.h"


#define CARRY_ADD(a, b)  ((0xFFFFFFFF-a) < b)
#define CARRY_SUB(a, b)  (a >= b)

#define OVERFLOW_ADD(a, b, res)  ((!(((a) ^ (b)) & 0x80000000)) && (((a) ^ (res)) & 0x80000000))
#define OVERFLOW_SUB(a, b, res)  ((((a) ^ (b)) & 0x80000000) && (((a) ^ (res)) & 0x80000000))


namespace ARMInterpreter
{


#define LSL_IMM(x, s) \
    x <<= s;

#define LSR_IMM(x, s) \
    if (s == 0) s = 32; \
    x >>= s;

#define ASR_IMM(x, s) \
    if (s == 0) s = 32; \
    x = ((s32)x) >> s;

#define ROR_IMM(x, s) \
    if (s == 0) \
    { \
        x = (x >> 1) | ((cpu->CPSR & 0x20000000) << 2); \
    } \
    else \
    { \
        x = ROR(x, s); \
    }

#define LSL_IMM_S(x, s) \
    if (s > 0) \
    { \
        cpu->SetC(x & (1<<(32-s))); \
        x <<= s; \
    }

#define LSR_IMM_S(x, s) \
    if (s == 0) s = 32; \
    cpu->SetC(x & (1<<(s-1))); \
    x >>= s;

#define ASR_IMM_S(x, s) \
    if (s == 0) s = 32; \
    cpu->SetC(x & (1<<(s-1))); \
    x = ((s32)x) >> s;

#define ROR_IMM_S(x, s) \
    if (s == 0) \
    { \
        cpu->SetC(x & 1); \
        x = (x >> 1) | ((cpu->CPSR & 0x20000000) << 2); \
    } \
    else \
    { \
        cpu->SetC(x & (1<<(s-1))); \
        x = ROR(x, s); \
    }

#define LSL_REG(x, s) \
    x <<= s;

#define LSR_REG(x, s) \
    x >>= s;

#define ASR_REG(x, s) \
    x = ((s32)x) >> s;

#define ROR_REG(x, s) \
    x = ROR(x, s);

#define LSL_REG_S(x, s) \
    if (s > 0) cpu->SetC(x & (1<<(32-s))); \
    x <<= s;

#define LSR_REG_S(x, s) \
    if (s > 0) cpu->SetC(x & (1<<(s-1))); \
    x >>= s;

#define ASR_REG_S(x, s) \
    if (s > 0) cpu->SetC(x & (1<<(s-1))); \
    x = ((s32)x) >> s;

#define ROR_REG_S(x, s) \
    if (s > 0) cpu->SetC(x & (1<<(s-1))); \
    x = ROR(x, s);



#define A_CALC_OP2_IMM \
    u32 b = ROR(cpu->CurInstr&0xFF, (cpu->CurInstr>>7)&0x1E);

#define A_CALC_OP2_REG_SHIFT_IMM(shiftop) \
    u32 b = cpu->R[cpu->CurInstr&0xF]; \
    u32 s = (cpu->CurInstr>>7)&0x1F; \
    shiftop(b, s);

#define A_CALC_OP2_REG_SHIFT_REG(shiftop) \
    u32 b = cpu->R[cpu->CurInstr&0xF]; \
    shiftop(b, cpu->R[(cpu->CurInstr>>8)&0xF]);


#define A_IMPLEMENT_ALU_OP(x) \
\
s32 A_##x##_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_IMM \
    A_##x(0) \
} \
s32 A_##x##_REG_LSL_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(LSL_IMM) \
    A_##x(0) \
} \
s32 A_##x##_REG_LSR_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(LSR_IMM) \
    A_##x(0) \
} \
s32 A_##x##_REG_ASR_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(ASR_IMM) \
    A_##x(0) \
} \
s32 A_##x##_REG_ROR_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(ROR_IMM) \
    A_##x(0) \
} \
s32 A_##x##_REG_LSL_REG(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(LSL_REG) \
    A_##x(1) \
} \
s32 A_##x##_REG_LSR_REG(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(LSR_REG) \
    A_##x(1) \
} \
s32 A_##x##_REG_ASR_REG(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(ASR_REG) \
    A_##x(1) \
} \
s32 A_##x##_REG_ROR_REG(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(ROR_REG) \
    A_##x(1) \
} \
s32 A_##x##_IMM_S(ARM* cpu) \
{ \
    A_CALC_OP2_IMM \
    A_##x##_S(0) \
} \
s32 A_##x##_REG_LSL_IMM_S(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(LSL_IMM_S) \
    A_##x##_S(0) \
} \
s32 A_##x##_REG_LSR_IMM_S(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(LSR_IMM_S) \
    A_##x##_S(0) \
} \
s32 A_##x##_REG_ASR_IMM_S(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(ASR_IMM_S) \
    A_##x##_S(0) \
} \
s32 A_##x##_REG_ROR_IMM_S(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(ROR_IMM_S) \
    A_##x##_S(0) \
} \
s32 A_##x##_REG_LSL_REG_S(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(LSL_REG_S) \
    A_##x##_S(1) \
} \
s32 A_##x##_REG_LSR_REG_S(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(LSR_REG_S) \
    A_##x##_S(1) \
} \
s32 A_##x##_REG_ASR_REG_S(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(ASR_REG_S) \
    A_##x##_S(1) \
} \
s32 A_##x##_REG_ROR_REG_S(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(ROR_REG_S) \
    A_##x##_S(1) \
}

#define A_IMPLEMENT_ALU_TEST(x) \
\
s32 A_##x##_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_IMM \
    A_##x(0) \
} \
s32 A_##x##_REG_LSL_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(LSL_IMM_S) \
    A_##x(0) \
} \
s32 A_##x##_REG_LSR_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(LSR_IMM_S) \
    A_##x(0) \
} \
s32 A_##x##_REG_ASR_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(ASR_IMM_S) \
    A_##x(0) \
} \
s32 A_##x##_REG_ROR_IMM(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_IMM(ROR_IMM_S) \
    A_##x(0) \
} \
s32 A_##x##_REG_LSL_REG(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(LSL_REG_S) \
    A_##x(1) \
} \
s32 A_##x##_REG_LSR_REG(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(LSR_REG_S) \
    A_##x(1) \
} \
s32 A_##x##_REG_ASR_REG(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(ASR_REG_S) \
    A_##x(1) \
} \
s32 A_##x##_REG_ROR_REG(ARM* cpu) \
{ \
    A_CALC_OP2_REG_SHIFT_REG(ROR_REG_S) \
    A_##x(1) \
}


#define A_TST(c) \
    u32 a = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 res = a & b; \
    cpu->SetNZ(res & 0x80000000, \
               !res); \
    return C_S(1) + C_I(c);

A_IMPLEMENT_ALU_TEST(TST)


#define A_TEQ(c) \
    u32 a = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 res = a ^ b; \
    cpu->SetNZ(res & 0x80000000, \
               !res); \
    return C_S(1) + C_I(c);

A_IMPLEMENT_ALU_TEST(TEQ)


#define A_CMP(c) \
    u32 a = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 res = a - b; \
    cpu->SetNZCV(res & 0x80000000, \
                 !res, \
                 CARRY_SUB(a, b), \
                 OVERFLOW_SUB(a, b, res)); \
    return C_S(1) + C_I(c);

A_IMPLEMENT_ALU_TEST(CMP)


#define A_CMN(c) \
    u32 a = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 res = a + b; \
    cpu->SetNZCV(res & 0x80000000, \
                 !res, \
                 CARRY_ADD(a, b), \
                 OVERFLOW_ADD(a, b, res)); \
    return C_S(1) + C_I(c);

A_IMPLEMENT_ALU_TEST(CMN)



#define A_ORR(c) \
    u32 a = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 res = a | b; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        cpu->JumpTo(res); \
        return C_S(2) + C_I(c) + C_N(1); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = res; \
        return C_S(1) + C_I(c); \
    }

#define A_ORR_S(c) \
    u32 a = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 res = a | b; \
    cpu->SetNZ(res & 0x80000000, \
               !res); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        cpu->JumpTo(res); \
        cpu->RestoreCPSR(); \
        return C_S(2) + C_I(c) + C_N(1); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = res; \
        return C_S(1) + C_I(c); \
    }

A_IMPLEMENT_ALU_OP(ORR)



#define A_MOV(c) \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        cpu->JumpTo(b); \
        return C_S(2) + C_I(c) + C_N(1); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = b; \
        return C_S(1) + C_I(c); \
    }

#define A_MOV_S(c) \
    cpu->SetNZ(b & 0x80000000, \
               !b); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        cpu->JumpTo(b); \
        cpu->RestoreCPSR(); \
        return C_S(2) + C_I(c) + C_N(1); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = b; \
        return C_S(1) + C_I(c); \
    }

A_IMPLEMENT_ALU_OP(MOV)



#define A_BIC(c) \
    u32 a = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 res = a & ~b; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        cpu->JumpTo(res); \
        return C_S(2) + C_I(c) + C_N(1); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = res; \
        return C_S(1) + C_I(c); \
    }

#define A_BIC_S(c) \
    u32 a = cpu->R[(cpu->CurInstr>>16) & 0xF]; \
    u32 res = a & ~b; \
    cpu->SetNZ(res & 0x80000000, \
               !res); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        cpu->JumpTo(res); \
        cpu->RestoreCPSR(); \
        return C_S(2) + C_I(c) + C_N(1); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = res; \
        return C_S(1) + C_I(c); \
    }

A_IMPLEMENT_ALU_OP(BIC)



#define A_MVN(c) \
    b = ~b; \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        cpu->JumpTo(b); \
        return C_S(2) + C_I(c) + C_N(1); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = b; \
        return C_S(1) + C_I(c); \
    }

#define A_MVN_S(c) \
    b = ~b; \
    cpu->SetNZ(b & 0x80000000, \
               !b); \
    if (((cpu->CurInstr>>12) & 0xF) == 15) \
    { \
        cpu->JumpTo(b); \
        cpu->RestoreCPSR(); \
        return C_S(2) + C_I(c) + C_N(1); \
    } \
    else \
    { \
        cpu->R[(cpu->CurInstr>>12) & 0xF] = b; \
        return C_S(1) + C_I(c); \
    }

A_IMPLEMENT_ALU_OP(MVN)


}
