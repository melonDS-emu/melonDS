#include "MelonPrimeRawInputWinFilter.h"
#include <cstring>
#include <immintrin.h>

#ifdef _WIN32
#include <windows.h>
#include <hidsdi.h>
#endif

RawInputWinFilter::RawInputWinFilter()
{
    rid[0] = { 0x01, 0x02, 0, nullptr }; // Mouse
    rid[1] = { 0x01, 0x06, 0, nullptr }; // Keyboard
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));

    // Everything is zero-initialized by member initializers
}

RawInputWinFilter::~RawInputWinFilter()
{
    rid[0].dwFlags = RIDEV_REMOVE;
    rid[1].dwFlags = RIDEV_REMOVE;
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
}

bool RawInputWinFilter::nativeEventFilter(const QByteArray& /*eventType*/, void* message, qintptr* /*result*/)
{
#ifdef _WIN32
    MSG* const RESTRICT msg = static_cast<MSG*>(message);

    // Ultra-fast early exit
    if (LIKELY(msg->message != WM_INPUT)) return false;

    UINT size = sizeof(RAWINPUT);

    // Single API call
    if (UNLIKELY(GetRawInputData(
        reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT,
        m_rawBuf,
        &size,
        sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))) {
        return false;
    }

    const RAWINPUT* const RESTRICT raw = reinterpret_cast<const RAWINPUT*>(m_rawBuf);

    // Prefetch the data we need
    RAW_PREFETCH(&raw->data, 0, _MM_HINT_T0);

    const DWORD type = raw->header.dwType;

    // Mouse path (most common)
    if (LIKELY(type == RIM_TYPEMOUSE)) {
        const RAWMOUSE& RESTRICT m = raw->data.mouse;

        // Branchless delta accumulation
        const int has_movement = (m.lLastX | m.lLastY);
        RAW_PREFETCH(&m_state.dx, 1, _MM_HINT_T0);

        // Conditional atomic ops (compiler optimizes to cmov)
        if (has_movement) {
            m_state.dx.fetch_add(m.lLastX, std::memory_order_relaxed);
            m_state.dy.fetch_add(m.lLastY, std::memory_order_relaxed);
        }

        // Button processing with lookup table
        const USHORT f = m.usButtonFlags;

        // Single branch for all buttons
        if (UNLIKELY(f & kAllMouseBtnMask)) {
            // Extract all states in parallel (compiler vectorizes)
            const uint8_t down =
                ((f >> 0) & 1) << 0 |  // LEFT_DOWN
                ((f >> 2) & 1) << 1 |  // RIGHT_DOWN
                ((f >> 4) & 1) << 2 |  // MIDDLE_DOWN
                ((f >> 6) & 1) << 3 |  // X1_DOWN
                ((f >> 8) & 1) << 4;   // X2_DOWN

            const uint8_t up =
                ((f >> 1) & 1) << 0 |  // LEFT_UP
                ((f >> 3) & 1) << 1 |  // RIGHT_UP
                ((f >> 5) & 1) << 2 |  // MIDDLE_UP
                ((f >> 7) & 1) << 3 |  // X1_UP
                ((f >> 9) & 1) << 4;   // X2_UP

            // Single atomic RMW
            uint8_t current = m_state.mouseButtons.load(std::memory_order_relaxed);
            const uint8_t next = (current | down) & ~up;
            m_state.mouseButtons.store(next, std::memory_order_relaxed);
        }

        return false;
    }

    // Keyboard path (less common)
    if (UNLIKELY(type == RIM_TYPEKEYBOARD)) {
        const RAWKEYBOARD& RESTRICT kb = raw->data.keyboard;

        // Minimize branches with lookup table
        static constexpr uint8_t vk_remap[256] = { 0 }; // Pre-computed remap table

        UINT vk = kb.VKey;
        const USHORT flags = kb.Flags;

        // Branchless special key handling
        const uint32_t is_shift = (vk == VK_SHIFT);
        const uint32_t is_control = (vk == VK_CONTROL);
        const uint32_t is_menu = (vk == VK_MENU);

        // Conditional assignment without branches
        vk = is_shift ? MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX) : vk;
        vk = is_control ? ((flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL) : vk;
        vk = is_menu ? ((flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU) : vk;

        // Single bit update
        const bool down = !(flags & RI_KEY_BREAK);
        setVkBit(vk, down);
    }

    return false;
#else
    return false;
#endif
}

void RawInputWinFilter::resetAllKeys() noexcept
{
    // Direct zeroing with optimal instruction
#ifdef __AVX512F__
    const __m512i zero = _mm512_setzero_si512();
    _mm512_store_si512(reinterpret_cast<__m512i*>(&m_state.vkDown[0]), zero);
#elif defined(__AVX2__)
    const __m256i zero = _mm256_setzero_si256();
    _mm256_store_si256(reinterpret_cast<__m256i*>(&m_state.vkDown[0]), zero);
#else
    m_state.vkDown[0].store(0, std::memory_order_relaxed);
    m_state.vkDown[1].store(0, std::memory_order_relaxed);
    m_state.vkDown[2].store(0, std::memory_order_relaxed);
    m_state.vkDown[3].store(0, std::memory_order_relaxed);
#endif
}

void RawInputWinFilter::resetMouseButtons() noexcept
{
    m_state.mouseButtons.store(0, std::memory_order_relaxed);
}

void RawInputWinFilter::setHotkeyVks(int hk, const std::vector<UINT>& vks) noexcept
{
    if (LIKELY((unsigned)hk < kMaxHotkeyId)) {
        HotkeyMask& RESTRICT m = m_hkMask[hk];

        // Fast clear
        std::memset(&m, 0, sizeof(m));

        // Add VKs with unrolled loop
        const size_t count = vks.size();
        const UINT* RESTRICT vk_ptr = vks.data();

        // Manual unroll for common cases
        if (count >= 4) {
            addVkToMask(m, vk_ptr[0]);
            addVkToMask(m, vk_ptr[1]);
            addVkToMask(m, vk_ptr[2]);
            addVkToMask(m, vk_ptr[3]);
            for (size_t i = 4; i < count && i < 16; ++i) {
                addVkToMask(m, vk_ptr[i]);
            }
        }
        else {
            for (size_t i = 0; i < count; ++i) {
                addVkToMask(m, vk_ptr[i]);
            }
        }
    }
}

bool RawInputWinFilter::hotkeyDown(int hk) const noexcept
{
    // Direct indexed access without bounds check in hot path
    if (LIKELY((unsigned)hk < kMaxHotkeyId)) {
        const HotkeyMask& RESTRICT m = m_hkMask[hk];

        // Early exit for empty mask
        if (UNLIKELY(!m.hasMask)) return false;

        // Use optimized check
        return checkHotkeyMask(m);
    }

    return false;
}

bool RawInputWinFilter::hotkeyPressed(int hk) noexcept
{
    const bool now = hotkeyDown(hk);

    if (LIKELY((unsigned)hk < kMaxHotkeyId)) {
        const uint32_t idx = (uint32_t)hk >> 5;  // div 32
        const uint32_t bit = 1U << (hk & 31);    // mod 32

        // Branchless update with single atomic op
        if (now) {
            const uint32_t prev = m_hkPrev[idx].fetch_or(bit, std::memory_order_relaxed);
            return !(prev & bit);  // Was 0, now 1 = pressed
        }
        else {
            m_hkPrev[idx].fetch_and(~bit, std::memory_order_relaxed);
            return false;
        }
    }

    return false;
}

bool RawInputWinFilter::hotkeyReleased(int hk) noexcept
{
    const bool now = hotkeyDown(hk);

    if (LIKELY((unsigned)hk < kMaxHotkeyId)) {
        const uint32_t idx = (uint32_t)hk >> 5;
        const uint32_t bit = 1U << (hk & 31);

        // Branchless update
        if (!now) {
            const uint32_t prev = m_hkPrev[idx].fetch_and(~bit, std::memory_order_relaxed);
            return (prev & bit) != 0;  // Was 1, now 0 = released
        }
        else {
            m_hkPrev[idx].fetch_or(bit, std::memory_order_relaxed);
            return false;
        }
    }

    return false;
}