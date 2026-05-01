#include "MelonPrimeInternal.h"
#include "EmuInstance.h"
#include "NDS.h"
#include "MelonPrimeDef.h"
#include "MelonPrimeGameRomAddrTable.h"
#ifdef MELONPRIME_DS
#include "MelonPrimeArm9Hook.h"
#endif

#include <array>

namespace MelonPrime {

    COLD_FUNCTION void MelonPrimeCore::DetectRomAndSetAddresses()
    {
        struct RomInfo {
            uint32_t    checksum;
            const char* name;
            RomGroup    group;
        };

        static const std::array<RomInfo, 17> ROM_INFO_TABLE = { {
            { RomVersions::US1_1,           "US1.1",           RomGroup::US1_1 },
            { RomVersions::US1_1_ENCRYPTED, "US1.1 ENCRYPTED", RomGroup::US1_1 },
            { RomVersions::US1_0,           "US1.0",           RomGroup::US1_0 },
            { RomVersions::US1_0_ENCRYPTED, "US1.0 ENCRYPTED", RomGroup::US1_0 },
            { RomVersions::EU1_1,           "EU1.1",           RomGroup::EU1_1 },
            { RomVersions::EU1_1_ENCRYPTED, "EU1.1 ENCRYPTED", RomGroup::EU1_1 },
            { RomVersions::EU1_1_BALANCED,  "EU1.1 BALANCED",  RomGroup::EU1_1 },
            { RomVersions::EU1_1_BALANCED_V1_2_11, "EU1.1 BALANCED V1.2.11", RomGroup::EU1_1 },
            { RomVersions::EU1_1_RUSSIANED, "EU1.1 RUSSIANED", RomGroup::EU1_1 },
            { RomVersions::EU1_0,           "EU1.0",           RomGroup::EU1_0 },
            { RomVersions::EU1_0_ENCRYPTED, "EU1.0 ENCRYPTED", RomGroup::EU1_0 },
            { RomVersions::JP1_0,           "JP1.0",           RomGroup::JP1_0 },
            { RomVersions::JP1_0_ENCRYPTED, "JP1.0 ENCRYPTED", RomGroup::JP1_0 },
            { RomVersions::JP1_1,           "JP1.1",           RomGroup::JP1_1 },
            { RomVersions::JP1_1_ENCRYPTED, "JP1.1 ENCRYPTED", RomGroup::JP1_1 },
            { RomVersions::KR1_0,           "KR1.0",           RomGroup::KR1_0 },
            { RomVersions::KR1_0_ENCRYPTED, "KR1.0 ENCRYPTED", RomGroup::KR1_0 },
        } };

        const RomInfo* romInfo = nullptr;
        for (const auto& info : ROM_INFO_TABLE) {
            if (globalChecksum == info.checksum) {
                romInfo = &info;
                break;
            }
        }

        if (!romInfo) return;

        // Copy the full address set for this ROM variant
        m_currentRom = *getRomAddrsPtr(romInfo->group);

        // --- Initialize hot addresses from base values ---
        auto& hot = m_addrHot;
        const auto& rom = m_currentRom;

        hot.inGame                  = rom.inGame;
        hot.isMapOrUserActionPaused = rom.isMapOrUserActionPaused;

        // Player-relative (base values, recalculated on player position change)
        hot.isAltForm           = rom.baseIsAltForm;
        hot.jumpFlag            = rom.baseJumpFlag;
        hot.weaponChange        = rom.baseWeaponChange;
        hot.selectedWeapon      = rom.baseSelectedWeapon;
        hot.aimX                = rom.baseAimX;
        hot.aimY                = rom.baseAimY;
        hot.loadedSpecialWeapon = rom.baseLoadedSpecialWeapon;
        hot.boostGauge          = rom.boostGauge;
        hot.isBoosting          = rom.isBoosting;
        hot.isInVisorOrMap      = rom.isInVisorOrMap;
        hot.chosenHunter        = rom.baseChosenHunter;
        hot.inGameSensi         = rom.baseInGameSensi;
        hot.currentWeapon       = rom.baseCurrentWeapon;
        hot.havingWeapons       = rom.baseHavingWeapons;
        hot.weaponAmmo          = rom.baseWeaponAmmo;

        m_flags.set(StateFlags::BIT_ROM_DETECTED);

        // OPT-L: Resolve inGame pointer immediately — it's read every frame
        //   before the in-game init block runs, so it must be available as soon
        //   as BIT_ROM_DETECTED is set. Other m_ptrs.* are resolved later in
        //   the per-game-join init block (player-position dependent).
        {
            melonDS::u8* ram = emuInstance->getNDS()->MainRAM;
            m_ptrs.inGame = GetRamPointer<uint16_t>(ram, m_addrHot.inGame);
        }

#ifdef MELONPRIME_DS
        ARM9Hook_Install(
            emuInstance->getNDS(),
            localCfg,
            m_currentRom.romGroupIndex);
#endif

        char message[256];
        snprintf(message, sizeof(message), "MPH Rom Detected: %s", romInfo->name);
        emuInstance->osdAddMessage(0, message);

        RecalcAimSensitivityCache(localCfg);
        ApplyAimAdjustSetting(localCfg);
    }

} // namespace MelonPrime
