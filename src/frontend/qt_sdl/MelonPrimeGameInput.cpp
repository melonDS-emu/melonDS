#include "MelonPrimeInternal.h"
#include "EmuInstance.h"
#include "NDS.h"
#include "main.h"
#include "Screen.h"
#include "MelonPrimeDef.h"

#include <cstdint>
#include <cstring>
#include <utility>
#include <QCursor>

#ifdef _WIN32
#include "MelonPrimeRawInputWinFilter.h"
#include "MelonPrimeRawInputState.h"
#endif

namespace MelonPrime {

    alignas(64) static constexpr std::array<uint8_t, 16> MoveLUT = {
        0xF0, 0xB0, 0x70, 0xF0,  0xD0, 0x90, 0x50, 0xD0,
        0xE0, 0xA0, 0x60, 0xE0,  0xF0, 0xB0, 0x70, 0xF0,
    };

    // V7: Grouped bit projection.
    //
    // V6 already removed the generic table walk. This version goes one step
    // further and exploits contiguous hotkey ranges so the compiler can emit a
    // few shifts / masks instead of many independent high-bit tests.
    //
    // The assumptions are guarded with static_assert so future enum reordering
    // cannot silently break the packed projections.
    static_assert(HK_MetroidMoveBack        == HK_MetroidMoveForward + 1, "Move group must stay contiguous");
    static_assert(HK_MetroidMoveLeft        == HK_MetroidMoveForward + 2, "Move group must stay contiguous");
    static_assert(HK_MetroidMoveRight       == HK_MetroidMoveForward + 3, "Move group must stay contiguous");
    static_assert(HK_MetroidUILeft          == HK_MetroidScanVisor + 1,   "UI group layout changed");
    static_assert(HK_MetroidUIRight         == HK_MetroidScanVisor + 2,   "UI group layout changed");
    static_assert(HK_MetroidUIOk            == HK_MetroidScanVisor + 3,   "UI group layout changed");
    static_assert(HK_MetroidUIYes           == HK_MetroidScanVisor + 4,   "UI group layout changed");
    static_assert(HK_MetroidUINo            == HK_MetroidScanVisor + 5,   "UI group layout changed");
    static_assert(HK_MetroidWeaponMissile   == HK_MetroidWeaponBeam + 1,  "Weapon group layout changed");
    static_assert(HK_MetroidWeaponSpecial   == HK_MetroidWeaponBeam + 2,  "Weapon group layout changed");
    static_assert(HK_MetroidWeaponNext      == HK_MetroidWeaponBeam + 3,  "Weapon group layout changed");
    static_assert(HK_MetroidWeaponPrevious  == HK_MetroidWeaponBeam + 4,  "Weapon group layout changed");
    static_assert(HK_MetroidWeapon1         == HK_MetroidWeaponBeam + 5,  "Weapon group layout changed");
    static_assert(HK_MetroidWeapon6         == HK_MetroidWeaponBeam + 10, "Weapon group layout changed");

    struct ProjectedDownState {
        uint64_t mask;
        uint32_t moveIndex;
    };

    [[nodiscard]] FORCE_INLINE ProjectedDownState ProjectDownState(const uint64_t hotMask) noexcept {
        const uint32_t moveBits = static_cast<uint32_t>((hotMask >> HK_MetroidMoveForward) & 0xFULL);
        uint64_t down = static_cast<uint64_t>(moveBits) << 6;

        down |= ((hotMask >> HK_MetroidJump)               & 1ULL) << 0;
        down |= ((((hotMask >> HK_MetroidShootScan) |
                   (hotMask >> HK_MetroidScanShoot))       & 1ULL) << 1);
        down |= ((hotMask >> HK_MetroidZoom)               & 1ULL) << 2;
        down |= ((hotMask >> HK_MetroidHoldMorphBallBoost) & 1ULL) << 4;
        down |= ((hotMask >> HK_MetroidWeaponCheck)        & 1ULL) << 5;
        down |= ((hotMask >> HK_MetroidMenu)               & 1ULL) << 10;

        return { down, moveBits };
    }

    [[nodiscard]] FORCE_INLINE uint64_t ProjectPressMask(const uint64_t hotMask) noexcept {
        const uint64_t uiBits = (hotMask >> HK_MetroidUILeft) & 0x1FULL;
        const uint64_t weaponBits = (hotMask >> HK_MetroidWeaponBeam) & 0x7FFULL;

        uint64_t press = 0;
        press |= ((hotMask >> HK_MetroidMorphBall) & 1ULL) << 3;
        press |= ((hotMask >> HK_MetroidWeaponCheck) & 1ULL) << 5;
        press |= ((hotMask >> HK_MetroidScanVisor) & 1ULL) << 11;

        // UI order in hotkeys: Left Right Ok Yes No
        // UI order in IB bits : Ok(12) Left(13) Right(14) Yes(15) No(16)
        press |= (uiBits & 0x3ULL) << 13;         // Left / Right
        press |= ((uiBits >> 2) & 0x1ULL) << 12;  // Ok
        press |= ((uiBits >> 3) & 0x3ULL) << 15;  // Yes / No

        // Weapon order in hotkeys:
        //   Beam Missile Special Next Prev 1 2 3 4 5 6
        // Weapon order in IB bits:
        //   17   18      25      26   27  19..24
        press |= (weaponBits & 0x3ULL) << 17;          // Beam / Missile
        press |= ((weaponBits >> 5) & 0x3FULL) << 19;  // Weapon1..6
        press |= ((weaponBits >> 2) & 0x1ULL) << 25;   // Special
        press |= ((weaponBits >> 3) & 0x3ULL) << 26;   // Next / Prev
        return press;
    }

    // =========================================================================
    // UpdateInputStateImpl<kReentrant>
    //
    // Unified implementation for UpdateInputState (kReentrant=false) and
    // UpdateInputStateReentrant (kReentrant=true). if constexpr branches are
    // completely eliminated at compile time — zero overhead vs hand-written
    // duplicates.
    //
    // kReentrant=false (full path):
    //   - PollAndSnapshot        : drains WM_INPUT and latches edge state
    //   - reads press mask       : ProjectPressMask from hotPressMask
    //   - reads wheelDelta       : from cached panel (P-3)
    //
    // kReentrant=true (re-entrant FrameAdvance path):
    //   - PollAndSnapshotNoEdges : drains WM_INPUT, no edge latch
    //   - press = 0              : outer-frame press detection preserved
    //   - wheelDelta = 0         : never consumed mid-frame
    // =========================================================================
    template <bool kReentrant>
    FORCE_INLINE void MelonPrimeCore::UpdateInputStateImpl(const bool focused)
    {
#ifdef _WIN32
        auto* const rawFilter = m_rawFilter.get();

        // OPT-Z3: Always poll to drain WM_INPUT even when unfocused,
        // preventing message buildup and stale delta accumulation. [FIX-2]
        FrameHotkeyState hk{};
        if (rawFilter) {
            if constexpr (kReentrant)
                rawFilter->PollAndSnapshotNoEdges(hk, m_input.mouseX, m_input.mouseY);
            else {
                rawFilter->PollAndSnapshot(hk, m_input.mouseX, m_input.mouseY);
                // P-47: Kernel buffer just drained; no FrameAdvance has occurred yet.
                // LateLatch skips processRawInputBatched on frames with no FrameAdvance.
                m_didFrameAdvanceSinceSnapshot = false;
            }
        }
#endif

        if (!focused) {
            m_input.down = 0;
            m_input.press = 0;
            m_input.moveIndex = 0;
            m_input.mouseX = 0;
            m_input.mouseY = 0;
            m_input.wheelDelta = 0;
            m_snapState = 0;
            // Re-entrant FrameAdvance does not call InputReset before rebuilding
            // the fast DS mask. Release it here so stale non-movement bits
            // cannot survive a focus loss.
            m_inputMaskFast = 0xFFFF;
            return;
        }

#ifdef _WIN32
        const uint64_t hotDownMask = hk.down | emuInstance->joyHotkeyMask;
        if constexpr (!kReentrant)
            m_input.press = ProjectPressMask(hk.pressed | emuInstance->joyHotkeyPress);
        else
            m_input.press = 0;
#else
        const uint64_t hotDownMask = emuInstance->hotkeyMask;
        if constexpr (!kReentrant)
            m_input.press = ProjectPressMask(emuInstance->hotkeyPress);
        else
            m_input.press = 0;
#endif

        const ProjectedDownState downState = ProjectDownState(hotDownMask);
        m_input.down = downState.mask;
        m_input.moveIndex = downState.moveIndex;

#if !defined(_WIN32)
        const QPoint currentPos = QCursor::pos();
        m_input.mouseX = currentPos.x() - m_aimData.centerX;
        m_input.mouseY = currentPos.y() - m_aimData.centerY;
#endif

        if constexpr (!kReentrant)
            m_input.wheelDelta = m_cachedPanel ? m_cachedPanel->getDelta() : 0;
        else
            m_input.wheelDelta = 0;
    }

    HOT_FUNCTION void MelonPrimeCore::UpdateInputState(const bool focused)          { UpdateInputStateImpl<false>(focused); }
    HOT_FUNCTION void MelonPrimeCore::UpdateInputStateReentrant(const bool focused) { UpdateInputStateImpl<true>(focused);  }

    // OPT-Z2: Unified move + button mask update.
    //
    //   Previously split across ProcessMoveInputFast() and an inline block,
    //   causing two separate store-load cycles on m_inputMaskFast and code
    //   duplication between HandleInGameLogic and RunFrameHook re-entrant path.
    //
    //   Now: single function, single store to m_inputMaskFast, zero duplication.
    //   Also enables the compiler to keep m_inputMaskFast in a register across
    //   the move LUT lookup and button merge.
    template <bool kInputMaskReset>
    FORCE_INLINE void MelonPrimeCore::ProcessMoveAndButtonsFastImpl()
    {
        const uint32_t curr = m_input.moveIndex;
        uint32_t finalInput;

        if (LIKELY(!m_snapTapMode)) {
            finalInput = curr;
        }
        else {
            const uint32_t last = m_snapState & 0xFFu;
            const uint32_t priority = m_snapState >> 8;
            const uint32_t newPress = curr & ~last;

            const uint32_t conflictFB = ((curr & 0x3u) == 0x3u) ? 0x3u : 0u;
            const uint32_t conflictLR = ((curr & 0xCu) == 0xCu) ? 0xCu : 0u;
            const uint32_t conflict = conflictFB | conflictLR;

            const bool hasNewConflict = (newPress & conflict) != 0;
            const uint32_t updateMask = hasNewConflict ? ~0u : 0u;

            const uint32_t newPriority = (priority & ~(conflict & updateMask)) | (newPress & conflict & updateMask);
            const uint32_t activePriority = newPriority & curr;

            m_snapState = static_cast<uint16_t>((curr & 0xFFu) | ((activePriority & 0xFFu) << 8));
            finalInput = (curr & ~conflict) | (activePriority & conflict);
        }

        const uint8_t lutResult = MoveLUT[finalInput & 0xF];
        uint16_t mask;
        if constexpr (kInputMaskReset) {
            mask = 0xFF0Fu | (static_cast<uint16_t>(lutResult) & 0x00F0u);
        }
        else {
            mask = (m_inputMaskFast & 0xFF0Fu) | (static_cast<uint16_t>(lutResult) & 0x00F0u);
        }

        // --- Branchless button merge (B/L/R) ---
        constexpr uint16_t kModBits = (1u << INPUT_B) | (1u << INPUT_L) | (1u << INPUT_R);
        const uint64_t nd = ~m_input.down;
        const uint16_t bBit = static_cast<uint16_t>(((nd >> 0) & 1u) << INPUT_B);
        const uint16_t lBit = static_cast<uint16_t>(((nd >> 1) & 1u) << INPUT_L);
        const uint16_t rBit = static_cast<uint16_t>(((nd >> 2) & 1u) << INPUT_R);
        m_inputMaskFast = (mask & ~kModBits) | bBit | lBit | rBit;
    }

    HOT_FUNCTION void MelonPrimeCore::ProcessMoveAndButtonsFast()
    {
        ProcessMoveAndButtonsFastImpl<false>();
    }

    HOT_FUNCTION void MelonPrimeCore::ProcessMoveAndButtonsFastFromReset()
    {
        ProcessMoveAndButtonsFastImpl<true>();
    }

    void MelonPrimeCore::ProcessAimInputStylus(melonDS::NDS* nds)
    {
        if (LIKELY(emuInstance->isTouching)) {
            nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
        }
        else {
            nds->ReleaseScreen();
        }
    }

    // P-29b: Cold path for aim reset (aimBlock or layout change).
    // Outlined from ProcessAimInputMouse to keep the hot path branch-free.
    COLD_FUNCTION void MelonPrimeCore::HandleAimEarlyReset()
    {
        if (m_aimResidualX | m_aimResidualY) {
            m_aimResidualX = 0;
            m_aimResidualY = 0;
        }
        m_nativeAimDeltaX = 0;
        m_nativeAimDeltaY = 0;

        if (m_isLayoutChangePending) {
            m_isLayoutChangePending = false;
#ifdef _WIN32
            if (m_rawFilter) m_rawFilter->discardDeltas();
#endif
        }
    }

    [[nodiscard]] static FORCE_INLINE int64_t ClampAimResidual(
        const int64_t value, const int64_t limit) noexcept
    {
        if (LIKELY(value <= limit && value >= -limit))
            return value;
        return (value < 0) ? -limit : limit;
    }

    // =========================================================================
    // ApplyAim - branchless deadzone/snap helper for the legacy aim path.
    //
    // Input:  raw     Q14 fixed-point residual for one axis
    //         adjT    deadzone threshold (Q14)
    //         snapT   snap threshold (Q14); 0 disables snap
    //
    // Output: Q14→int16 reduced value (consumed portion)
    //
    // All branches are replaced with arithmetic using sign-extension tricks:
    //   isDeadzone / isSnap produce 0 or -1 masks via SAR 63.
    //   snapVal computes ±1 from the sign bit without a branch.
    //   normVal is the standard >> FracBits reduction.
    // The final OR selects the appropriate output for each region.
    // =========================================================================
    [[nodiscard]] static FORCE_INLINE int16_t ApplyAim(
        int64_t raw, int64_t adjT, int64_t snapT) noexcept
    {
        constexpr int kFracBits = 14; // AIM_FRAC_BITS
        const int64_t sign      = raw >> 63;
        const int64_t absRaw    = (raw ^ sign) - sign;
        const int64_t isDeadzone = (absRaw - adjT) >> 63;
        const int64_t isSnap     = (absRaw - snapT) >> 63;
        const int64_t snapVal    = (1LL ^ sign) - sign;
        const int64_t normVal    = raw >> kFracBits;
        return static_cast<int16_t>(
            (~isDeadzone & isSnap & snapVal) |
            (~isSnap     & normVal)
        );
    }

#include "MelonPrimePatchNativeAimDeltaHook.inc"

    // =========================================================================
    // ProcessAimInputMouse
    //
    // P-17: Sub-pixel residual accumulation.
    // P-18: Dual-path aim pipeline.
    //
    //   Direct path (m_disableMphAimSmoothing = true, ASM patch active):
    //     - No deadzone (DS-side also bypassed by ASM patch)
    //     - >> 12 direct output (4x resolution vs >> 14 << 2)
    //     - Every frame with mouse movement produces output
    //     - ~8 instructions (SAR ×2 + SUB ×2 + test + store ×2)
    //
    //   Legacy path (m_disableMphAimSmoothing = false):
    //     - Deadzone/snap for DS-side compatibility
    //     - P-17 residual accumulation with ApplyAim branchless logic
    //     - ampShift = 0 (DS handles amplification internally)
    // =========================================================================
    HOT_FUNCTION void MelonPrimeCore::ProcessAimInputMouse()
    {
        m_nativeAimDeltaX = 0;
        m_nativeAimDeltaY = 0;

        // P-29b: Combined early-exit gate.
        // Single branch covers both aimBlock (morph/weapon) and layout change.
        // Cold path handles the specifics.
        if (UNLIKELY(m_aimBlockBits | static_cast<uint32_t>(m_isLayoutChangePending))) {
            HandleAimEarlyReset();
            return;
        }

        if (LIKELY(m_flags.test(StateFlags::BIT_LAST_FOCUSED))) {
            const int32_t deltaX = m_input.mouseX;
            const int32_t deltaY = m_input.mouseY;
            const bool hasDelta = (deltaX | deltaY) != 0;
            int64_t resX = m_aimResidualX;
            int64_t resY = m_aimResidualY;

            // P-44: Skip IMUL + clamp when mouse is completely at rest.
            // With 8kHz mouse this is rarely zero, but at standard poll rates
            // or when idling, it saves 2 IMUL (~6 cyc) + 2 clamp (~4 cyc).
            // The direct path's (outX|outY)==0 exit handles the "no output" case,
            // but skipping accumulation avoids touching the residual accumulators.
            if (LIKELY(hasDelta)) {
                // P-17: Accumulate into residual (Q14 fixed-point).
                resX += static_cast<int64_t>(deltaX) * m_aimFixedScaleX;
                resY += static_cast<int64_t>(deltaY) * m_aimFixedScaleY;

                // P-29a: Clamp only when residual escapes the normal range.
                resX = ClampAimResidual(resX, AIM_MAX_RESIDUAL);
                resY = ClampAimResidual(resY, AIM_MAX_RESIDUAL);
            } else if ((resX | resY) == 0) {
                // No delta AND no residual → nothing to output. Skip everything.
                // Note: not LIKELY — with accumulator enabled, residuals persist after
                //       mouse stops, so zero-residual is not the dominant case here.
                return;
            }

            if (m_disableMphAimSmoothing) {
                // =========================================================
                // P-18a+b: Direct path (ASM patch enabled)
                //
                // >> 12 = >> 14 then << 2, but in one operation.
                // This preserves 2 extra fractional bits that >> 14 discards,
                // giving 4x finer resolution (minimum output ±1 vs ±4).
                //
                // No deadzone: mouse raw input has zero noise at rest
                // (delta=0 → residual unchanged → output 0).
                // DS-side deadzone is also bypassed by the ASM patch.
                // =========================================================
                const int16_t outX = static_cast<int16_t>(resX >> AIM_DIRECT_BITS);
                const int16_t outY = static_cast<int16_t>(resY >> AIM_DIRECT_BITS);

                // Subtract consumed integer portion; fractional remainder carries over.
                resX -= static_cast<int64_t>(outX) << AIM_DIRECT_BITS;
                resY -= static_cast<int64_t>(outY) << AIM_DIRECT_BITS;

                if ((outX | outY) == 0) {
                    if (hasDelta) {
                        m_aimResidualX = resX;
                        m_aimResidualY = resY;
                    }
                    return;
                }

                if (m_enableNativeAimDeltaHook) {
                    m_nativeAimDeltaX = outX;
                    m_nativeAimDeltaY = outY;
                }
                else {
                    // Direct write fallback — no << ampShift needed.
                    // >> 12 already produces the same scale as the old >> 14 << 2.
                    *m_ptrs.aimX = static_cast<uint16_t>(outX);
                    *m_ptrs.aimY = static_cast<uint16_t>(outY);
                }
            }
            else {
                // =========================================================
                // Legacy path (DS-side smoothing active)
                //
                // Deadzone + snap + P-17 residual accumulation.
                // ampShift = 0 (DS handles amplification internally).
                // =========================================================

                // Early exit if both residuals are in deadzone.
                {
                    const int64_t absResX = resX < 0 ? -resX : resX;
                    const int64_t absResY = resY < 0 ? -resY : resY;
                    if (absResX < m_aimFixedAdjust && absResY < m_aimFixedAdjust) {
                        if (hasDelta) {
                            m_aimResidualX = resX;
                            m_aimResidualY = resY;
                        }
                        return;
                    }
                }

                const int64_t adjT  = m_aimFixedAdjust;
                const int64_t snapT = m_aimFixedSnapThresh;

                const int16_t outX = ApplyAim(resX, adjT, snapT);
                const int16_t outY = ApplyAim(resY, adjT, snapT);

                resX -= static_cast<int64_t>(outX) << AIM_FRAC_BITS;
                resY -= static_cast<int64_t>(outY) << AIM_FRAC_BITS;

                if ((outX | outY) == 0) {
                    if (hasDelta) {
                        m_aimResidualX = resX;
                        m_aimResidualY = resY;
                    }
                    return;
                }

                *m_ptrs.aimX = static_cast<uint16_t>(outX);
                *m_ptrs.aimY = static_cast<uint16_t>(outY);
            }

            // Discard sub-pixel residuals when accumulator is off.
            // Fractional remainder won't carry to the next frame.
            if (!m_enableAimAccumulator) {
                resX = 0;
                resY = 0;
            }
            m_aimResidualX = resX;
            m_aimResidualY = resY;

#if !defined(_WIN32)
            QCursor::setPos(m_aimData.centerX, m_aimData.centerY);
#endif
            return;
        }

#if !defined(_WIN32)
        const QPoint center = GetAdjustedCenter();
        m_aimData.centerX = center.x();
        m_aimData.centerY = center.y();
        QCursor::setPos(center);
#endif
        m_isLayoutChangePending = false;
        m_aimResidualX = 0;
        m_aimResidualY = 0;
        m_nativeAimDeltaX = 0;
        m_nativeAimDeltaY = 0;
    }

} // namespace MelonPrime
