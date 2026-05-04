#ifdef MELONPRIME_DS

#include "MelonPrimeArm9Hook.h"
#include "MelonPrime.h"
#include "MelonPrimeCompilerHints.h"
#include "MelonPrimePatchShadowFreezeRuntimeHook.h"
#include "MelonPrimePatchFixNoxusBladePersistence.h"
#include "NDS.h"

#include <cstdint>

namespace MelonPrime {

namespace {

enum DispatchMask : uint8_t
{
    Dispatch_NativeAimDelta             = 1u << 0,
    Dispatch_ShadowFreeze               = 1u << 1,
    Dispatch_NoxusBlade                 = 1u << 2,
    Dispatch_ImmediateInputEdgeOverlay  = 1u << 3,
};

struct DispatchEntry
{
    uint32_t Address;
    uint8_t Mask;
};

static DispatchEntry s_dispatchEntries[melonDS::NDS::ARM9InstructionHookMaxAddresses] = {};
static uint32_t s_dispatchCount = 0;

static void ClearDispatchEntries() noexcept
{
    s_dispatchCount = 0;
    for (auto& entry : s_dispatchEntries)
        entry = {};
}

static void AddDispatchAddress(uint32_t address, uint8_t mask) noexcept
{
    for (uint32_t i = 0; i < s_dispatchCount; ++i)
    {
        if (s_dispatchEntries[i].Address == address)
        {
            s_dispatchEntries[i].Mask |= mask;
            return;
        }
    }

    if (s_dispatchCount >= melonDS::NDS::ARM9InstructionHookMaxAddresses)
        return;

    s_dispatchEntries[s_dispatchCount++] = {address, mask};
}

[[nodiscard]] static FORCE_INLINE uint8_t FindDispatchMask(uint32_t arm9ExecAddr) noexcept
{
    switch (s_dispatchCount)
    {
    case 8:
        if (s_dispatchEntries[7].Address == arm9ExecAddr) return s_dispatchEntries[7].Mask;
        [[fallthrough]];
    case 7:
        if (s_dispatchEntries[6].Address == arm9ExecAddr) return s_dispatchEntries[6].Mask;
        [[fallthrough]];
    case 6:
        if (s_dispatchEntries[5].Address == arm9ExecAddr) return s_dispatchEntries[5].Mask;
        [[fallthrough]];
    case 5:
        if (s_dispatchEntries[4].Address == arm9ExecAddr) return s_dispatchEntries[4].Mask;
        [[fallthrough]];
    case 4:
        if (s_dispatchEntries[0].Address == arm9ExecAddr) return s_dispatchEntries[0].Mask;
        if (s_dispatchEntries[1].Address == arm9ExecAddr) return s_dispatchEntries[1].Mask;
        if (s_dispatchEntries[2].Address == arm9ExecAddr) return s_dispatchEntries[2].Mask;
        if (s_dispatchEntries[3].Address == arm9ExecAddr) return s_dispatchEntries[3].Mask;
        return 0;
    case 3:
        if (s_dispatchEntries[0].Address == arm9ExecAddr) return s_dispatchEntries[0].Mask;
        if (s_dispatchEntries[1].Address == arm9ExecAddr) return s_dispatchEntries[1].Mask;
        if (s_dispatchEntries[2].Address == arm9ExecAddr) return s_dispatchEntries[2].Mask;
        return 0;
    case 2:
        if (s_dispatchEntries[0].Address == arm9ExecAddr) return s_dispatchEntries[0].Mask;
        if (s_dispatchEntries[1].Address == arm9ExecAddr) return s_dispatchEntries[1].Mask;
        return 0;
    case 1:
        return (s_dispatchEntries[0].Address == arm9ExecAddr) ? s_dispatchEntries[0].Mask : 0;
    default:
        return 0;
    }
}

static bool DispatcherCallback(
    melonDS::NDS* nds,
    void* userdata,
    uint32_t arm9ExecAddr,
    uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    redirectExecAddr = 0;

    const uint8_t mask = FindDispatchMask(arm9ExecAddr);
    if (UNLIKELY(mask == 0))
        return false;

    if ((mask & Dispatch_NativeAimDelta) != 0)
    {
        if (auto* core = static_cast<MelonPrimeCore*>(userdata))
            core->NativeAimDeltaHook_DispatchCheck(nds, arm9ExecAddr, regs);
    }

    if ((mask & Dispatch_ImmediateInputEdgeOverlay) != 0)
    {
        if (auto* core = static_cast<MelonPrimeCore*>(userdata))
            core->ImmediateInputEdgeOverlay_DispatchCheck(nds, arm9ExecAddr, regs);
    }

    // Side-effect hook: runs regardless of whether a redirect follows.
    if ((mask & Dispatch_NoxusBlade) != 0)
        FixNoxusBladePersistence_DispatchCheck(nds, arm9ExecAddr, regs);

    // Redirect hook: may change execution address.
    if ((mask & Dispatch_ShadowFreeze) != 0)
    {
        return ShadowFreezeRuntimeHook_DispatchCheckAndRedirect(
            nds, arm9ExecAddr, regs, redirectExecAddr);
    }

    return false;
}

} // anonymous namespace

void ARM9Hook_Install(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    MelonPrimeCore* core)
{
    ClearDispatchEntries();

    if (!nds)
    {
        ShadowFreezeRuntimeHook_ClearState();
        FixNoxusBladePersistence_ClearState();
        return;
    }

    ShadowFreezeRuntimeHook_SetState(&cfg, romGroupIndex);
    FixNoxusBladePersistence_SetState(&cfg, romGroupIndex);

    uint32_t moduleAddresses[melonDS::NDS::ARM9InstructionHookMaxAddresses] = {};
    uint32_t moduleCount = 0;

    moduleCount = MelonPrimeCore::NativeAimDeltaHook_GetAddresses(
        romGroupIndex,
        moduleAddresses,
        melonDS::NDS::ARM9InstructionHookMaxAddresses);
    for (uint32_t i = 0; i < moduleCount; ++i)
        AddDispatchAddress(moduleAddresses[i], Dispatch_NativeAimDelta);

    moduleCount = ShadowFreezeRuntimeHook_GetAddresses(
        romGroupIndex,
        moduleAddresses,
        melonDS::NDS::ARM9InstructionHookMaxAddresses);
    for (uint32_t i = 0; i < moduleCount; ++i)
        AddDispatchAddress(moduleAddresses[i], Dispatch_ShadowFreeze);

    moduleCount = FixNoxusBladePersistence_GetAddresses(
        romGroupIndex,
        moduleAddresses,
        melonDS::NDS::ARM9InstructionHookMaxAddresses);
    for (uint32_t i = 0; i < moduleCount; ++i)
        AddDispatchAddress(moduleAddresses[i], Dispatch_NoxusBlade);

    moduleCount = MelonPrimeCore::ImmediateInputEdgeOverlay_GetAddresses(
        romGroupIndex,
        moduleAddresses,
        melonDS::NDS::ARM9InstructionHookMaxAddresses);
    for (uint32_t i = 0; i < moduleCount; ++i)
        AddDispatchAddress(moduleAddresses[i], Dispatch_ImmediateInputEdgeOverlay);

    uint32_t addresses[melonDS::NDS::ARM9InstructionHookMaxAddresses] = {};
    const uint32_t count = s_dispatchCount;
    for (uint32_t i = 0; i < count; ++i)
        addresses[i] = s_dispatchEntries[i].Address;

    if (count > 0)
    {
        nds->SetARM9InstructionHook(DispatcherCallback, core, addresses, count);

        // Force JIT cache invalidation for every registered address.
        // The death cleanup code (NoxusBlade hook) may have been JIT-compiled
        // during game initialization before hook registration, so its compiled
        // block has no hook call.  Writing back the same instruction value
        // triggers melonDS to discard and recompile the affected block with the
        // hook in place.
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t addr = addresses[i] & ~3u;
            nds->ARM9Write32(addr, nds->ARM9Read32(addr));
        }
    }
    else
        nds->ClearARM9InstructionHook();
}

void ARM9Hook_Uninstall(melonDS::NDS* nds)
{
    ClearDispatchEntries();
    ShadowFreezeRuntimeHook_ClearState();
    FixNoxusBladePersistence_ClearState();
    if (nds)
        nds->ClearARM9InstructionHook();
}

void ARM9Hook_ResetPatchState()
{
    ShadowFreezeRuntimeHook_ResetPatchState();
    FixNoxusBladePersistence_ResetPatchState();
}

} // namespace MelonPrime

#endif // MELONPRIME_DS
