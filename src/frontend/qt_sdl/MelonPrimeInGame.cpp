#include "MelonPrimeInternal.h"
#include "MelonPrimeGameSettings.h"
#include "EmuInstance.h"
#include "NDS.h"
#include "main.h"
#include "Screen.h"
#include "MelonPrimeDef.h"
#include "MelonPrimeGameRomAddrTable.h"
#ifdef _WIN32
#include "MelonPrimeRawInputWinFilter.h"
#endif

namespace MelonPrime {

    // =========================================================================
    // HandleInGameLogic - per-frame in-game update
    // Optimized with Hot/Cold splitting to minimize instruction cache pressure.
    // =========================================================================
    HOT_FUNCTION void MelonPrimeCore::HandleInGameLogic()
    {
        PREFETCH_READ(m_ptrs.isAltForm);
        const bool isStylusMode = this->isStylusMode;
        // Early prefetch of aim pointers - gives ~50-100 instructions of
        // lead time before ProcessAimInputMouse reads them, hiding potential L2 miss.
        if (LIKELY(!isStylusMode)) {
            PREFETCH_WRITE(m_ptrs.aimX);
            PREFETCH_WRITE(m_ptrs.aimY);
        }
        // Cache NDS pointer - avoids repeated emuInstance->getNDS() pointer chase.
        auto* const nds = emuInstance->getNDS();

        // --- Rare Actions (Morph, Weapon Switch) ---
        if (UNLIKELY(IsPressed(IB_MORPH))) {
            HandleRareMorph();
        }

        // Combined weapon input gate.
        //   Single bitmask test + wheelDelta check skips call entirely on 99%+ frames.
        {
            constexpr uint64_t IB_WEAPON_ALL_TRIGGERS =
                IB_WEAPON_ANY | IB_WEAPON_NEXT | IB_WEAPON_PREV;
            const bool hasWeaponInput =
                (m_input.press & IB_WEAPON_ALL_TRIGGERS) || m_input.wheelDelta;
            if (UNLIKELY(hasWeaponInput && ProcessWeaponSwitch())) {
                HandleRareWeaponSwitch();
            }
        }

        // --- Adventure Mode ---
        if (UNLIKELY(m_flags.test(StateFlags::BIT_IN_ADVENTURE))) {
            const bool isPaused = (*m_ptrs.isMapOrUserActionPaused) == 0x1;
            m_flags.assign(StateFlags::BIT_PAUSED, isPaused);

            if (IsAnyPressed(IB_SCAN_VISOR | IB_UI_ANY)) {
                HandleAdventureMode();
            }
        }

        // --- Weapon Check ---
        if (IsDown(IB_WEAPON_CHECK)) {
            const bool isOmegaCannonFlagActive =
                ((*m_ptrs.havingWeapons & WeaponMask::OmegaCannon) != 0);

            if (UNLIKELY(isOmegaCannonFlagActive)) {
                if (UNLIKELY(m_isWeaponCheckActive)) {
                    HandleRareWeaponCheckEnd();
                }
                if (IsPressed(IB_WEAPON_CHECK)) {
                    emuInstance->osdAddMessage(0, "Weapon Check is unavailable while Omega Cannon is active!");
                }
            }
            else {
                if (!m_isWeaponCheckActive) {
                    HandleRareWeaponCheckStart();
                }
                using namespace Consts::UI;
                nds->TouchScreen(WEAPON_CHECK_START.x(), WEAPON_CHECK_START.y());
            }
        }
        else if (UNLIKELY(m_isWeaponCheckActive)) {
            HandleRareWeaponCheckEnd();
        }

        // --- Movement & Buttons (Hot Path) ---
        ProcessMoveAndButtonsFastFromReset();

        // --- Morph Boost & Aim (Hot Path) ---
        HandleMorphBallBoost();

        if (isStylusMode) {
            if (!m_flags.test(StateFlags::BIT_BLOCK_STYLUS)) {
                ProcessAimInputStylus(nds);
            }
        }
        else {
#ifdef _WIN32
            // P-47: LateLatch only matters when FrameAdvance was called since
            // PollAndSnapshot (morph: ~96 ms, weapon: ~32 ms).  On normal frames
            // the window is ~40–100 ns → kernel buffer is empty.
            // Skipping processRawInputBatched saves ~500–2000 cyc/frame.
            if (m_rawFilter && m_didFrameAdvanceSinceSnapshot)
                m_rawFilter->LateLatchMouseDelta(m_input.mouseX, m_input.mouseY);
#endif
            ProcessAimInputMouse();
            // m_aimBlockBits replaces m_isAimDisabled (same semantics: != 0)
            if (!m_flags.test(StateFlags::BIT_LAST_FOCUSED) || !m_aimBlockBits) {
                using namespace Consts::UI;
                nds->TouchScreen(CENTER_RESET.x(), CENTER_RESET.y());
            }
        }
    }

    // =========================================================================
    // Outlined Cold Paths
    // =========================================================================

    COLD_FUNCTION void MelonPrimeCore::HandleRareMorph()
    {
        if (isStylusMode) m_flags.set(StateFlags::BIT_BLOCK_STYLUS);
        auto* nds = emuInstance->getNDS();
        nds->ReleaseScreen();
        FrameAdvanceTwice();
        using namespace Consts::UI;
        nds->TouchScreen(MORPH_START.x(), MORPH_START.y());
        FrameAdvanceTwice();
        nds->ReleaseScreen();
        FrameAdvanceTwice();
    }

    COLD_FUNCTION void MelonPrimeCore::HandleRareWeaponSwitch()
    {
        if (isStylusMode) m_flags.set(StateFlags::BIT_BLOCK_STYLUS);
    }

    COLD_FUNCTION void MelonPrimeCore::HandleRareWeaponCheckStart()
    {
        if (isStylusMode) m_flags.set(StateFlags::BIT_BLOCK_STYLUS);
        m_isWeaponCheckActive = true;
        SetAimBlockBranchless(AIMBLK_CHECK_WEAPON, true);
        emuInstance->getNDS()->ReleaseScreen();
        FrameAdvanceTwice();
    }

    COLD_FUNCTION void MelonPrimeCore::HandleRareWeaponCheckEnd()
    {
        m_isWeaponCheckActive = false;
        emuInstance->getNDS()->ReleaseScreen();
        SetAimBlockBranchless(AIMBLK_CHECK_WEAPON, false);
        FrameAdvanceTwice();
    }

    // =========================================================================
    // HandleAdventureMode
    //
    // REFACTORED: Replaced TOUCH_IF_PRESSED preprocessor macro with a
    // constexpr table + loop. Benefits:
    //   - Type-safe: no macro expansion surprises
    //   - Debuggable: breakpoints work on individual iterations
    //   - Maintainable: adding a new UI button is a single table entry
    //   - Same codegen: compiler unrolls small constexpr loops
    // =========================================================================
    COLD_FUNCTION void MelonPrimeCore::HandleAdventureMode()
    {
        auto* nds = emuInstance->getNDS();

        if (IsPressed(IB_SCAN_VISOR)) {
            if (isStylusMode) m_flags.set(StateFlags::BIT_BLOCK_STYLUS);

            nds->ReleaseScreen();
            FrameAdvanceTwice();
            using namespace Consts::UI;
            nds->TouchScreen(SCAN_VISOR_BUTTON.x(), SCAN_VISOR_BUTTON.y());

            if ((*m_ptrs.isInVisorOrMap) == 0x1) {
                FrameAdvanceTwice();
            }
            else {
                // Loop body reduced to bare FrameAdvanceOnce.
                // Re-entrant path handles all input.
                for (int i = 0; i < 30; ++i) {
                    FrameAdvanceOnce();
                }
            }
            nds->ReleaseScreen();
            FrameAdvanceTwice();
        }

        // --- UI Touch Buttons (data-driven) ---
        if (IsAnyPressed(IB_UI_ANY)) {
            struct UIAction {
                uint64_t bit;
                QPoint   point;
            };
            // constexpr array replaces 5 TOUCH_IF_PRESSED macro invocations.
            // Compiler unrolls this loop since the array size is known at compile time.
            static constexpr UIAction kUIActions[] = {
                { IB_UI_OK,    Consts::UI::OK    },
                { IB_UI_LEFT,  Consts::UI::LEFT  },
                { IB_UI_RIGHT, Consts::UI::RIGHT },
                { IB_UI_YES,   Consts::UI::YES   },
                { IB_UI_NO,    Consts::UI::NO    },
            };

            for (const auto& action : kUIActions) {
                if (IsPressed(action.bit)) {
                    nds->ReleaseScreen();
                    FrameAdvanceTwice();
                    nds->TouchScreen(action.point.x(), action.point.y());
                    FrameAdvanceTwice();
                }
            }
        }
    }

    // =========================================================================
    // Hot Helpers
    // =========================================================================

    HOT_FUNCTION bool MelonPrimeCore::HandleMorphBallBoost()
    {
        // P-36: Combined early exit — skip both samus check AND boost-down check
        // in a single branch. ~70%+ of frames hit this (non-Samus or no boost key).
        // Original had nested if/else with the aimBlock cleanup duplicated in the
        // else branch; now the common fast path (no boost) is a single LIKELY test.
        if (LIKELY(!m_flags.test(StateFlags::BIT_IS_SAMUS) || !IsDown(IB_MORPH_BOOST))) {
            // Guard redundant store - 99%+ of frames this bit is already 0.
            if (UNLIKELY(m_aimBlockBits & AIMBLK_MORPHBALL_BOOST)) {
                SetAimBlockBranchless(AIMBLK_MORPHBALL_BOOST, false);
            }
            return false;
        }

        // Samus + boost key held — full logic (rare path)
        const bool isAltForm = (*m_ptrs.isAltForm) == 0x02;
        m_flags.assign(StateFlags::BIT_IS_ALT_FORM, isAltForm);

        if (isAltForm) {
            const uint8_t boostGauge = *m_ptrs.boostGauge;
            const bool isBoosting = (*m_ptrs.isBoosting) != 0x00;
            const bool gaugeEnough = boostGauge > 0x0A;

            SetAimBlockBranchless(AIMBLK_MORPHBALL_BOOST, true);

            if (!IsDown(IB_WEAPON_CHECK)) {
                emuInstance->getNDS()->ReleaseScreen();
            }

            InputSetBranchless(INPUT_R, !isBoosting && gaugeEnough);

            if (isBoosting) {
                SetAimBlockBranchless(AIMBLK_MORPHBALL_BOOST, false);
            }
            return true;
        }

        return false;
    }

    COLD_FUNCTION void MelonPrimeCore::ApplyGameSettingsOnce()
    {
        InputSetBranchless(INPUT_L, !IsPressed(IB_UI_LEFT));
        InputSetBranchless(INPUT_R, !IsPressed(IB_UI_RIGHT));

        auto* nds = emuInstance->getNDS();

        MelonPrimeGameSettings::ApplyHeadphone(nds, localCfg, m_currentRom.operationAndSound);

        MelonPrimeGameSettings::ApplyMphSensitivity(
            nds, localCfg, m_currentRom.sensitivity,
            m_currentRom.baseInGameSensi, m_flags.test(StateFlags::BIT_IN_GAME_INIT));

        MelonPrimeGameSettings::ApplyUnlockHuntersMaps(
            nds, localCfg,
            m_currentRom.unlockMapsHunters, m_currentRom.unlockMapsHunters2,
            m_currentRom.unlockMapsHunters3, m_currentRom.unlockMapsHunters4,
            m_currentRom.unlockMapsHunters5);

        MelonPrimeGameSettings::UseDsName(nds, localCfg, m_currentRom.dsNameFlagAndMicVolume);
        MelonPrimeGameSettings::ApplySelectedHunterStrict(nds, localCfg, m_currentRom.mainHunter);
        MelonPrimeGameSettings::ApplyLicenseColorStrict(nds, localCfg, m_currentRom.rankColor);

        MelonPrimeGameSettings::ApplySfxVolume(nds, localCfg, m_currentRom.volSfx8Bit);
        MelonPrimeGameSettings::ApplyMusicVolume(nds, localCfg, m_currentRom.volMusic8Bit);
    }

} // namespace MelonPrime
