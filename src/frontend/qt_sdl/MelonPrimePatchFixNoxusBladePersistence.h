#ifndef MELON_PRIME_PATCH_FIX_NOXUS_BLADE_PERSISTENCE_H
#define MELON_PRIME_PATCH_FIX_NOXUS_BLADE_PERSISTENCE_H

#ifdef MELONPRIME_DS

#include <cstdint>

namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

// Cave-free / ROM-write-free Noxus Vhoscythe blade persistence fix.
//
// Fires at the death cleanup sequence immediately after the original
// CPlayer+0x860 clear.  Preserves the original instruction stream and only
// adds one side effect:
//
//   CPlayer+0x704 = 0
//
// CPlayer+0x704 is the alt attack timer used by the timer-based Noxus blade
// hit branch.  If it survives death, the Vhoscythe blade hit state can persist
// after respawn.
//
// This patch is integrated into the shared ARM9 hook dispatcher owned by
// MelonPrimePatchShadowFreezeRuntimeHook.  Do not call SetARM9InstructionHook
// directly from this file.
//
// ROM group order: JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6.

// Returns the number of hook addresses written to out[] (≤ maxCount).
// Called by the shared dispatcher Install to build the combined address list.
uint32_t FixNoxusBladePersistence_GetAddresses(
    uint8_t romGroupIndex,
    uint32_t* out,
    uint32_t maxCount);

// Called by the shared dispatcher Install/Uninstall to manage per-file state.
void FixNoxusBladePersistence_SetState(Config::Table* cfg, uint8_t romGroupIndex);
void FixNoxusBladePersistence_ClearState();

// Fast dispatch path called from the shared ARM9 HookCallback.
// Uses cached config state.  Side-effect only — never redirects execution.
void FixNoxusBladePersistence_DispatchCheck(
    melonDS::NDS* nds,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16]);

void FixNoxusBladePersistence_ResetPatchState();
void FixNoxusBladePersistence_NotifyConfigChanged();

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_PATCH_FIX_NOXUS_BLADE_PERSISTENCE_H
