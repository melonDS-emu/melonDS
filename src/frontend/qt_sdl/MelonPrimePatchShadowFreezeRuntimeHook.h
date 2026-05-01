#ifndef MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H
#define MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H

#ifdef MELONPRIME_DS

#include <cstdint>

namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

// Cave-free / ROM-write-free Shadow Freeze fix.
//
// Hook registration is managed by MelonPrimeArm9Hook (the shared ARM9 dispatcher).
// The functions below are the per-module API used by that dispatcher.
//
// ROM group order: JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6.

// Dispatcher API — called by MelonPrimeArm9Hook Install/Uninstall.
uint32_t ShadowFreezeRuntimeHook_GetAddresses(
    uint8_t romGroupIndex,
    uint32_t* out,
    uint32_t maxCount);

void ShadowFreezeRuntimeHook_SetState(Config::Table* cfg, uint8_t romGroupIndex);
void ShadowFreezeRuntimeHook_ClearState();

// Fast dispatch path called from the shared ARM9 HookCallback.
// Uses cached config state.  Returns true and sets redirectExecAddr when redirecting.
bool ShadowFreezeRuntimeHook_DispatchCheckAndRedirect(
    melonDS::NDS* nds,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr);

// Direct check for callers that hold an explicit Config::Table reference.
// Pass the real instruction address currently about to execute, not pipelined R15.
bool ShadowFreezeRuntimeHook_CheckAndRedirect(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr);

bool ShadowFreezeRuntimeHook_CheckAndRedirectFromPipelinedR15(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr);

void ShadowFreezeRuntimeHook_ResetPatchState();
void ShadowFreezeRuntimeHook_NotifyConfigChanged();

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H
