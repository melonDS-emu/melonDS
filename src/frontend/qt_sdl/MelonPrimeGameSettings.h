#ifndef MELONPRIME_SETTINGS_H
#define MELONPRIME_SETTINGS_H

#include <cstdint>
#include <array>
#include <algorithm>
#include <cmath>

#include "types.h"
#include "Config.h"

namespace melonDS { class NDS; }

namespace MelonPrime {
    struct RomAddresses;

    class MelonPrimeGameSettings
    {
    public:
        MelonPrimeGameSettings() = delete;

        static bool ApplyHeadphoneOnce(melonDS::NDS* nds, Config::Table& cfg, melonDS::u32 addr, uint8_t& flags, uint8_t bit);
        static bool ApplySfxVolumeOnce(melonDS::NDS* nds, Config::Table& cfg, melonDS::u32 addr, uint8_t& flags, uint8_t bit);
        static bool ApplyMusicVolumeOnce(melonDS::NDS* nds, Config::Table& cfg, melonDS::u32 addr, uint8_t& flags, uint8_t bit);
        static bool ApplyLicenseColorStrict(melonDS::NDS* nds, Config::Table& cfg, melonDS::u32 addr);
        static bool ApplySelectedHunterStrict(melonDS::NDS* nds, Config::Table& cfg, melonDS::u32 addr);
        static bool UseDsName(melonDS::NDS* nds, Config::Table& cfg, melonDS::u32 addr);
        static void ApplyMphSensitivity(melonDS::NDS* nds, Config::Table& cfg, melonDS::u32 addrSensi, melonDS::u32 addrInGame, bool inGameInit);
        static bool ApplyUnlockHuntersMaps(melonDS::NDS* nds, Config::Table& cfg,
            melonDS::u32 a1, melonDS::u32 a2, melonDS::u32 a3, melonDS::u32 a4, melonDS::u32 a5);
        static melonDS::u32 CalculatePlayerAddress(melonDS::u32 base, melonDS::u8 pos, int32_t inc);

        // --- Safe Anti-Smoothing Memory Patcher ---
        static void ApplyAimSmoothingPatch(melonDS::NDS* nds, const RomAddresses& rom, bool enable);

    private:
        static uint16_t SensiNumToSensiVal(double sensiNum);
    };

} // namespace MelonPrime

#endif // MELONPRIME_SETTINGS_H