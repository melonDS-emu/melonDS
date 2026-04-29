#include "MelonPrimeGameSettings.h"
#include "MelonPrimeDef.h"
#include "MelonPrimeGameRomAddrTable.h"
#include "NDS.h"

namespace MelonPrime {

    uint16_t MelonPrimeGameSettings::SensiNumToSensiVal(double sensiNum)
    {
        constexpr double BASE_VAL = 2457.0; // 0x0999
        constexpr double STEP_VAL = 409.0;  // 0x0199

        // FMA-friendly: val + 0.5 then cast is a standard fast-round for positive numbers
        const double val = BASE_VAL + (sensiNum - 1.0) * STEP_VAL;
        return static_cast<uint16_t>(std::min(static_cast<uint32_t>(val + 0.5), 0xFFFFu));
    }

    bool MelonPrimeGameSettings::ApplyHeadphone(
        melonDS::NDS* nds, Config::Table& localCfg, melonDS::u32 addr)
    {
        if (!nds || !localCfg.GetBool(CfgKey::Headphone)) return false;

        const uint8_t oldVal = nds->ARM9Read8(addr);
        constexpr uint8_t kMask = 0x18;
        if ((oldVal & kMask) == kMask) return false;

        nds->ARM9Write8(addr, oldVal | kMask);
        return true;
    }

    bool MelonPrimeGameSettings::ApplyLicenseColorStrict(
        melonDS::NDS* nds, Config::Table& localCfg, melonDS::u32 addr)
    {
        if (!nds || !localCfg.GetBool(CfgKey::LicColorApply)) return false;

        const int sel = localCfg.GetInt(CfgKey::LicColorSel);
        if (sel < 0 || sel > 2) return false;

        constexpr std::array<uint8_t, 3> kColorBits = { 0x00, 0x40, 0x80 };
        const uint8_t oldVal = nds->ARM9Read8(addr);
        const uint8_t newVal = (oldVal & 0x3F) | kColorBits[sel];
        if (newVal == oldVal) return false;

        nds->ARM9Write8(addr, newVal);
        return true;
    }

    bool MelonPrimeGameSettings::ApplySelectedHunterStrict(
        melonDS::NDS* nds, Config::Table& localCfg, melonDS::u32 addr)
    {
        if (!nds || !localCfg.GetBool(CfgKey::HunterApply)) return false;

        constexpr std::array<uint8_t, 7> kHunterBits = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30 };
        const int sel = std::clamp(localCfg.GetInt(CfgKey::HunterSel), 0, 6);

        const uint8_t oldVal = nds->ARM9Read8(addr);
        const uint8_t newVal = (oldVal & ~0x78) | (kHunterBits[sel] & 0x78);
        if (newVal == oldVal) return false;

        nds->ARM9Write8(addr, newVal);
        return true;
    }

    bool MelonPrimeGameSettings::UseDsName(
        melonDS::NDS* nds, Config::Table& localCfg, melonDS::u32 addr)
    {
        if (!nds || !localCfg.GetBool(CfgKey::UseFwName)) return false;

        const uint8_t oldVal = nds->ARM9Read8(addr);
        const uint8_t newVal = oldVal & ~0x01;
        if (newVal == oldVal) return false;

        nds->ARM9Write8(addr, newVal);
        return true;
    }

    bool MelonPrimeGameSettings::ApplySfxVolume(
        melonDS::NDS* nds, Config::Table& localCfg, melonDS::u32 addr)
    {
        if (!nds || !localCfg.GetBool(CfgKey::SfxVolApply)) return false;

        const uint8_t oldVal = nds->ARM9Read8(addr);
        const uint8_t steps = static_cast<uint8_t>(std::clamp(localCfg.GetInt(CfgKey::SfxVol), 0, 9));
        const uint8_t newVal = (oldVal & 0xC0) | ((steps & 0x0F) << 2) | 0x03;

        if (newVal == oldVal) return false;
        nds->ARM9Write8(addr, newVal);
        return true;
    }

    bool MelonPrimeGameSettings::ApplyMusicVolume(
        melonDS::NDS* nds, Config::Table& localCfg, melonDS::u32 addr)
    {
        if (!nds || !localCfg.GetBool(CfgKey::MusicVolApply)) return false;

        const uint8_t oldVal = nds->ARM9Read8(addr);
        const uint8_t steps = static_cast<uint8_t>(std::clamp(localCfg.GetInt(CfgKey::MusicVol), 0, 9));
        const uint8_t newVal = (oldVal & ~0x3C) | ((steps & 0x0F) << 2);

        if (newVal == oldVal) return false;
        nds->ARM9Write8(addr, newVal);
        return true;
    }

    void MelonPrimeGameSettings::ApplyMphSensitivity(
        melonDS::NDS* nds, Config::Table& localCfg, melonDS::u32 addrSensi, melonDS::u32 addrInGame, bool inGameInit)
    {
        const double mphSensi = localCfg.GetDouble(CfgKey::MphSens);
        const uint16_t sensiVal = SensiNumToSensiVal(mphSensi);
        nds->ARM9Write16(addrSensi, sensiVal);
        if (inGameInit) nds->ARM9Write16(addrInGame, sensiVal);
    }

    bool MelonPrimeGameSettings::ApplyUnlockHuntersMaps(
        melonDS::NDS* nds, Config::Table& localCfg,
        melonDS::u32 a1, melonDS::u32 a2, melonDS::u32 a3, melonDS::u32 a4, melonDS::u32 a5)
    {
        if (!localCfg.GetBool(CfgKey::DataUnlock)) return false;

        // Read-before-write: only write if the value was reset by the game.
        // On most frames nothing has changed so this is effectively a no-op.
        bool changed = false;
        const uint8_t v1 = nds->ARM9Read8(a1);
        if ((v1 & 0x03) != 0x03)               { nds->ARM9Write8(a1, v1 | 0x03);  changed = true; }
        if (nds->ARM9Read32(a2) != 0x07FFFFFFu) { nds->ARM9Write32(a2, 0x07FFFFFFu); changed = true; }
        if (nds->ARM9Read8(a3)  != 0x7F)        { nds->ARM9Write8(a3, 0x7F);       changed = true; }
        if (nds->ARM9Read32(a4) != 0xFFFFFFFFu) { nds->ARM9Write32(a4, 0xFFFFFFFFu); changed = true; }
        if (nds->ARM9Read8(a5)  != 0x7F)        { nds->ARM9Write8(a5, 0x7F);       changed = true; }
        return changed;
    }

    melonDS::u32 MelonPrimeGameSettings::CalculatePlayerAddress(
        melonDS::u32 base, melonDS::u8 pos, int32_t inc)
    {
        if (pos == 0) return base;
        const int64_t result = static_cast<int64_t>(base) + (static_cast<int64_t>(pos) * inc);
        if (result < 0 || result > UINT32_MAX) return base;
        return static_cast<melonDS::u32>(result);
    }

    // =========================================================================
    // Safe Anti-Smoothing ASM Memory Patcher
    //
    // This safely extracts C++ injected data by shifting the read instruction
    // to +0x3C / +0x44 (which avoids the game's zero-overwrite mechanism),
    // and then skips the 4-frame moving average.
    // It verifies the exact expected assembly instructions exist before patching.
    // =========================================================================
    void MelonPrimeGameSettings::ApplyAimSmoothingPatch(melonDS::NDS* nds, const RomAddresses& rom, bool enable)
    {
        if (!nds || rom.aimPatchAddrX == 0) return;

        // "b +0x18" : Jumps exactly to the final strh instruction, skipping all smoothing
        constexpr uint32_t JUMP_INSTR = 0xEA000006;

        if (enable) {
            // Apply X patch if original instructions perfectly match (Crash Prevention Guard)
            if (nds->ARM9Read32(rom.aimPatchAddrX) == rom.aimPatchOrigX1 &&
                nds->ARM9Read32(rom.aimPatchAddrX + 4) == rom.aimPatchOrigX2) {
                nds->ARM9Write32(rom.aimPatchAddrX, rom.aimPatchX1);
                nds->ARM9Write32(rom.aimPatchAddrX + 4, JUMP_INSTR);
            }
            // Apply Y patch if original instructions perfectly match
            if (nds->ARM9Read32(rom.aimPatchAddrY) == rom.aimPatchOrigY1 &&
                nds->ARM9Read32(rom.aimPatchAddrY + 4) == rom.aimPatchOrigY2) {
                nds->ARM9Write32(rom.aimPatchAddrY, rom.aimPatchY1);
                nds->ARM9Write32(rom.aimPatchAddrY + 4, JUMP_INSTR);
            }
        }
        else {
            // Restore X patch if it was previously patched by us
            if (nds->ARM9Read32(rom.aimPatchAddrX) == rom.aimPatchX1 &&
                nds->ARM9Read32(rom.aimPatchAddrX + 4) == JUMP_INSTR) {
                nds->ARM9Write32(rom.aimPatchAddrX, rom.aimPatchOrigX1);
                nds->ARM9Write32(rom.aimPatchAddrX + 4, rom.aimPatchOrigX2);
            }
            // Restore Y patch if it was previously patched by us
            if (nds->ARM9Read32(rom.aimPatchAddrY) == rom.aimPatchY1 &&
                nds->ARM9Read32(rom.aimPatchAddrY + 4) == JUMP_INSTR) {
                nds->ARM9Write32(rom.aimPatchAddrY, rom.aimPatchOrigY1);
                nds->ARM9Write32(rom.aimPatchAddrY + 4, rom.aimPatchOrigY2);
            }
        }
    }

} // namespace MelonPrime