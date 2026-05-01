#ifndef MELON_PRIME_ARM9_HOOK_H
#define MELON_PRIME_ARM9_HOOK_H

#ifdef MELONPRIME_DS

#include <cstdint>

namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

class MelonPrimeCore;

// Combined ARM9 instruction hook dispatcher.
//
// Owns the single SetARM9InstructionHook slot and dispatches to all registered
// MelonPrime runtime hooks in priority order:
//
//   1. NativeAimDeltaHook        — register side effect only, never redirects
//   2. FixNoxusBladePersistence  — RAM side effect only, never redirects
//   3. ShadowFreezeRuntimeHook   — may redirect execution
//
// Call Install once after ROM detection.  Call Uninstall on emu stop/reset.

void ARM9Hook_Install(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    MelonPrimeCore* core);

void ARM9Hook_Uninstall(melonDS::NDS* nds);

// Calls ResetPatchState for every registered hook.
void ARM9Hook_ResetPatchState();

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_ARM9_HOOK_H
