#ifndef MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H
#define MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H

#ifdef MELONPRIME_DS

#include <cstdint>

namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

// Cave-free / ARM9-code-injection-free Shadow Freeze fix.
//
// Intended use:
//   Call from an ARM9 pre-instruction hook before executing the current ARM
//   instruction.  Pass the real instruction address about to execute.
//
// Behavior:
//   - Leaves the game's original lateral range check untouched.
//   - Replaces only the final Ice Wave angle decision with a C++ full-3D check.
//   - Does not write to ARM9 memory.
//   - Does not use E200/cave code.
//
// Result:
//   range = lateral distance
//   angle = full 3D angle
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

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_PATCH_SHADOW_FREEZE_RUNTIME_HOOK_H
