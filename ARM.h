// ARM shit

#ifndef ARM_H
#define ARM_H

#include "types.h"
#include "NDS.h"

// lame
#define C_S(x) x
#define C_N(x) x
#define C_I(x) x

#define ROR(x, n) (((x) >> (n)) | ((x) << (32-(n))))

class ARM
{
public:
    ARM(u32 num);
    ~ARM(); // destroy shit

    void Reset();

    void JumpTo(u32 addr);
    void RestoreCPSR();

    s32 Execute(s32 cycles);

    bool CheckCondition(u32 code)
    {
        if (code == 0xE) return true;
        if (ConditionTable[code] & (1 << (CPSR>>28))) return true;
        return false;
    }

    void SetC(bool c)
    {
        if (c) CPSR |= 0x20000000;
        else CPSR &= ~0x20000000;
    }

    void SetNZ(bool n, bool z)
    {
        CPSR &= ~0xC0000000;
        if (n) CPSR |= 0x80000000;
        if (z) CPSR |= 0x40000000;
    }

    void SetNZCV(bool n, bool z, bool c, bool v)
    {
        CPSR &= ~0xF0000000;
        if (n) CPSR |= 0x80000000;
        if (z) CPSR |= 0x40000000;
        if (c) CPSR |= 0x20000000;
        if (v) CPSR |= 0x10000000;
    }

    void UpdateMode(u32 oldmode, u32 newmode);


    u8 Read8(u32 addr, u32 forceuser=0)
    {
        if (!Num)
        {
            // TODO: PU shit
            return NDS::ARM9Read8(addr);
        }
        else
            return NDS::ARM7Read8(addr);
    }

    u16 Read16(u32 addr, u32 forceuser=0)
    {
        addr &= ~1;
        if (!Num)
        {
            // TODO: PU shit
            return NDS::ARM9Read16(addr);
        }
        else
            return NDS::ARM7Read16(addr);
    }

    u32 Read32(u32 addr, u32 forceuser=0)
    {
        addr &= ~3;
        if (!Num)
        {
            // TODO: PU shit
            return NDS::ARM9Read32(addr);
        }
        else
            return NDS::ARM7Read32(addr);
    }

    void Write8(u32 addr, u8 val, u32 forceuser=0)
    {
        if (!Num)
        {
            // TODO: PU shit
            NDS::ARM9Write8(addr, val);
        }
        else
            NDS::ARM7Write8(addr, val);
    }

    void Write16(u32 addr, u16 val, u32 forceuser=0)
    {
        addr &= ~1;
        if (!Num)
        {
            // TODO: PU shit
            NDS::ARM9Write16(addr, val);
        }
        else
            NDS::ARM7Write16(addr, val);
    }

    void Write32(u32 addr, u32 val, u32 forceuser=0)
    {
        addr &= ~3;
        if (!Num)
        {
            // TODO: PU shit
            NDS::ARM9Write32(addr, val);
        }
        else
            NDS::ARM7Write32(addr, val);
    }


    s32 MemWaitstate(u32 type, u32 addr)
    {
        // type:
        // 0 = code16
        // 1 = code32
        // 2 = data16
        // 3 = data32

        return 1; // sorry
    }


    u32 Num;

    u32 R[16]; // heh
    u32 CPSR;
    u32 R_FIQ[8]; // holding SPSR too
    u32 R_SVC[3];
    u32 R_ABT[3];
    u32 R_IRQ[3];
    u32 R_UND[3];
    u32 CurInstr;
    u32 NextInstr;

    u32 ExceptionBase;

    static u32 ConditionTable[16];
};

#endif // ARM_H
