#ifndef MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H
#define MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H

#ifdef MELONPRIME_DS

#include <cstdint>

namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

// Cave-free / ROM-write-free Shadow Freeze fix.
//
// Install registers the ARM9 pre-instruction hook for the given ROM version.
// Uninstall clears it.  Both are safe to call multiple times.
void ShadowFreezeRuntimeHook_Install(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex);

void ShadowFreezeRuntimeHook_Uninstall(melonDS::NDS* nds);

// Direct check, intended to be called from an ARM9 pre-instruction hook.
// Pass the real instruction address currently about to execute, not the pipelined
// ARM R15 value.  When it returns true, skip normal execution of that instruction
// and jump to redirectExecAddr instead.
//
// ROM group order:
//   JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6.
bool ShadowFreezeRuntimeHook_CheckAndRedirect(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr);

// Optional helper for implementations that only have the usual ARM pipelined R15.
// In ARM state, the executing instruction address is normally R15 - 8.
bool ShadowFreezeRuntimeHook_CheckAndRedirectFromPipelinedR15(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr);

void ShadowFreezeRuntimeHook_ResetPatchState();

// Call after saving settings so the callback refreshes its cached enabled state.
void ShadowFreezeRuntimeHook_NotifyConfigChanged();

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H
