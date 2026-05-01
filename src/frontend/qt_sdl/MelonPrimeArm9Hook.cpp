#ifdef MELONPRIME_DS

#include "MelonPrimeArm9Hook.h"
#include "MelonPrimePatchShadowFreezeRuntimeHook.h"
#include "MelonPrimePatchFixNoxusBladePersistence.h"
#include "NDS.h"

#include <cstdint>

namespace MelonPrime {

namespace {

static bool DispatcherCallback(
    melonDS::NDS* nds,
    void* /*userdata*/,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    redirectExecAddr = 0;

    // Side-effect hook: runs regardless of whether a redirect follows.
    FixNoxusBladePersistence_DispatchCheck(nds, arm9ExecAddr, regs);

    // Redirect hook: may change execution address.
    return ShadowFreezeRuntimeHook_DispatchCheckAndRedirect(
        nds, arm9ExecAddr, regs, redirectExecAddr);
}

} // anonymous namespace

void ARM9Hook_Install(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex)
{
    if (!nds)
    {
        ShadowFreezeRuntimeHook_ClearState();
        FixNoxusBladePersistence_ClearState();
        return;
    }

    ShadowFreezeRuntimeHook_SetState(&cfg, romGroupIndex);
    FixNoxusBladePersistence_SetState(&cfg, romGroupIndex);

    uint32_t addresses[melonDS::NDS::ARM9InstructionHookMaxAddresses] = {};
    uint32_t count = 0;

    count += ShadowFreezeRuntimeHook_GetAddresses(
        romGroupIndex,
        addresses + count,
        melonDS::NDS::ARM9InstructionHookMaxAddresses - count);

    count += FixNoxusBladePersistence_GetAddresses(
        romGroupIndex,
        addresses + count,
        melonDS::NDS::ARM9InstructionHookMaxAddresses - count);

    if (count > 0)
    {
        nds->SetARM9InstructionHook(DispatcherCallback, nullptr, addresses, count);

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
