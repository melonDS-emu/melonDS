#ifndef MELON_PRIME_PATCH_INSTANT_AIM_FOLLOW_H
#define MELON_PRIME_PATCH_INSTANT_AIM_FOLLOW_H

#ifdef MELONPRIME_DS

#include <cstdint>

namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

void InstantAimFollow_ApplyOnce(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex);

void InstantAimFollow_RestoreOnce(
    melonDS::NDS* nds,
    uint8_t romGroupIndex);

void InstantAimFollow_ResetPatchState();

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_PATCH_INSTANT_AIM_FOLLOW_H
