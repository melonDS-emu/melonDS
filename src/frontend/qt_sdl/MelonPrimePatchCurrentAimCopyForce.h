#ifndef MELON_PRIME_PATCH_CURRENT_AIM_COPY_FORCE_H
#define MELON_PRIME_PATCH_CURRENT_AIM_COPY_FORCE_H

#ifdef MELONPRIME_DS

#include <cstdint>

namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

void CurrentAimCopyForce_ApplyOnce(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex);

void CurrentAimCopyForce_RestoreOnce(
    melonDS::NDS* nds,
    uint8_t romGroupIndex);

void CurrentAimCopyForce_ResetPatchState();

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_PATCH_CURRENT_AIM_COPY_FORCE_H
