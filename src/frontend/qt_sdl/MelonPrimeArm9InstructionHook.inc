#if defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_NDS_PUBLIC_TYPES)

    using ARM9InstructionHookFn = bool (*)(NDS* nds, void* userdata, u32 arm9ExecAddr, const u32 regs[16], u32& redirectExecAddr);
    static constexpr u32 ARM9InstructionHookMaxAddresses = 4;

#elif defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_NDS_FIELDS)

    ARM9InstructionHookFn ARM9InstructionHook = nullptr;
    void* ARM9InstructionHookUserData = nullptr;
    u32 ARM9InstructionHookAddresses[ARM9InstructionHookMaxAddresses] {};
    u32 ARM9InstructionHookAddrCount = 0;

#elif defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_NDS_METHODS)

    void SetARM9InstructionHook(ARM9InstructionHookFn hook, void* userdata, const u32* addresses, u32 addressCount) noexcept;
    void ClearARM9InstructionHook() noexcept;

    [[nodiscard]] bool ARM9InstructionHookMatches(u32 arm9ExecAddr) const noexcept
    {
        if (!ARM9InstructionHook)
            return false;

        for (u32 i = 0; i < ARM9InstructionHookAddrCount; ++i)
            if (ARM9InstructionHookAddresses[i] == arm9ExecAddr)
                return true;

        return false;
    }

    [[nodiscard]] bool ARM9InstructionHookCheckAndRedirect(u32 arm9ExecAddr, const u32 regs[16], u32& redirectExecAddr) noexcept
    {
        return ARM9InstructionHook
            && ARM9InstructionHookMatches(arm9ExecAddr)
            && ARM9InstructionHook(this, ARM9InstructionHookUserData, arm9ExecAddr, regs, redirectExecAddr);
    }

#elif defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_NDS_CPP)

void NDS::SetARM9InstructionHook(ARM9InstructionHookFn hook, void* userdata, const u32* addresses, u32 addressCount) noexcept
{
    ARM9InstructionHook = hook;
    ARM9InstructionHookUserData = userdata;
    ARM9InstructionHookAddrCount = std::min(addressCount, ARM9InstructionHookMaxAddresses);

    for (u32 i = 0; i < ARM9InstructionHookAddrCount; ++i)
        ARM9InstructionHookAddresses[i] = addresses ? addresses[i] : 0;
    for (u32 i = ARM9InstructionHookAddrCount; i < ARM9InstructionHookMaxAddresses; ++i)
        ARM9InstructionHookAddresses[i] = 0;

#ifdef JIT_ENABLED
    JIT.ResetBlockCache();
#endif
}

void NDS::ClearARM9InstructionHook() noexcept
{
    SetARM9InstructionHook(nullptr, nullptr, nullptr, 0);
}

#elif defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_ARM_INTERPRETER_EXECUTE)

                const u32 instrAddr = R[15] - 8;
                u32 redirectExecAddr = 0;
                if (NDS.ARM9InstructionHookCheckAndRedirect(instrAddr, R, redirectExecAddr))
                {
                    JumpTo(redirectExecAddr);
                }
                else if (CheckCondition(CurInstr >> 28))

#elif defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_JIT_X64_DECLS)

    void Comp_ARM9InstructionHook();

#elif defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_JIT_X64_TRAMPOLINE)

static u32 ARM9InstructionHookTrampoline(NDS* nds, ARMv5* cpu, u32 arm9ExecAddr)
{
    u32 redirectExecAddr = 0;
    if (!nds->ARM9InstructionHookCheckAndRedirect(arm9ExecAddr, cpu->R, redirectExecAddr))
        return 0;

    cpu->JumpTo(redirectExecAddr);
    return 1;
}

#elif defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_JIT_X64_COMPILE_LOOP)

        if (!Thumb && Num == 0 && NDS.ARM9InstructionHookMatches(CurInstr.Addr))
            Comp_ARM9InstructionHook();

#elif defined(MELONPRIME_ARM9_INSTRUCTION_HOOK_JIT_X64_METHOD)

void Compiler::Comp_ARM9InstructionHook()
{
    if (ConstantCycles)
    {
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm32(ConstantCycles));
        ConstantCycles = 0;
    }

    RegCache.Flush();
    SaveCPSR(false);

    MOV(32, MDisp(RCPU, offsetof(ARM, R) + 15 * 4), Imm32(R15));
    MOV(32, MDisp(RCPU, offsetof(ARM, CurInstr)), Imm32(CurInstr.Instr));

    MOV(64, R(ABI_PARAM1), ImmPtr(&NDS));
    MOV(64, R(ABI_PARAM2), R(RCPU));
    MOV(32, R(ABI_PARAM3), Imm32(CurInstr.Addr));
    ABI_CallFunction(ARM9InstructionHookTrampoline);

    TEST(32, R(RSCRATCH), R(RSCRATCH));
    FixupBranch skipRedirect = J_CC(CC_Z);
    ABI_TailCall(ARM_Ret);
    SetJumpTarget(skipRedirect);
}

#else
#error MelonPrimeArm9InstructionHook.inc included without a known section macro.
#endif
