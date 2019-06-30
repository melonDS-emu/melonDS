#include "ARMJIT_Compiler.h"

#include "../GPU.h"
#include "../Wifi.h"

namespace NDS
{
#define MAIN_RAM_SIZE 0x400000
extern u8* SWRAM_ARM9;
extern u32 SWRAM_ARM9Mask;
extern u8* SWRAM_ARM7;
extern u32 SWRAM_ARM7Mask;
extern u8 ARM7WRAM[];
extern u16 ARM7BIOSProt;
}

using namespace Gen;

namespace ARMJIT
{

void* ReadMemFuncs9[16];
void* ReadMemFuncs7[2][16];
void* WriteMemFuncs9[16];
void* WriteMemFuncs7[2][16];

template <typename T>
int squeezePointer(T* ptr)
{
    int truncated = (int)((u64)ptr);
    assert((T*)((u64)truncated) == ptr);
    return truncated;
}

u32 ReadVRAM9(u32 addr)
{
    switch (addr & 0x00E00000)
    {
        case 0x00000000: return GPU::ReadVRAM_ABG<u32>(addr);
        case 0x00200000: return GPU::ReadVRAM_BBG<u32>(addr);
        case 0x00400000: return GPU::ReadVRAM_AOBJ<u32>(addr);
        case 0x00600000: return GPU::ReadVRAM_BOBJ<u32>(addr);
        default:         return GPU::ReadVRAM_LCDC<u32>(addr);
    }
}

void WriteVRAM9(u32 addr, u32 val)
{
    switch (addr & 0x00E00000)
    {
        case 0x00000000: GPU::WriteVRAM_ABG<u32>(addr, val); return;
        case 0x00200000: GPU::WriteVRAM_BBG<u32>(addr, val); return;
        case 0x00400000: GPU::WriteVRAM_AOBJ<u32>(addr, val); return;
        case 0x00600000: GPU::WriteVRAM_BOBJ<u32>(addr, val); return;
        default:         GPU::WriteVRAM_LCDC<u32>(addr, val); return;
    }
}

/*
    R11 - data to write (store only)
    RSCRATCH2 - address
    RSCRATCH3 - code cycles
*/
void* Compiler::Gen_MemoryRoutine9(bool store, int size, u32 region)
{
    AlignCode4();
    void* res = (void*)GetWritableCodePtr();

    if (!store)
    {
        MOV(32, R(RSCRATCH), R(RSCRATCH2));
        AND(32, R(RSCRATCH), Imm8(0x3));
        SHL(32, R(RSCRATCH), Imm8(3));
        // enter the shadow realm!
        MOV(32, MDisp(RSP, 8), R(RSCRATCH));
    }

    // cycle counting!
    // this is AddCycles_CDI
    MOV(32, R(R10), R(RSCRATCH2));
    SHR(32, R(R10), Imm8(12));
    MOVZX(32, 8, R10, MComplex(RCPU, R10, SCALE_1, offsetof(ARMv5, MemTimings) + 2));
    LEA(32, RSCRATCH, MComplex(RSCRATCH3, R10, SCALE_1, -6));
    CMP(32, R(R10), R(RSCRATCH3));
    CMOVcc(32, RSCRATCH3, R(R10), CC_G);
    CMP(32, R(RSCRATCH), R(RSCRATCH3));
    CMOVcc(32, RSCRATCH3, R(RSCRATCH), CC_G);
    ADD(32, R(RCycles), R(RSCRATCH3));

    if (!store)
        XOR(32, R(RSCRATCH), R(RSCRATCH));
    AND(32, R(RSCRATCH2), Imm32(~3));

    {
        MOV(32, R(RSCRATCH3), R(RSCRATCH2));
        SUB(32, R(RSCRATCH2), MDisp(RCPU, offsetof(ARMv5, DTCMBase)));
        CMP(32, R(RSCRATCH2), MDisp(RCPU, offsetof(ARMv5, DTCMSize)));
        FixupBranch outsideDTCM = J_CC(CC_AE);
        AND(32, R(RSCRATCH2), Imm32(0x3FFF));
        if (!store)
        {
            MOV(32, R(RSCRATCH), MComplex(RCPU, RSCRATCH2, SCALE_1, offsetof(ARMv5, DTCM)));
            MOV(32, R(ECX), MDisp(RSP, 8));
            ROR_(32, R(RSCRATCH), R(ECX));
        }
        else
            MOV(32, MComplex(RCPU, RSCRATCH2, SCALE_1, offsetof(ARMv5, DTCM)), R(R11));
        RET();
        SetJumpTarget(outsideDTCM);
        MOV(32, R(RSCRATCH2), R(RSCRATCH3));
    }

    switch (region)
    {
    case 0x00000000:
    case 0x01000000:
        {
            CMP(32, R(RSCRATCH2), MDisp(RCPU, offsetof(ARMv5, ITCMSize)));
            FixupBranch insideITCM = J_CC(CC_B);
            RET();
            SetJumpTarget(insideITCM);
            AND(32, R(RSCRATCH2), Imm32(0x7FFF));
            if (!store)
                MOV(32, R(RSCRATCH), MComplex(RCPU, RSCRATCH2, SCALE_1, offsetof(ARMv5, ITCM)));
            else
            {
                MOV(32, MComplex(RCPU, RSCRATCH2, SCALE_1, offsetof(ARMv5, ITCM)), R(R11));
                MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.ARM9_ITCM)), Imm32(0));
                MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.ARM9_ITCM) + 8), Imm32(0));
            }
        }
        break;
    case 0x02000000:
        AND(32, R(RSCRATCH2), Imm32(MAIN_RAM_SIZE - 1));
        if (!store)
            MOV(32, R(RSCRATCH), MDisp(RSCRATCH2, squeezePointer(NDS::MainRAM)));
        else
        {
            MOV(32, MDisp(RSCRATCH2, squeezePointer(NDS::MainRAM)), R(R11));
            MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.MainRAM)), Imm32(0));
            MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.MainRAM) + 8), Imm32(0));
        }
        break;
    case 0x03000000:
        {
            MOV(64, R(RSCRATCH3), M(&NDS::SWRAM_ARM9));
            TEST(64, R(RSCRATCH3), R(RSCRATCH3));
            FixupBranch notMapped = J_CC(CC_Z);
            AND(32, R(RSCRATCH2), M(&NDS::SWRAM_ARM9Mask));
            if (!store)
                MOV(32, R(RSCRATCH), MRegSum(RSCRATCH2, RSCRATCH3));
            else
            {
                MOV(32, MRegSum(RSCRATCH2, RSCRATCH3), R(R11));
                MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.SWRAM)), Imm32(0));
                MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.SWRAM) + 8), Imm32(0));
            }
            SetJumpTarget(notMapped);
        }
        break;
    case 0x04000000:
        MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
        if (!store)
        {
            ABI_PushRegistersAndAdjustStack({}, 8, 0);
            ABI_CallFunction(NDS::ARM9IORead32);
            ABI_PopRegistersAndAdjustStack({}, 8, 0);
        }
        else
        {
            MOV(32, R(ABI_PARAM2), R(R11));
            JMP((u8*)NDS::ARM9IOWrite32, true);
        }
        break;
    case 0x05000000:
        {        
            MOV(32, R(RSCRATCH), Imm32(1<<1));
            MOV(32, R(RSCRATCH3), Imm32(1<<9));
            TEST(32, R(RSCRATCH2), Imm32(0x400));
            CMOVcc(32, RSCRATCH, R(RSCRATCH3), CC_NZ);
            TEST(16, R(RSCRATCH), M(&NDS::PowerControl9));
            FixupBranch available = J_CC(CC_NZ);
            RET();
            SetJumpTarget(available);
            AND(32, R(RSCRATCH2), Imm32(0x7FF));
            if (!store)
                MOV(32, R(RSCRATCH), MDisp(RSCRATCH2, squeezePointer(GPU::Palette)));
            else
                MOV(32, MDisp(RSCRATCH2, squeezePointer(GPU::Palette)), R(R11));
        }
        break;
    case 0x06000000:
        MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
        if (!store)
        { 
            ABI_PushRegistersAndAdjustStack({}, 8);
            ABI_CallFunction(ReadVRAM9);
            ABI_PopRegistersAndAdjustStack({}, 8);
        }
        else
        {
            MOV(32, R(ABI_PARAM2), R(R11));
            JMP((u8*)WriteVRAM9, true);
        }
        break;
    case 0x07000000:
        {
            MOV(32, R(RSCRATCH), Imm32(1<<1));
            MOV(32, R(RSCRATCH3), Imm32(1<<9));
            TEST(32, R(RSCRATCH2), Imm32(0x400));
            CMOVcc(32, RSCRATCH, R(RSCRATCH3), CC_NZ);
            TEST(16, R(RSCRATCH), M(&NDS::PowerControl9));
            FixupBranch available = J_CC(CC_NZ);
            RET();
            SetJumpTarget(available);
            AND(32, R(RSCRATCH2), Imm32(0x7FF));
            if (!store)
                MOV(32, R(RSCRATCH), MDisp(RSCRATCH2, squeezePointer(GPU::OAM)));
            else
                MOV(32, MDisp(RSCRATCH2, squeezePointer(GPU::OAM)), R(R11));
        }
        break;
    case 0x08000000:
    case 0x09000000:
    case 0x0A000000:
        if (!store)
            MOV(32, R(RSCRATCH), Imm32(0xFFFFFFFF));
        break;
    case 0xFF000000:
        if (!store)
        {
            AND(32, R(RSCRATCH2), Imm32(0xFFF));
            MOV(32, R(RSCRATCH), MDisp(RSCRATCH2, squeezePointer(NDS::ARM9BIOS)));
        }
        break;
    default:
        MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
        if (!store)
        {
            ABI_PushRegistersAndAdjustStack({}, 8, 0);
            ABI_CallFunction(NDS::ARM9Read32);
            ABI_PopRegistersAndAdjustStack({}, 8, 0);
        }
        else
        {
            MOV(32, R(ABI_PARAM2), R(R11));
            JMP((u8*)NDS::ARM9Write32, true);
        }
        break;
    }

    if (!store)
    {
        MOV(32, R(ECX), MDisp(RSP, 8));
        ROR_(32, R(RSCRATCH), R(ECX));
    }

    RET();

    return res;
}

void* Compiler::Gen_MemoryRoutine7(bool store, int size, bool mainRAMCode, u32 region)
{
    AlignCode4();
    void* res = GetWritableCodePtr();

    if (!store)
    {
        MOV(32, R(RSCRATCH), R(RSCRATCH2));
        AND(32, R(RSCRATCH), Imm8(0x3));
        SHL(32, R(RSCRATCH), Imm8(3));
        // enter the shadow realm!
        MOV(32, MDisp(RSP, 8), R(RSCRATCH));
    }

    // AddCycles_CDI
    MOV(32, R(RSCRATCH), R(RSCRATCH2));
    SHR(32, R(RSCRATCH), Imm8(15));
    MOVZX(32, 8, RSCRATCH, MDisp(RSCRATCH, squeezePointer(NDS::ARM7MemTimings + 2)));
    if ((region == 0x02000000 && mainRAMCode) || (region != 0x02000000 && !mainRAMCode))
    {
        if (!store && region != 0x02000000)
            LEA(32, RSCRATCH3, MComplex(RSCRATCH, RSCRATCH3, SCALE_1, 1));
        ADD(32, R(RCycles), R(RSCRATCH3));
    }
    else
    {
        if (!store)
            ADD(32, R(region == 0x02000000 ? RSCRATCH2 : RSCRATCH), Imm8(1));
        LEA(32, R10, MComplex(RSCRATCH, RSCRATCH3, SCALE_1, -3));
        CMP(32, R(RSCRATCH3), R(RSCRATCH));
        CMOVcc(32, RSCRATCH, R(RSCRATCH3), CC_G);
        CMP(32, R(R10), R(RSCRATCH));
        CMOVcc(32, RSCRATCH, R(R10), CC_G);
        ADD(32, R(RCycles), R(RSCRATCH));
    }

    if (!store)
        XOR(32, R(RSCRATCH), R(RSCRATCH));
    AND(32, R(RSCRATCH2), Imm32(~3));

    switch (region)
    {
        case 0x00000000:
            if (!store) {
                CMP(32, R(RSCRATCH2), Imm32(0x4000));
                FixupBranch outsideBIOS1 = J_CC(CC_AE);

                MOV(32, R(RSCRATCH), MDisp(RCPU, offsetof(ARM, R[15])));
                CMP(32, R(RSCRATCH), Imm32(0x4000));
                FixupBranch outsideBIOS2 = J_CC(CC_AE);
                MOV(32, R(RSCRATCH3), M(&NDS::ARM7BIOSProt));
                CMP(32, R(RSCRATCH2), R(RSCRATCH3));
                FixupBranch notDenied1 = J_CC(CC_AE);
                CMP(32, R(RSCRATCH), R(RSCRATCH3));
                FixupBranch notDenied2 = J_CC(CC_B);
                SetJumpTarget(outsideBIOS2);
                MOV(32, R(RSCRATCH), Imm32(0xFFFFFFFF));
                RET();

                SetJumpTarget(notDenied1);
                SetJumpTarget(notDenied2);
                MOV(32, R(RSCRATCH), MDisp(RSCRATCH2, squeezePointer(NDS::ARM7BIOS)));
                MOV(32, R(ECX), MDisp(RSP, 8));
                ROR_(32, R(RSCRATCH), R(ECX));
                RET();

                SetJumpTarget(outsideBIOS1);
            }
            break;
        case 0x02000000:
            AND(32, R(RSCRATCH2), Imm32(MAIN_RAM_SIZE - 1));
            if (!store)
                MOV(32, R(RSCRATCH), MDisp(RSCRATCH2, squeezePointer(NDS::MainRAM)));
            else
            {
                MOV(32, MDisp(RSCRATCH2, squeezePointer(NDS::MainRAM)), R(R11));
                MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.MainRAM)), Imm32(0));
                MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.MainRAM) + 8), Imm32(0));
            }
            break;
        case 0x03000000:
            {
                TEST(32, R(RSCRATCH2), Imm32(0x800000));
                FixupBranch region = J_CC(CC_NZ);
                MOV(64, R(RSCRATCH), M(&NDS::SWRAM_ARM7));
                TEST(64, R(RSCRATCH), R(RSCRATCH));
                FixupBranch notMapped = J_CC(CC_Z);
                AND(32, R(RSCRATCH2), M(&NDS::SWRAM_ARM7Mask));
                if (!store)
                {
                    MOV(32, R(RSCRATCH), MRegSum(RSCRATCH, RSCRATCH2));
                    MOV(32, R(ECX), MDisp(RSP, 8));
                    ROR_(32, R(RSCRATCH), R(ECX));
                }
                else
                {
                    MOV(32, MRegSum(RSCRATCH, RSCRATCH2), R(R11));
                    MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.SWRAM)), Imm32(0));
                    MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.SWRAM) + 8), Imm32(0));
                }
                RET();
                SetJumpTarget(region);
                SetJumpTarget(notMapped);
                AND(32, R(RSCRATCH2), Imm32(0xFFFF));
                if (!store)
                    MOV(32, R(RSCRATCH), MDisp(RSCRATCH2, squeezePointer(NDS::ARM7WRAM)));
                else
                {
                    MOV(32, MDisp(RSCRATCH2, squeezePointer(NDS::ARM7WRAM)), R(R11));
                    MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.ARM7_WRAM)), Imm32(0));
                    MOV(64, MScaled(RSCRATCH2, SCALE_4, squeezePointer(cache.ARM7_WRAM) + 8), Imm32(0));
                }
            }
            break;
        case 0x04000000:
            {
                TEST(32, R(RSCRATCH2), Imm32(0x800000));
                FixupBranch region = J_CC(CC_NZ);
                MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
                if (!store)
                {
                    ABI_PushRegistersAndAdjustStack({}, 8);
                    ABI_CallFunction(NDS::ARM7IORead32);
                    ABI_PopRegistersAndAdjustStack({}, 8);

                    MOV(32, R(ECX), MDisp(RSP, 8));
                    ROR_(32, R(RSCRATCH), R(ECX));
                    RET();
                }
                else
                {
                    MOV(32, R(ABI_PARAM2), R(R11));
                    JMP((u8*)NDS::ARM7IOWrite32, true);
                }
                SetJumpTarget(region);

                if (!store)
                {
                    ABI_PushRegistersAndAdjustStack({RSCRATCH2}, 8);
                    MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
                    ABI_CallFunction(Wifi::Read);
                    ABI_PopRegistersAndAdjustStack({RSCRATCH2}, 8);

                    ADD(32, R(RSCRATCH2), Imm8(2));
                    ABI_PushRegistersAndAdjustStack({EAX}, 8);
                    MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
                    ABI_CallFunction(Wifi::Read);
                    MOV(32, R(RSCRATCH2), R(EAX));
                    SHL(32, R(RSCRATCH2), Imm8(16));
                    ABI_PopRegistersAndAdjustStack({EAX}, 8);
                    OR(32, R(EAX), R(RSCRATCH2));
                }
                else
                {
                    ABI_PushRegistersAndAdjustStack({RSCRATCH2, R11}, 8);
                    MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
                    MOVZX(32, 16, ABI_PARAM2, R(R11));
                    ABI_CallFunction(Wifi::Write);
                    ABI_PopRegistersAndAdjustStack({RSCRATCH2, R11}, 8);
                    SHR(32, R(R11), Imm8(16));
                    ADD(32, R(RSCRATCH2), Imm8(2));
                    ABI_PushRegistersAndAdjustStack({RSCRATCH2, R11}, 8);
                    MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
                    MOVZX(32, 16, ABI_PARAM2, R(R11));
                    ABI_CallFunction(Wifi::Write);
                    ABI_PopRegistersAndAdjustStack({RSCRATCH2, R11}, 8);
                }
            }
            break;
        case 0x06000000:
            MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
            if (!store)
            {
                ABI_PushRegistersAndAdjustStack({}, 8);
                ABI_CallFunction(GPU::ReadVRAM_ARM7<u32>);
                ABI_PopRegistersAndAdjustStack({}, 8);
            }
            else
            {
                AND(32, R(ABI_PARAM1), Imm32(0x40000 - 1));
                MOV(64, MScaled(ABI_PARAM1, SCALE_4, squeezePointer(cache.ARM7_WVRAM)), Imm32(0));
                MOV(64, MScaled(ABI_PARAM1, SCALE_4, squeezePointer(cache.ARM7_WVRAM) + 8), Imm32(0));
                MOV(32, R(ABI_PARAM2), R(R11));
                JMP((u8*)GPU::WriteVRAM_ARM7<u32>, true);
            }
            break;
        case 0x08000000:
        case 0x09000000:
        case 0x0A000000:
            if (!store)
                MOV(32, R(RSCRATCH), Imm32(0xFFFFFFFF));
            break;
        /*default:
            ABI_PushRegistersAndAdjustStack({}, 8, 0);
            MOV(32, R(ABI_PARAM1), R(RSCRATCH2));
            ABI_CallFunction(NDS::ARM7Read32);
            ABI_PopRegistersAndAdjustStack({}, 8, 0);
            break;*/
    }

    if (!store)
    {
        MOV(32, R(ECX), MDisp(RSP, 8));
        ROR_(32, R(RSCRATCH), R(ECX));
    }

    RET();

    return res;
}

OpArg Compiler::A_Comp_GetMemWBOffset()
{
    if (!(CurrentInstr.Instr & (1 << 25)))
        return Imm32(CurrentInstr.Instr & 0xFFF);
    else
    {
        int op = (CurrentInstr.Instr >> 5) & 0x3;
        int amount = (CurrentInstr.Instr >> 7) & 0x1F;
        OpArg rm = MapReg(CurrentInstr.A_Reg(0));
        bool carryUsed;
        return Comp_RegShiftImm(op, amount, rm, false, carryUsed);
    }
}

void Compiler::A_Comp_MemWB()
{    
    OpArg rn = MapReg(CurrentInstr.A_Reg(16));
    OpArg rd = MapReg(CurrentInstr.A_Reg(12));
    bool load = CurrentInstr.Instr & (1 << 20);

    MOV(32, R(RSCRATCH2), rn);
    if (CurrentInstr.Instr & (1 << 24))
    {
        OpArg offset = A_Comp_GetMemWBOffset();
        if (CurrentInstr.Instr & (1 << 23))
            ADD(32, R(RSCRATCH2), offset);
        else
            SUB(32, R(RSCRATCH2), offset);

        if (CurrentInstr.Instr & (1 << 21))
            MOV(32, rn, R(RSCRATCH2));
    }

    u32 cycles = Num ? NDS::ARM7MemTimings[CurrentInstr.CodeCycles][2] : CurrentInstr.CodeCycles;
    MOV(32, R(RSCRATCH3), Imm32(cycles));
    MOV(32, R(RSCRATCH), R(RSCRATCH2));
    SHR(32, R(RSCRATCH), Imm8(24));
    AND(32, R(RSCRATCH), Imm8(0xF));
    void** funcArray;
    if (load)
        funcArray = Num ? ReadMemFuncs7[CodeRegion == 0x02] : ReadMemFuncs9;
    else
    {
        funcArray = Num ? WriteMemFuncs7[CodeRegion == 0x02] : WriteMemFuncs9; 
        MOV(32, R(R11), rd);
    }
    CALLptr(MScaled(RSCRATCH, SCALE_8, squeezePointer(funcArray)));

    if (load)
        MOV(32, R(RSCRATCH2), R(RSCRATCH));

    if (!(CurrentInstr.Instr & (1 << 24)))
    {
        OpArg offset = A_Comp_GetMemWBOffset();

        if (CurrentInstr.Instr & (1 << 23))
            ADD(32, rn, offset);
        else
            SUB(32, rn, offset);
    }

    if (load)
        MOV(32, rd, R(RSCRATCH2));
}

void Compiler::T_Comp_MemReg()
{
    OpArg rd = MapReg(CurrentInstr.T_Reg(0));
    OpArg rb = MapReg(CurrentInstr.T_Reg(3));
    OpArg ro = MapReg(CurrentInstr.T_Reg(6));

    int op = (CurrentInstr.Instr >> 10) & 0x3;
    bool load = op & 0x2;
    
    MOV(32, R(RSCRATCH2), rb);
    ADD(32, R(RSCRATCH2), ro);

    u32 cycles = Num ? NDS::ARM7MemTimings[CurrentInstr.CodeCycles][0] : (R15 & 0x2 ? 0 : CurrentInstr.CodeCycles);
    MOV(32, R(RSCRATCH3), Imm32(cycles));
    MOV(32, R(RSCRATCH), R(RSCRATCH2));
    SHR(32, R(RSCRATCH), Imm8(24));
    AND(32, R(RSCRATCH), Imm8(0xF));
    void** funcArray;
    if (load)
        funcArray = Num ? ReadMemFuncs7[CodeRegion == 0x02] : ReadMemFuncs9;
    else
    {
        funcArray = Num ? WriteMemFuncs7[CodeRegion == 0x02] : WriteMemFuncs9; 
        MOV(32, R(R11), rd);
    }
    CALLptr(MScaled(RSCRATCH, SCALE_8, squeezePointer(funcArray)));

    if (load)
        MOV(32, rd, R(RSCRATCH));
}

void Compiler::T_Comp_MemImm()
{
    // TODO: aufräumen!!!
    OpArg rd = MapReg(CurrentInstr.T_Reg(0));
    OpArg rb = MapReg(CurrentInstr.T_Reg(3));
    
    int op = (CurrentInstr.Instr >> 11) & 0x3;
    u32 offset = ((CurrentInstr.Instr >> 6) & 0x1F) * 4;
    bool load = op & 0x1;

    LEA(32, RSCRATCH2, MDisp(rb.GetSimpleReg(), offset));
    u32 cycles = Num ? NDS::ARM7MemTimings[CurrentInstr.CodeCycles][0] : (R15 & 0x2 ? 0 : CurrentInstr.CodeCycles);
    MOV(32, R(RSCRATCH3), Imm32(cycles));
    MOV(32, R(RSCRATCH), R(RSCRATCH2));
    SHR(32, R(RSCRATCH), Imm8(24));
    AND(32, R(RSCRATCH), Imm8(0xF));
    void** funcArray;
    if (load)
        funcArray = Num ? ReadMemFuncs7[CodeRegion == 0x02] : ReadMemFuncs9;
    else
    {
        funcArray = Num ? WriteMemFuncs7[CodeRegion == 0x02] : WriteMemFuncs9; 
        MOV(32, R(R11), rd);
    }
    CALLptr(MScaled(RSCRATCH, SCALE_8, squeezePointer(funcArray)));

    if (load)
        MOV(32, rd, R(RSCRATCH));
}

}