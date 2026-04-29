#ifndef MELON_PRIME_DEF_H
#define MELON_PRIME_DEF_H

#include <cstdint>

namespace MelonPrime {

    // Global state (set by EmuInstance)
    extern uint32_t globalChecksum;
    extern bool isRomDetected;

    // =========================================================================
    // Config key string constants — avoid repeated string construction per frame
    // =========================================================================
    namespace CfgKey {
        inline constexpr const char* Joy2Key        = "Metroid.Apply.joy2KeySupport";
        inline constexpr const char* SnapTap         = "Metroid.Operation.SnapTap";
        inline constexpr const char* StylusMode      = "Metroid.Enable.stylusMode";
        inline constexpr const char* AimSens         = "Metroid.Sensitivity.Aim";
        inline constexpr const char* AimYScale       = "Metroid.Sensitivity.AimYAxisScale";
        inline constexpr const char* AimAdjust       = "Metroid.Aim.Adjust";
        inline constexpr const char* DisableMphAimSmoothing = "Metroid.Aim.Disable.MphAimSmoothing";
        inline constexpr const char* AimAccumulator = "Metroid.Aim.Enable.Accumulator";
        inline constexpr const char* ScreenSyncMode = "Metroid.Screen.SyncMode";
        inline constexpr const char* MphSens         = "Metroid.Sensitivity.Mph";
        inline constexpr const char* Headphone       = "Metroid.Apply.Headphone";
        inline constexpr const char* SfxVolApply     = "Metroid.Apply.SfxVolume";
        inline constexpr const char* SfxVol          = "Metroid.Volume.SFX";
        inline constexpr const char* MusicVolApply   = "Metroid.Apply.MusicVolume";
        inline constexpr const char* MusicVol        = "Metroid.Volume.Music";
        inline constexpr const char* LicColorApply   = "Metroid.HunterLicense.Color.Apply";
        inline constexpr const char* LicColorSel     = "Metroid.HunterLicense.Color.Selected";
        inline constexpr const char* HunterApply     = "Metroid.HunterLicense.Hunter.Apply";
        inline constexpr const char* HunterSel       = "Metroid.HunterLicense.Hunter.Selected";
        inline constexpr const char* UseFwName       = "Metroid.Use.Firmware.Name";
        inline constexpr const char* DataUnlock      = "Metroid.Data.Unlock";
        inline constexpr const char* FixShadowFreeze = "Metroid.BugFix.FixShadowFreeze";
    }


    namespace WeaponId {
        inline constexpr uint8_t PowerBeam   = 0;
        inline constexpr uint8_t VoltDriver  = 1;
        inline constexpr uint8_t Missile     = 2;
        inline constexpr uint8_t Battlehammer= 3;
        inline constexpr uint8_t Imperialist = 4;
        inline constexpr uint8_t Judicator   = 5;
        inline constexpr uint8_t Magmaul     = 6;
        inline constexpr uint8_t ShockCoil   = 7;
        inline constexpr uint8_t OmegaCannon = 8;
    }

    namespace WeaponMask {
        inline constexpr uint16_t PowerBeam    = static_cast<uint16_t>(1u << WeaponId::PowerBeam);
        inline constexpr uint16_t VoltDriver   = static_cast<uint16_t>(1u << WeaponId::VoltDriver);
        inline constexpr uint16_t Missile      = static_cast<uint16_t>(1u << WeaponId::Missile);
        inline constexpr uint16_t Battlehammer = static_cast<uint16_t>(1u << WeaponId::Battlehammer);
        inline constexpr uint16_t Imperialist  = static_cast<uint16_t>(1u << WeaponId::Imperialist);
        inline constexpr uint16_t Judicator    = static_cast<uint16_t>(1u << WeaponId::Judicator);
        inline constexpr uint16_t Magmaul      = static_cast<uint16_t>(1u << WeaponId::Magmaul);
        inline constexpr uint16_t ShockCoil    = static_cast<uint16_t>(1u << WeaponId::ShockCoil);
        inline constexpr uint16_t OmegaCannon  = static_cast<uint16_t>(1u << WeaponId::OmegaCannon);
    }

    namespace RomVersions {
        constexpr uint32_t US1_0           = 0x218DA42C;
        constexpr uint32_t US1_1           = 0x91B46577;
        constexpr uint32_t EU1_0           = 0xA4A8FE5A;
        constexpr uint32_t EU1_1           = 0x910018A5;
        constexpr uint32_t EU1_1_BALANCED  = 0x948B1E48;
        constexpr uint32_t EU1_1_RUSSIANED = 0x9E20F3A8;
        constexpr uint32_t JP1_0           = 0xD75F539D;
        constexpr uint32_t JP1_1           = 0x42EBF348;
        constexpr uint32_t KR1_0           = 0xE54682F3;
        constexpr uint32_t EU1_1_ENCRYPTED = 0x31703770;
        constexpr uint32_t EU1_0_ENCRYPTED = 0x979BB267;
        constexpr uint32_t JP1_1_ENCRYPTED = 0x0A1203A5;
        constexpr uint32_t JP1_0_ENCRYPTED = 0xE795A10C;
        constexpr uint32_t KR1_0_ENCRYPTED = 0xC26916F3;
        constexpr uint32_t US1_1_ENCRYPTED = 0x01476E8F;
        constexpr uint32_t US1_0_ENCRYPTED = 0xE048CD92;
    }

} // namespace MelonPrime

#endif // MELON_PRIME_DEF_H
