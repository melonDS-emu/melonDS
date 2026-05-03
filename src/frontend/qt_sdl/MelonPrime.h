#ifndef MELONPRIME_H
#define MELONPRIME_H

#include <functional>
#include <vector>
#include <cstdint>
#include <array>
#include <string_view>
#include <memory>
#include <cmath>
#include <cstring>
#include <atomic>

class QPoint;
class ScreenPanel;  // P-3: forward decl for cached panel pointer

#include "types.h"
#include "Config.h"
#include "MelonPrimeCompilerHints.h"  // Centralised macros (was inline here)
#include "MelonPrimeGameSettings.h"
#include "MelonPrimeGameRomAddrTable.h"
#ifdef MELONPRIME_DS
#include "MelonPrimePatchShadowFreezeRuntimeHook.h"
#endif

class EmuInstance;
namespace melonDS { class NDS; }

namespace MelonPrime {

#ifdef _WIN32
    class RawInputWinFilter;
#endif

    enum InputCacheBit : uint64_t {
        IB_JUMP = 1ULL << 0,
        IB_SHOOT = 1ULL << 1,
        IB_ZOOM = 1ULL << 2,
        IB_MORPH = 1ULL << 3,
        IB_MORPH_BOOST = 1ULL << 4,
        IB_WEAPON_CHECK = 1ULL << 5,
        IB_MOVE_F = 1ULL << 6,
        IB_MOVE_B = 1ULL << 7,
        IB_MOVE_L = 1ULL << 8,
        IB_MOVE_R = 1ULL << 9,
        IB_MENU = 1ULL << 10,
        IB_SCAN_VISOR = 1ULL << 11,
        IB_UI_OK = 1ULL << 12,
        IB_UI_LEFT = 1ULL << 13,
        IB_UI_RIGHT = 1ULL << 14,
        IB_UI_YES = 1ULL << 15,
        IB_UI_NO = 1ULL << 16,
        IB_WEAPON_BEAM = 1ULL << 17,
        IB_WEAPON_MISSILE = 1ULL << 18,
        IB_WEAPON_1 = 1ULL << 19,
        IB_WEAPON_2 = 1ULL << 20,
        IB_WEAPON_3 = 1ULL << 21,
        IB_WEAPON_4 = 1ULL << 22,
        IB_WEAPON_5 = 1ULL << 23,
        IB_WEAPON_6 = 1ULL << 24,
        IB_WEAPON_SPECIAL = 1ULL << 25,
        IB_WEAPON_NEXT = 1ULL << 26,
        IB_WEAPON_PREV = 1ULL << 27,

        IB_MOVE_MASK = IB_MOVE_F | IB_MOVE_B | IB_MOVE_L | IB_MOVE_R,
        IB_WEAPON_ANY = IB_WEAPON_BEAM | IB_WEAPON_MISSILE |
        IB_WEAPON_1 | IB_WEAPON_2 | IB_WEAPON_3 |
        IB_WEAPON_4 | IB_WEAPON_5 | IB_WEAPON_6 |
        IB_WEAPON_SPECIAL,
        IB_UI_ANY = IB_UI_OK | IB_UI_LEFT | IB_UI_RIGHT | IB_UI_YES | IB_UI_NO,
    };

    struct alignas(64) FrameInputState {
        uint64_t down;
        uint64_t press;
        int32_t  mouseX;
        int32_t  mouseY;
        int32_t  wheelDelta;
        uint32_t moveIndex;
        uint32_t _pad[2];
    };
    static_assert(sizeof(FrameInputState) == 64);

    struct alignas(64) GameAddressesHot {
        melonDS::u32 isAltForm;
        melonDS::u32 jumpFlag;
        melonDS::u32 weaponChange;
        melonDS::u32 selectedWeapon;
        melonDS::u32 currentWeapon;
        melonDS::u32 aimX;
        melonDS::u32 aimY;
        melonDS::u32 havingWeapons;
        melonDS::u32 weaponAmmo;
        melonDS::u32 boostGauge;
        melonDS::u32 isBoosting;
        melonDS::u32 loadedSpecialWeapon;
        melonDS::u32 isInVisorOrMap;
        melonDS::u32 isMapOrUserActionPaused;
        melonDS::u32 inGame;
        melonDS::u32 chosenHunter;
        melonDS::u32 inGameSensi;
    };

    // =========================================================================
    // HotPointers: RAM pointers resolved once per game-join.
    // Tiered by access frequency for optimal cache-line packing.
    // =========================================================================
    struct alignas(64) HotPointers {
        // [Tier 0: Every Frame (Always) - frame gate check]
        uint16_t* inGame;

        // [Tier 1: Every Frame (Always)]
        uint16_t* aimX;
        uint16_t* aimY;
        uint8_t* isAltForm;
        uint8_t* jumpFlag;
        uint32_t* weaponAmmo;

        // [Tier 2: Every Frame (Often)]
        uint16_t* havingWeapons;
        uint8_t* currentWeapon;
        uint8_t* weaponChange;

        // [Tier 3: Conditional / Rare]
        uint8_t* selectedWeapon;
        uint8_t* loadedSpecialWeapon;
        uint8_t* boostGauge;
        uint8_t* isBoosting;
        uint8_t* isInVisorOrMap;
        uint8_t* isMapOrUserActionPaused;
    };

    enum AimBlockBit : uint32_t {
        AIMBLK_CHECK_WEAPON = 1u << 0,
        AIMBLK_MORPHBALL_BOOST = 1u << 1,
        AIMBLK_CURSOR_MODE = 1u << 2,
        AIMBLK_NOT_IN_GAME = 1u << 3,
    };


#ifdef _WIN32
    struct FilterDeleter {
        void operator()(RawInputWinFilter* ptr);
    };
#endif

    class MelonPrimeCore
    {
    public:
        explicit MelonPrimeCore(EmuInstance* instance);
        ~MelonPrimeCore();

        MelonPrimeCore(const MelonPrimeCore&) = delete;
        MelonPrimeCore& operator=(const MelonPrimeCore&) = delete;

        void Initialize();
        HOT_FUNCTION void RunFrameHook();
        void OnEmuStart();
        void OnEmuStop();
        void OnEmuPause();
        void OnEmuUnpause();
        void OnReset();

        void SetFrameAdvanceFunc(std::function<void()> func);

        [[nodiscard]] FORCE_INLINE bool IsInGame() const { return m_flags.test(StateFlags::BIT_IN_GAME); }
        [[nodiscard]] bool ShouldForceSoftwareRenderer() const;
        [[nodiscard]] uint16_t GetInputMaskFast() const { return m_inputMaskFast; }

#ifdef MELONPRIME_DS
        static uint32_t NativeAimDeltaHook_GetAddresses(
            uint8_t romGroupIndex,
            uint32_t* out,
            uint32_t maxCount);
        void NativeAimDeltaHook_DispatchCheck(
            melonDS::NDS* nds,
            uint32_t arm9ExecAddr,
            uint32_t regs[16]);
#endif

#ifdef MELONPRIME_CUSTOM_HUD
        [[nodiscard]] const RomAddresses& GetCurrentRom() const { return m_currentRom; }
        [[nodiscard]] const GameAddressesHot& GetAddrHot() const { return m_addrHot; }
        [[nodiscard]] uint8_t GetPlayerPosition() const { return m_playerPosition; }
        [[nodiscard]] uint8_t GetHunterID() const { return m_hunterID; }
        [[nodiscard]] bool IsRomDetected() const { return m_flags.test(StateFlags::BIT_ROM_DETECTED); }
#endif

        bool isCursorMode = true;
        std::atomic_bool isFocused{ false };
        bool isClipWanted = false;
        bool isStylusMode = false;
        bool m_snapTapMode = false;     // Cached from BIT_SNAP_TAP; avoids bitmask test in hot path
        bool isFastForward = false;     // Set by EmuThread; Screen Sync skips when true
        int  screenSyncMode = 0;       // 0=Off, 1=glFinish, 2=DwmFlush

        void NotifyLayoutChange();  // P-3: impl in .cpp (needs complete EmuInstance type)

        // P-14/P-33: PrePollRawInput retired. P-19 (HiddenWndProc) captures
        // all WM_INPUT at dispatch time, making pre-drain unnecessary.
        // Kept as empty inline for source compatibility; remove in future cleanup.
        void PrePollRawInput() {}

        // P-22: Drain WM_INPUT queue after RunFrame.
        void DeferredDrainInput();

    private:
        // =================================================================
        // Cache-optimized member layout
        //   CL0:  m_input         (R/W every frame, 64B)
        //   CL1+: m_ptrs          (R every frame)
        //   Hot scalars + emuInstance (R/W every frame, same CL cluster)
        //   Warm: fnAdvance, frameAdvanceFunc, rawFilter
        //   Cold: m_addrHot, m_currentRom (init-only, pushed to end)
        // =================================================================

        // --- CL0: Input State (R/W every frame) ---
        alignas(64) FrameInputState m_input{};

        // --- CL1+: RAM Pointers (R every frame) ---
        alignas(64) HotPointers m_ptrs{};

        // --- Hot Scalars + Core Pointers (R/W every frame) ---
        EmuInstance* emuInstance;
        ScreenPanel* m_cachedPanel = nullptr;  // P-3: cached to avoid 3-level pointer chase
        Config::Table& localCfg;
        Config::Table& globalCfg;

        uint16_t m_inputMaskFast = 0xFFFF;
        uint16_t m_snapState = 0;
        uint32_t m_aimBlockBits = 0;

        // =============================================================
        // Fixed-point aim pipeline (Q14 = 14-bit fractional).
        // Pure integer path replaces float: IMUL x2 + SAR x2 vs CVTSI2SS x2 + MULSS x2 + CVTTSS2SI x2
        // =============================================================
        static constexpr int AIM_FRAC_BITS = 14;
        static constexpr int64_t AIM_ONE_FP = 1LL << AIM_FRAC_BITS;

        // P-18: Direct path uses >> 12 instead of >> 14 then << 2.
        // This preserves 2 extra fractional bits, giving 4x resolution (±1 vs ±4).
        static constexpr int AIM_DIRECT_BITS = AIM_FRAC_BITS - 2;  // 12

        // P-18c: Residual clamp prevents wind-up during rapid direction changes.
        // 128 DS-units at Q14 = a large but bounded rotation.
        static constexpr int64_t AIM_MAX_RESIDUAL = 128LL << AIM_FRAC_BITS;

        int32_t  m_aimFixedScaleX = 164;
        int32_t  m_aimFixedScaleY = 218;
        int64_t  m_aimFixedAdjust = 8192;
        int64_t  m_aimFixedSnapThresh = AIM_ONE_FP;

        // P-17: Sub-pixel residual accumulators (Q14 fixed-point).
        // Carry fractional remainder across frames for smooth slow-speed aiming.
        int64_t  m_aimResidualX = 0;
        int64_t  m_aimResidualY = 0;

        // P-30: Hot bools grouped with residuals (same cache line fetch).
        // ProcessAimInputMouse reads these every frame right after residuals.
        bool     m_disableMphAimSmoothing = false;
        bool     m_enableAimAccumulator = false;
        bool     m_enableNativeAimDeltaHook = false;
        int16_t  m_nativeAimDeltaX = 0;
        int16_t  m_nativeAimDeltaY = 0;

        // Warm scalars (checked per frame but not in aim hot path)
        bool     m_isRunningHook = false;
        bool     m_isWeaponCheckActive = false;
        bool     m_isLayoutChangePending = true;
        // P-47: Set by FrameAdvanceOnce; cleared after PollAndSnapshot.
        // True  → LateLatch must call processRawInputBatched (events may have
        //          arrived during the FrameAdvance window: ~32–96 ms).
        // False → PollAndSnapshot was just called; kernel buffer is still
        //          empty on a normal frame (~40–100 ns window). Skip the
        //          GetRawInputBuffer syscall entirely (~500–2000 cyc saved).
        bool     m_didFrameAdvanceSinceSnapshot = false;

        // Cold: float intermediates (config change only)
        float    m_aimSensiFactor = 0.01f;
        float    m_aimCombinedY = 0.013333333f;
        float    m_aimAdjust = 0.5f;

        struct alignas(4) StateFlags {
            uint32_t packed = 0;
            static constexpr uint32_t BIT_ROM_DETECTED = 1u << 0;
            static constexpr uint32_t BIT_IN_GAME = 1u << 1;
            static constexpr uint32_t BIT_IN_GAME_INIT = 1u << 2;
            static constexpr uint32_t BIT_PAUSED = 1u << 3;
            static constexpr uint32_t BIT_IN_ADVENTURE = 1u << 4;
            static constexpr uint32_t BIT_WAS_IN_GAME_RENDERER = 1u << 5;
            static constexpr uint32_t BIT_IS_SAMUS = 1u << 6;
            static constexpr uint32_t BIT_IS_WEAVEL = 1u << 7;
            static constexpr uint32_t BIT_IS_ALT_FORM = 1u << 8;
            static constexpr uint32_t BIT_SNAP_TAP = 1u << 9;
            static constexpr uint32_t BIT_JOY2KEY = 1u << 10;
            static constexpr uint32_t BIT_STYLUS_MODE = 1u << 11;
            static constexpr uint32_t BIT_LAST_FOCUSED = 1u << 13;
            static constexpr uint32_t BIT_BLOCK_STYLUS = 1u << 14;

            FORCE_INLINE void set(uint32_t bit) { packed |= bit; }
            FORCE_INLINE void clear(uint32_t bit) { packed &= ~bit; }
            FORCE_INLINE void assign(uint32_t bit, bool val) { packed = (packed & ~bit) | (val ? bit : 0u); }
            [[nodiscard]] FORCE_INLINE bool test(uint32_t bit) const { return (packed & bit) != 0; }
        } m_flags{};

        // --- Warm: Frame advance + platform ---
        std::function<void()> m_frameAdvanceFunc;
        using AdvanceMethod = void (MelonPrimeCore::*)();
        AdvanceMethod m_fnAdvance = &MelonPrimeCore::FrameAdvanceDefault;

#ifdef _WIN32
        std::unique_ptr<RawInputWinFilter, FilterDeleter> m_rawFilter;
        void* m_cachedHwnd = nullptr;
        bool  m_isNativeFilterInstalled = false;  // Replaced static local for thread safety
#endif

        struct AimData {
            int centerX = 0;
            int centerY = 0;
        } m_aimData;

        // --- Cold: Init-only data (pushed to end) ---
        GameAddressesHot m_addrHot{};
        RomAddresses m_currentRom{};
        melonDS::u8  m_playerPosition = 0;
        uint8_t      m_hunterID = 0;

        // =================================================================
        // Inline helpers
        // =================================================================
        // InputReset: clears DS input mask for fresh per-frame rebuild.
        // Called every frame at the top of RunFrameHook.
        //
        // NOTE: m_aimResidualX/Y must NOT be zeroed here!
        // Residuals accumulate across frames (P-17/P-18). Zeroing them here
        // would defeat sub-pixel accumulation entirely — every frame would
        // start from 0, making P-17/P-18 non-functional.
        //
        // Residuals are only zeroed on actual state resets:
        //   - OnEmuStart/InputReset → OnEmuStart calls its own reset
        //   - RecalcAimFixedPoint  → sensitivity change invalidates old residuals
        //   - Layout change        → coordinate system reset
        //   - Aim block / focus loss → stale residuals would cause jump
        FORCE_INLINE void InputReset() {
            m_inputMaskFast = 0xFFFF;
        }

        FORCE_INLINE void InputSetBranchless(uint16_t bit, bool released) {
            const uint16_t mask = 1u << bit;
            m_inputMaskFast = (m_inputMaskFast & ~mask) | (static_cast<uint16_t>(released) * mask);
        }

        FORCE_INLINE void SetAimBlockBranchless(uint32_t bitMask, bool enable) noexcept {
            m_aimBlockBits = (m_aimBlockBits & ~bitMask) | (enable ? bitMask : 0u);
        }

        [[nodiscard]] FORCE_INLINE bool IsSamusMorphBallAltForm() const noexcept {
            return m_flags.test(StateFlags::BIT_IS_SAMUS)
                && m_ptrs.isAltForm
                && *m_ptrs.isAltForm == 0x02;
        }

        template <typename T>
        [[nodiscard]] FORCE_INLINE T* GetRamPointer(melonDS::u8* ram, melonDS::u32 addr) {
            return reinterpret_cast<T*>(&ram[addr & 0x3FFFFF]);
        }

        [[nodiscard]] FORCE_INLINE bool IsDown(uint64_t bit) const { return (m_input.down & bit) != 0; }
        [[nodiscard]] FORCE_INLINE bool IsPressed(uint64_t bit) const { return (m_input.press & bit) != 0; }
        [[nodiscard]] FORCE_INLINE bool IsAnyPressed(uint64_t mask) const { return (m_input.press & mask) != 0; }

        // =================================================================
        // Methods
        // =================================================================
        HOT_FUNCTION void UpdateInputState(bool focused);
        HOT_FUNCTION void UpdateInputStateReentrant(bool focused);  // re-entrant FrameAdvance path
        template <bool kReentrant> FORCE_INLINE void UpdateInputStateImpl(bool focused);
        HOT_FUNCTION void HandleInGameLogic();
        template <bool kInputMaskReset> FORCE_INLINE void ProcessMoveAndButtonsFastImpl();
        HOT_FUNCTION void ProcessMoveAndButtonsFast();
        HOT_FUNCTION void ProcessMoveAndButtonsFastFromReset();
        HOT_FUNCTION void ProcessAimInputMouse();
        HOT_FUNCTION bool ProcessWeaponSwitch();
        HOT_FUNCTION bool HandleMorphBallBoost();

        // --- Cold Path Handlers (Outlined) ---
        COLD_FUNCTION void HandleRareMorph();
        COLD_FUNCTION void HandleRareWeaponSwitch();
        COLD_FUNCTION void HandleRareWeaponCheckStart();
        COLD_FUNCTION void HandleRareWeaponCheckEnd();
        COLD_FUNCTION void HandleAimEarlyReset();  // P-29b
        COLD_FUNCTION void HandleAdventureMode();

        COLD_FUNCTION void HandleGameJoinInit();
        COLD_FUNCTION void DetectRomAndSetAddresses();
        COLD_FUNCTION void ApplyGameSettingsOnce();

        void RecalcAimSensitivityCache(Config::Table& cfg);
        void ApplyAimAdjustSetting(Config::Table& cfg);
        void RecalcAimFixedPoint();
        FORCE_INLINE void HandleGlobalHotkeys();
        void ProcessAimInputStylus(melonDS::NDS* nds);
        void SwitchWeapon(int weaponIndex);
        void ShowCursor(bool show);
        void FrameAdvanceTwice();
        FORCE_INLINE void FrameAdvanceOnce() { m_didFrameAdvanceSinceSnapshot = true; (this->*m_fnAdvance)(); }
        void FrameAdvanceDefault();
        void FrameAdvanceCustom();
        QPoint GetAdjustedCenter();

        void SetupRawInput();
        void ApplyJoy2KeySupportAndQtFilter(bool enable, bool doReset = true);
        void ReloadConfigFlags();
    };

} // namespace MelonPrime

#endif // MELONPRIME_H
