#ifndef MELON_PRIME_PATCH_SHOW_HEADSHOT_ONLINE_H
#define MELON_PRIME_PATCH_SHOW_HEADSHOT_ONLINE_H

#ifdef MELONPRIME_DS

#include <cstdint>

namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

void ShowHeadshotOnline_ApplyOnce(melonDS::NDS* nds, Config::Table& cfg, uint8_t romGroupIndex);
void ShowHeadshotOnline_RestoreOnce(melonDS::NDS* nds, uint8_t romGroupIndex);
void ShowHeadshotOnline_ResetPatchState();

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_PATCH_SHOW_HEADSHOT_ONLINE_H
