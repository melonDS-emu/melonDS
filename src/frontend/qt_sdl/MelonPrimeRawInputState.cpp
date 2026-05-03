#ifdef _WIN32
#include "MelonPrimeRawInputState.h"
#include "MelonPrimeRawWinInternal.h"
#include <cstring>
#include <algorithm>
#include <memory>
#include <new>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifndef QWORD
typedef unsigned __int64 QWORD;
#endif

namespace MelonPrime {

    std::array<InputState::BtnLutEntry, 1024> InputState::s_btnLut;
    std::array<InputState::VkRemapEntry, 256> InputState::s_vkRemap;
    std::array<uint16_t, 512> InputState::s_makeCodeLut;
    uint16_t InputState::s_scancodeLShift = 0;
    uint16_t InputState::s_scancodeRShift = 0;
    std::once_flag InputState::s_initFlag;
    NtUserGetRawInputBuffer_t InputState::s_fnBestGetRawInputBuffer = ::GetRawInputBuffer;

    InputState::InputState() noexcept {
        for (auto& vk : m_vkDown)
            vk.store(0, std::memory_order_relaxed);
        m_mouseButtons.store(0, std::memory_order_relaxed);
        m_mouseButtonPresses.store(0, std::memory_order_relaxed);

        m_accumMouseX.store(0, std::memory_order_relaxed);
        m_accumMouseY.store(0, std::memory_order_relaxed);
        m_lastReadMouseX = 0;
        m_lastReadMouseY = 0;

        std::memset(&m_hkMasks, 0, sizeof(HotkeyMasks));
        m_hkPrev = 0;
        m_boundHotkeys = 0;
    }

    void InputState::InitializeTables() noexcept {
        std::call_once(s_initFlag, []() {
            WinInternal::ResolveNtApis();
            if (WinInternal::fnNtUserGetRawInputBuffer) {
                s_fnBestGetRawInputBuffer = WinInternal::fnNtUserGetRawInputBuffer;
            }

            for (int i = 0; i < 1024; ++i) {
                uint8_t d = 0, u = 0;
                if (i & RI_MOUSE_BUTTON_1_DOWN) d |= 0x01;
                if (i & RI_MOUSE_BUTTON_1_UP)   u |= 0x01;
                if (i & RI_MOUSE_BUTTON_2_DOWN) d |= 0x02;
                if (i & RI_MOUSE_BUTTON_2_UP)   u |= 0x02;
                if (i & RI_MOUSE_BUTTON_3_DOWN) d |= 0x04;
                if (i & RI_MOUSE_BUTTON_3_UP)   u |= 0x04;
                if (i & RI_MOUSE_BUTTON_4_DOWN) d |= 0x08;
                if (i & RI_MOUSE_BUTTON_4_UP)   u |= 0x08;
                if (i & RI_MOUSE_BUTTON_5_DOWN) d |= 0x10;
                if (i & RI_MOUSE_BUTTON_5_UP)   u |= 0x10;
                s_btnLut[i] = { d, u };
            }

            s_vkRemap.fill(VkRemapEntry{ 0, 0 });
            auto setRemap = [](int base, int l, int r) {
                s_vkRemap[base] = { static_cast<uint8_t>(l), static_cast<uint8_t>(r) };
                };
            setRemap(VK_CONTROL, VK_LCONTROL, VK_RCONTROL);
            setRemap(VK_MENU, VK_LMENU, VK_RMENU);
            setRemap(VK_SHIFT, VK_LSHIFT, VK_RSHIFT);

            s_scancodeLShift = static_cast<uint16_t>(MapVirtualKeyW(VK_LSHIFT, MAPVK_VK_TO_VSC));
            s_scancodeRShift = static_cast<uint16_t>(MapVirtualKeyW(VK_RSHIFT, MAPVK_VK_TO_VSC));

            s_makeCodeLut.fill(0);
            for (UINT i = 1; i < 512; ++i) {
                s_makeCodeLut[i] = static_cast<uint16_t>(MapVirtualKeyW(i, MAPVK_VSC_TO_VK_EX));
            }
            });
    }

    // =========================================================================
    // R3: processRawInput — single-writer mouse accumulation
    //
    // HiddenWndProc / nativeEventFilter act as the sole writer in each mode, so
    // we keep the original load(relaxed)+store(release) pattern here. This avoids
    // `lock xadd` on x86 while preserving release ordering for the consumer.
    // =========================================================================
    void InputState::processRawInput(HRAWINPUT hRaw) noexcept {
        alignas(16) uint8_t rawBuf[sizeof(RAWINPUT)];
        UINT size = sizeof(rawBuf);
        UINT result;

        if (LIKELY(WinInternal::fnNtUserGetRawInputData != nullptr)) {
            result = WinInternal::fnNtUserGetRawInputData(hRaw, RID_INPUT, rawBuf, &size, sizeof(RAWINPUTHEADER));
        }
        else {
            result = GetRawInputData(hRaw, RID_INPUT, rawBuf, &size, sizeof(RAWINPUTHEADER));
        }

        if (UNLIKELY(result == UINT(-1) || result == 0)) return;
        const auto* raw = reinterpret_cast<const RAWINPUT*>(rawBuf);

        switch (raw->header.dwType) {
        case RIM_TYPEMOUSE: {
            const RAWMOUSE& m = raw->data.mouse;
            if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                if (m.lLastX) {
                    const int64_t curX = m_accumMouseX.load(std::memory_order_relaxed);
                    m_accumMouseX.store(curX + m.lLastX, std::memory_order_release);
                }
                if (m.lLastY) {
                    const int64_t curY = m_accumMouseY.load(std::memory_order_relaxed);
                    m_accumMouseY.store(curY + m.lLastY, std::memory_order_release);
                }
            }
            const USHORT flags = m.usButtonFlags & 0x03FF;
            if (flags) {
                const auto& lut = s_btnLut[flags];
                if (lut.downBits | lut.upBits) {
                    const uint8_t cur = m_mouseButtons.load(std::memory_order_relaxed);
                    // UP without a known DOWN is still evidence of a short click.
                    const uint8_t pressBits = lut.downBits | (lut.upBits & ~cur);
                    m_mouseButtons.store(
                        (cur | lut.downBits) & ~lut.upBits,
                        std::memory_order_release);
                    if (pressBits) {
                        m_mouseButtonPresses.fetch_or(pressBits, std::memory_order_release);
                    }
                }
            }
            break;
        }
        case RIM_TYPEKEYBOARD: {
            const RAWKEYBOARD& kb = raw->data.keyboard;
            UINT vk = kb.VKey;

            if (UNLIKELY(vk == 0)) {
                vk = LIKELY(kb.MakeCode < 512) ? s_makeCodeLut[kb.MakeCode]
                    : MapVirtualKeyW(kb.MakeCode, MAPVK_VSC_TO_VK_EX);
            }

            if (LIKELY(vk > 0 && vk < 256)) {
                vk = remapVk(vk, kb.MakeCode, kb.Flags);
                setVkBit(vk, !(kb.Flags & RI_KEY_BREAK));
                std::atomic_thread_fence(std::memory_order_release);
            }
            break;
        }
        }
    }

    // =========================================================================
    // R2 BUG FIX: processRawInputBatched — button precedence consistency
    //
    // Previously: finalBtnState = (finalBtnState & ~lut.upBits) | lut.downBits
    //   -> DOWN wins when both DOWN and UP are set for the same button
    //
    // Fixed:      finalBtnState = (finalBtnState | lut.downBits) & ~lut.upBits
    //   -> UP wins (consistent with processRawInput single-event path)
    //
    // When a RAWINPUT message has both BUTTON_DOWN and BUTTON_UP for the same
    // button (coalesced input), the final state should be "released" (UP wins).
    // This matches the convention in processRawInput and Windows input handling.
    // =========================================================================
    void InputState::processRawInputBatched() noexcept {
        alignas(64) static uint8_t buffer[16384];

        int64_t localAccX = 0, localAccY = 0;
        uint64_t localKeyDeltaDown[4] = {};
        uint64_t localKeyDeltaUp[4] = {};
        bool hasKeyChanges = false;

        const uint8_t initialBtnState = m_mouseButtons.load(std::memory_order_relaxed);
        uint8_t finalBtnState = initialBtnState;
        uint8_t localBtnPresses = 0;

        for (;;) {
            UINT size = sizeof(buffer);
            uint8_t* batchBuffer = buffer;
            UINT count = s_fnBestGetRawInputBuffer(
                reinterpret_cast<PRAWINPUT>(buffer), &size, sizeof(RAWINPUTHEADER));
            std::unique_ptr<uint8_t[]> overflowBuffer;
            if (UNLIKELY(count == UINT(-1))) {
                if (size <= sizeof(buffer)) break;

                overflowBuffer.reset(new (std::nothrow) uint8_t[size]);
                if (!overflowBuffer) break;

                batchBuffer = overflowBuffer.get();
                UINT retrySize = size;
                count = s_fnBestGetRawInputBuffer(
                    reinterpret_cast<PRAWINPUT>(batchBuffer), &retrySize, sizeof(RAWINPUTHEADER));
            }
            if (count == 0 || count == UINT(-1)) break;

            const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(batchBuffer);
            for (UINT i = 0; i < count; ++i) {
                // P-6: LIKELY -- 8000Hz mouse generates ~133 events/frame vs 0-2 keyboard
                if (LIKELY(raw->header.dwType == RIM_TYPEMOUSE)) {
                    const RAWMOUSE& m = raw->data.mouse;
                    if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                        localAccX += m.lLastX;
                        localAccY += m.lLastY;
                    }
                    const USHORT flags = m.usButtonFlags & 0x03FF;
                    if (flags) {
                        const auto& lut = s_btnLut[flags];
                        // If only an UP arrives while our logical state is already up,
                        // the matching DOWN was probably consumed by the WM_INPUT drain
                        // race. Treat it as a one-frame click instead of dropping it.
                        localBtnPresses |= lut.downBits | (lut.upBits & ~finalBtnState);
                        // R2 FIX: UP wins (was: DOWN wins). See header comment.
                        finalBtnState = (finalBtnState | lut.downBits) & ~lut.upBits;
                    }
                }
                else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                    const RAWKEYBOARD& kb = raw->data.keyboard;
                    UINT vk = kb.VKey;

                    if (UNLIKELY(vk == 0)) {
                        vk = LIKELY(kb.MakeCode < 512) ? s_makeCodeLut[kb.MakeCode]
                            : MapVirtualKeyW(kb.MakeCode, MAPVK_VSC_TO_VK_EX);
                    }

                    if (LIKELY(vk > 0 && vk < 256)) {
                        vk = remapVk(vk, kb.MakeCode, kb.Flags);
                        const int idx = vk >> 6;
                        const uint64_t bit = 1ULL << (vk & 63);

                        const uint64_t isUpMask = (kb.Flags & RI_KEY_BREAK) ? ~0ULL : 0ULL;
                        const uint64_t maskDown = ~isUpMask & bit;
                        const uint64_t maskUp = isUpMask & bit;

                        localKeyDeltaDown[idx] = (localKeyDeltaDown[idx] & ~bit) | maskDown;
                        localKeyDeltaUp[idx] = (localKeyDeltaUp[idx] & ~bit) | maskUp;

                        hasKeyChanges = true;
                    }
                }
                raw = NEXTRAWINPUTBLOCK(raw);
            }

            // FIX-4: MSDN contract — DefRawInputProc after GetRawInputBuffer.
            // Without this, Windows may fail to retire internal buffer entries,
            // causing GetRawInputBuffer to return 0 on subsequent calls
            // → key events dropped → stuck keys / unresponsive keys.
            PRAWINPUT pri = reinterpret_cast<PRAWINPUT>(batchBuffer);
            DefRawInputProc(&pri, static_cast<INT>(count), sizeof(RAWINPUTHEADER));
        }

        // --- Commit phase (single-writer, wait-free) ---
        // P-37: Combined nonzero check reduces branch count.
        // With 8kHz mouse, (localAccX | localAccY) is nonzero on ~99% of frames.
        if (localAccX | localAccY) {
            m_accumMouseX.store(m_accumMouseX.load(std::memory_order_relaxed) + localAccX, std::memory_order_relaxed);
            m_accumMouseY.store(m_accumMouseY.load(std::memory_order_relaxed) + localAccY, std::memory_order_relaxed);
        }

        if (finalBtnState != initialBtnState) {
            m_mouseButtons.store(finalBtnState, std::memory_order_relaxed);
        }
        if (localBtnPresses) {
            m_mouseButtonPresses.fetch_or(localBtnPresses, std::memory_order_release);
        }

        if (hasKeyChanges) {
            for (int i = 0; i < 4; ++i) {
                if (localKeyDeltaDown[i] | localKeyDeltaUp[i]) {
                    const uint64_t cur = m_vkDown[i].load(std::memory_order_relaxed);
                    m_vkDown[i].store(
                        (cur | localKeyDeltaDown[i]) & ~localKeyDeltaUp[i],
                        std::memory_order_relaxed);
                }
            }
        }

        std::atomic_thread_fence(std::memory_order_release);
    }

    void InputState::fetchMouseDelta(int& outX, int& outY) noexcept {
        const int64_t curX = m_accumMouseX.load(std::memory_order_acquire);
        const int64_t curY = m_accumMouseY.load(std::memory_order_acquire);
        outX = static_cast<int>(curX - m_lastReadMouseX);
        outY = static_cast<int>(curY - m_lastReadMouseY);
        m_lastReadMouseX = curX;
        m_lastReadMouseY = curY;
    }

    void InputState::discardDeltas() noexcept {
        m_lastReadMouseX = m_accumMouseX.load(std::memory_order_relaxed);
        m_lastReadMouseY = m_accumMouseY.load(std::memory_order_relaxed);
    }

    void InputState::resetAllKeys() noexcept {
        for (auto& vk : m_vkDown) vk.store(0, std::memory_order_relaxed);
        m_mouseButtons.store(0, std::memory_order_relaxed);
        m_mouseButtonPresses.store(0, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        m_hkPrev = 0;
    }

    void InputState::resetMouseButtons() noexcept {
        m_mouseButtons.store(0, std::memory_order_release);
        m_mouseButtonPresses.store(0, std::memory_order_release);
    }

    // =========================================================================
    // P-9: resetAll — combined reset with single fence.
    //
    // Replaces separate resetAllKeys() + resetMouseButtons() calls.
    // resetAllKeys does 4 stores + fence + m_hkPrev=0.
    // resetMouseButtons does 1 store(release).
    // Combined: 5 stores(relaxed) + 1 fence. Saves one redundant fence.
    // =========================================================================
    void InputState::resetAll() noexcept {
        for (auto& vk : m_vkDown) vk.store(0, std::memory_order_relaxed);
        m_mouseButtons.store(0, std::memory_order_relaxed);
        m_mouseButtonPresses.store(0, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        m_hkPrev = 0;
    }

    // =========================================================================
    // R2: setHotkeyVks — pointer+count interface + mouse button LUT
    //
    // Changes:
    //   1. Primary interface now takes (const UINT*, size_t) to allow
    //      zero-allocation calls from SmallVkList (was std::vector only).
    //   2. Mouse button VK mapping uses constexpr LUT instead of switch.
    //   3. Removed hasMask[] write (hasMask eliminated in R2).
    // =========================================================================
    void InputState::setHotkeyVks(int id, const UINT* vks, size_t count) {
        if (UNLIKELY(id < 0 || static_cast<size_t>(id) >= kMaxHotkeyId)) return;

        std::memset(m_hkMasks.vkMask[id], 0, sizeof(uint64_t) * 4);
        m_hkMasks.mouseMask[id] = 0;

        const uint64_t bbit = 1ULL << id;

        if (count == 0) {
            m_boundHotkeys &= ~bbit;
            m_hkFastWord[id] = 5;
            return;
        }

        // R2: constexpr LUT replaces 5-case switch for mouse VK -> bit position.
        // VK_LBUTTON=1, VK_RBUTTON=2, VK_CANCEL=3, VK_MBUTTON=4, VK_XBUTTON1=5, VK_XBUTTON2=6
        // Mapping: 1->0, 2->1, 3->0xFF(skip), 4->2, 5->3, 6->4
        static constexpr uint8_t kMouseVkToBit[7] = { 0xFF, 0, 1, 0xFF, 2, 3, 4 };

        for (size_t i = 0; i < count; ++i) {
            const UINT vk = vks[i];
            if (vk >= VK_LBUTTON && vk <= VK_XBUTTON2) {
                const uint8_t bit = kMouseVkToBit[vk];
                if (bit != 0xFF)
                    m_hkMasks.mouseMask[id] |= static_cast<uint8_t>(1u << bit);
            }
            else if (vk < 256) {
                m_hkMasks.vkMask[id][vk >> 6] |= (1ULL << (vk & 63));
            }
        }
        // P-42: Precompute fast-check word index.
        // If this hotkey uses exactly one vkMask word and no mouse bits,
        // scanBoundHotkeys can do 1 AND instead of 4 AND + 3 OR.
        {
            const bool hasMouse = (m_hkMasks.mouseMask[id] != 0);
            uint8_t activeWords = 0;
            uint8_t lastWord = 0;
            for (int w = 0; w < 4; ++w) {
                if (m_hkMasks.vkMask[id][w]) {
                    activeWords++;
                    lastWord = static_cast<uint8_t>(w);
                }
            }
            if (activeWords == 0 && !hasMouse) {
                m_boundHotkeys &= ~bbit;
                m_hkFastWord[id] = 5;
            } else if (activeWords == 0) {
                m_boundHotkeys |= bbit;
                m_hkFastWord[id] = 4;  // mouse-only
            } else if (activeWords == 1 && !hasMouse) {
                m_boundHotkeys |= bbit;
                m_hkFastWord[id] = lastWord;  // single word fast path
            } else {
                m_boundHotkeys |= bbit;
                m_hkFastWord[id] = 5;  // multi-word or mixed → full check
            }
        }
    }

    // =========================================================================
    // REFACTORED (R1): pollHotkeys / snapshotInputFrame / resetHotkeyEdges / hotkeyDown
    //
    // Uses takeSnapshot() + scanBoundHotkeys() from the header.
    // Code reduction: ~60 lines -> ~25 lines (4 functions combined).
    // Performance: identical -- inlined helpers produce the same machine code.
    // =========================================================================

    void InputState::pollHotkeys(FrameHotkeyState& out) noexcept {
        const auto snap = takeSnapshot();
        const uint64_t newDown = scanBoundHotkeys(snap);
        out.down = newDown;
        out.pressed = newDown & ~m_hkPrev;
        m_hkPrev = newDown;
    }

    // =========================================================================
    // clearStuckMouseButtons — GetAsyncKeyState recovery for stuck mouse buttons.
    //
    // Root cause: When SDL/Qt calls PeekMessage+DispatchMessage internally, it
    // can dispatch old WM_INPUT handles to HiddenWndProc. GetRawInputData may
    // then return stale button-DOWN data (FIX-1 shared-buffer semantics), even
    // though processRawInputBatched already captured the correct DOWN+UP sequence.
    // The subsequent button-UP WM_INPUT is silently removed by drainMessagesOnly
    // (PM_REMOVE without DispatchMessage), so HiddenWndProc never clears the bit.
    // Result: m_mouseButtons stuck with a button-down bit set.
    //
    // Fix: poll physical button state via GetAsyncKeyState at snapshot time.
    // If a bit is set but the physical button is released, clear it.
    // Overhead: fast-exit when m_mouseButtons == 0 (~99%+ of frames).
    //           At most 5 GetAsyncKeyState calls when any button is stuck.
    // =========================================================================
    FORCE_INLINE bool InputState::clearStuckMouseButtons() noexcept {
        const uint8_t cur = m_mouseButtons.load(std::memory_order_relaxed);
        if (!cur) return false;

        static constexpr UINT kBitToVk[5] = {
            VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2
        };

        uint8_t cleared = cur;
        for (int i = 0; i < 5; ++i) {
            if ((cur >> i) & 1u) {
                if (!(GetAsyncKeyState(kBitToVk[i]) & 0x8000)) {
                    cleared &= ~static_cast<uint8_t>(1u << i);
                }
            }
        }
        if (cleared != cur) {
            m_mouseButtons.store(cleared, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    // =========================================================================
    // clearStuckKeys — GetAsyncKeyState recovery for stuck keyboard keys.
    //
    // Same root cause as clearStuckMouseButtons: stale KEY_DOWN data from
    // GetRawInputData (FIX-1 shared-buffer semantics). Only occurs in the
    // hidden-window path (joy2key OFF), where SDL/Qt PeekMessage+DispatchMessage
    // can dispatch WM_INPUT from the hidden window, triggering processRawInput
    // with stale data that re-sets a key as DOWN after processRawInputBatched
    // already captured the correct DOWN+UP sequence.
    //
    // Scans only set bits in m_vkDown (proportional to held-key count, not 256).
    // Skips VK 0 (invalid). Mouse-button VKs (1-6) are never in m_vkDown.
    // =========================================================================
    FORCE_INLINE bool InputState::clearStuckKeys() noexcept {
        bool anyCleared = false;
        for (int w = 0; w < 4; ++w) {
            const uint64_t cur = m_vkDown[w].load(std::memory_order_relaxed);
            if (!cur) continue;

            uint64_t cleared = cur;
            uint64_t bits = cur;
            while (bits) {
#if defined(_MSC_VER) && !defined(__clang__)
                unsigned long lsb;
                _BitScanForward64(&lsb, bits);
                const int bit = static_cast<int>(lsb);
#else
                const int bit = __builtin_ctzll(bits);
#endif
                bits &= bits - 1;
                const UINT vk = static_cast<UINT>(w * 64 + bit);
                if (vk == 0) continue;
                if (!(GetAsyncKeyState(static_cast<int>(vk)) & 0x8000)) {
                    cleared &= ~(1ULL << bit);
                }
            }
            if (cleared != cur) {
                m_vkDown[w].store(cleared, std::memory_order_relaxed);
                anyCleared = true;
            }
        }
        if (anyCleared) {
            std::atomic_thread_fence(std::memory_order_release);
        }
        return anyCleared;
    }

    // =========================================================================
    // P-1 FIX: Memory ordering correction.
    //
    // Previously, mouse accumulator loads were sequenced BEFORE the acquire
    // fence inside takeSnapshot(). On weakly-ordered architectures (ARM,
    // RISC-V), the CPU could reorder these loads after the VK loads,
    // producing an inconsistent snapshot.
    //
    // Fix: move mouse loads AFTER takeSnapshot() so the acquire fence
    // covers them. On x86 (TSO) this is a no-op; on ARM it prevents
    // load-load reordering.
    // =========================================================================
    void InputState::snapshotInputFrame(FrameHotkeyState& outHk,
        int& outMouseX, int& outMouseY) noexcept
    {
        auto snap = takeSnapshot();  // acquire fence inside
        snap.mouse |= m_mouseButtonPresses.exchange(0, std::memory_order_acquire);

        // clearStuck* runs AFTER takeSnapshot so that valid quick presses
        // (button pressed and released within one frame) are captured in the
        // snapshot before being cleared.  Stuck bits are cleared for the
        // next frame — one frame of false-fire from a stuck button is
        // preferable to silently dropping a genuine click.
        const bool clearedMouse = clearStuckMouseButtons();
        const bool clearedKeys = clearStuckKeys();

        // Mouse loads are now sequenced after the acquire fence,
        // guaranteeing consistency with the VK snapshot.
        const int64_t curX = m_accumMouseX.load(std::memory_order_relaxed);
        const int64_t curY = m_accumMouseY.load(std::memory_order_relaxed);

        outMouseX = static_cast<int>(curX - m_lastReadMouseX);
        outMouseY = static_cast<int>(curY - m_lastReadMouseY);
        m_lastReadMouseX = curX;
        m_lastReadMouseY = curY;

        const uint64_t newDown = scanBoundHotkeys(snap);
        outHk.down = newDown;
        outHk.pressed = newDown & ~m_hkPrev;

        // If recovery cleared stale bits, keep edge tracking aligned with the
        // post-recovery held state. The frame output still preserves the
        // pre-clear snapshot so short clicks are not dropped.
        if (UNLIKELY(clearedMouse || clearedKeys)) {
            m_hkPrev = scanBoundHotkeys(takeSnapshot());
        } else {
            m_hkPrev = newDown;
        }
    }

    // =========================================================================
    // V2: snapshotInputFrameNoEdges — re-entrant snapshot without edge advance.
    //
    // Use this for nested RunFrameHook paths that only need current down-state
    // and mouse deltas. It deliberately does NOT update m_hkPrev, so press-edge
    // detection for the next outer frame is preserved.
    // =========================================================================
    void InputState::snapshotInputFrameNoEdges(FrameHotkeyState& outHk,
        int& outMouseX, int& outMouseY) noexcept
    {
        // No-edge snapshots are used only by re-entrant frame advances. They
        // should reflect physical held state, not the one-frame click cache.
        // Clear stale held bits first.
        (void)clearStuckMouseButtons();
        (void)clearStuckKeys();

        auto snap = takeSnapshot();  // acquire fence inside

        const int64_t curX = m_accumMouseX.load(std::memory_order_relaxed);
        const int64_t curY = m_accumMouseY.load(std::memory_order_relaxed);

        outMouseX = static_cast<int>(curX - m_lastReadMouseX);
        outMouseY = static_cast<int>(curY - m_lastReadMouseY);
        m_lastReadMouseX = curX;
        m_lastReadMouseY = curY;

        outHk.down = scanBoundHotkeys(snap);
        outHk.pressed = 0;
    }

    // =========================================================================
    // R2: hotkeyDown — uses m_boundHotkeys instead of removed hasMask[]
    // =========================================================================
    bool InputState::hotkeyDown(int id) const noexcept {
        if (UNLIKELY(static_cast<unsigned>(id) >= kMaxHotkeyId)) return false;
        if (!(m_boundHotkeys & (1ULL << id))) return false;

        const auto snap = takeSnapshot();
        const uint8_t fw = m_hkFastWord[id];
        if (LIKELY(fw <= 3)) {
            return (m_hkMasks.vkMask[id][fw] & snap.vk[fw]) != 0;
        }
        if (fw == 4) {
            return (m_hkMasks.mouseMask[id] & snap.mouse) != 0;
        }
        return testHotkeyMask(id, snap.vk, snap.mouse);
    }

    void InputState::resetHotkeyEdges() noexcept {
        const auto snap = takeSnapshot();
        m_hkPrev = scanBoundHotkeys(snap);
    }

} // namespace MelonPrime
#endif
