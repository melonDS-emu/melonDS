#include "MelonPrimeRawInputWinFilter.h"
#include <cstring>
#include <algorithm>
#include <immintrin.h>

#ifdef _WIN32
#include <windows.h>
#include <hidsdi.h>
#endif

RawInputWinFilter::RawInputWinFilter()
{
    rid[0] = { 0x01, 0x02, 0, nullptr }; // mouse
    rid[1] = { 0x01, 0x06, 0, nullptr }; // keyboard
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));

    std::memset(m_vkDown.data(), 0, sizeof(m_vkDown));
    std::memset(m_hkPrev.data(), 0, sizeof(m_hkPrev));

    std::memset(m_mb.data(), 0, sizeof(m_mb));

    dx.store(0, std::memory_order_relaxed);
    dy.store(0, std::memory_order_relaxed);

    if (!m_joyHK) m_joyHK = &kEmptyMask;
}

RawInputWinFilter::~RawInputWinFilter()
{
    rid[0].dwFlags = RIDEV_REMOVE; rid[0].hwndTarget = nullptr;
    rid[1].dwFlags = RIDEV_REMOVE; rid[1].hwndTarget = nullptr;
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
}

bool RawInputWinFilter::nativeEventFilter(const QByteArray& /*eventType*/, void* message, qintptr* /*result*/)
{
#ifdef _WIN32
    MSG* const msg = static_cast<MSG*>(message);

    // Early return with branch prediction
    if (Q_UNLIKELY(!msg)) [[unlikely]] return false;
    if (Q_LIKELY(msg->message != WM_INPUT)) [[likely]] return false;

    // Pre-calculate buffer size
    constexpr UINT expectedSize = sizeof(RAWINPUT);
    UINT size = expectedSize;

    // Optimized GetRawInputData call
    const UINT result = GetRawInputData(
        reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT,
        m_rawBuf,
        &size,
        sizeof(RAWINPUTHEADER)
    );

    if (Q_UNLIKELY(result == static_cast<UINT>(-1))) [[unlikely]] return false;

    const RAWINPUT* const raw = reinterpret_cast<const RAWINPUT*>(m_rawBuf);
    const DWORD deviceType = raw->header.dwType;

    // Most frequent case first - mouse movement
    if (Q_LIKELY(deviceType == RIM_TYPEMOUSE)) [[likely]] {
        const RAWMOUSE& mouse = raw->data.mouse;

        // Mouse movement processing (optimized)
        const LONG deltaX = mouse.lLastX;
        const LONG deltaY = mouse.lLastY;

        // Use bitwise OR for zero check
        if (Q_LIKELY((deltaX | deltaY) != 0)) [[likely]] {
            // Prefetch cache line for atomic operations
            _mm_prefetch(reinterpret_cast<const char*>(&dx), _MM_HINT_T0);
            _mm_prefetch(reinterpret_cast<const char*>(&dy), _MM_HINT_T0);

            dx.fetch_add(deltaX, std::memory_order_relaxed);
            dy.fetch_add(deltaY, std::memory_order_relaxed);
        }

        const USHORT buttonFlags = mouse.usButtonFlags;
        const USHORT relevantFlags = buttonFlags & kAllMouseBtnMask;

        // Skip if no button events
        if (Q_LIKELY(relevantFlags == 0)) [[likely]] return false;

        // Optimized button processing with manual unrolling
        // Process buttons using bit manipulation
        if (relevantFlags & (RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP)) {
            m_mb[0].store((buttonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) ? 1u : 0u,
                std::memory_order_relaxed);
        }
        if (relevantFlags & (RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP)) {
            m_mb[1].store((buttonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) ? 1u : 0u,
                std::memory_order_relaxed);
        }
        if (relevantFlags & (RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP)) {
            m_mb[2].store((buttonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) ? 1u : 0u,
                std::memory_order_relaxed);
        }
        if (relevantFlags & (RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP)) {
            m_mb[3].store((buttonFlags & RI_MOUSE_BUTTON_4_DOWN) ? 1u : 0u,
                std::memory_order_relaxed);
        }
        if (relevantFlags & (RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP)) {
            m_mb[4].store((buttonFlags & RI_MOUSE_BUTTON_5_DOWN) ? 1u : 0u,
                std::memory_order_relaxed);
        }
    }
    else if (Q_UNLIKELY(deviceType == RIM_TYPEKEYBOARD)) [[unlikely]] {
        const RAWKEYBOARD& keyboard = raw->data.keyboard;
        UINT virtualKey = keyboard.VKey;
        const USHORT flags = keyboard.Flags;
        const bool isKeyUp = (flags & RI_KEY_BREAK) != 0;

        // Use jump table for special key normalization
        if (Q_UNLIKELY(virtualKey == VK_SHIFT || virtualKey == VK_CONTROL || virtualKey == VK_MENU)) [[unlikely]] {
            switch (virtualKey) {
            case VK_SHIFT:
                virtualKey = MapVirtualKey(keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);
                break;
            case VK_CONTROL:
                virtualKey = (flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;
                break;
            case VK_MENU:
                virtualKey = (flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;
                break;
            }
        }

        // Boundary check with prefetch
        if (Q_LIKELY(virtualKey < m_vkDown.size())) [[likely]] {
            _mm_prefetch(reinterpret_cast<const char*>(&m_vkDown[virtualKey]), _MM_HINT_T0);
            const uint8_t state = static_cast<uint8_t>(!isKeyUp);
            m_vkDown[virtualKey].store(state, std::memory_order_relaxed);
        }
    }

    return false;
#else
    return false;
#endif
}

void RawInputWinFilter::resetAllKeys() noexcept
{
    // Use AVX2 for faster clearing
#ifdef __AVX2__
    const __m256i zero = _mm256_setzero_si256();
    __m256i* ptr = reinterpret_cast<__m256i*>(m_vkDown.data());

    // Unroll loop for better performance
    constexpr size_t chunks = sizeof(m_vkDown) / 32;
    size_t i = 0;

    // Process 4 chunks at a time
    for (; i + 3 < chunks; i += 4) {
        _mm256_store_si256(ptr + i, zero);
        _mm256_store_si256(ptr + i + 1, zero);
        _mm256_store_si256(ptr + i + 2, zero);
        _mm256_store_si256(ptr + i + 3, zero);
    }

    // Process remaining chunks
    for (; i < chunks; ++i) {
        _mm256_store_si256(ptr + i, zero);
    }
#else
    std::memset(m_vkDown.data(), 0, sizeof(m_vkDown));
#endif
}

void RawInputWinFilter::resetMouseButtons() noexcept
{
    // Unroll for small fixed size
    m_mb[0].store(0, std::memory_order_relaxed);
    m_mb[1].store(0, std::memory_order_relaxed);
    m_mb[2].store(0, std::memory_order_relaxed);
    m_mb[3].store(0, std::memory_order_relaxed);
    m_mb[4].store(0, std::memory_order_relaxed);
}

void RawInputWinFilter::setHotkeyVks(int hk, const std::vector<UINT>& vks) noexcept
{
    const size_t currentCount = m_hkCount.load(std::memory_order_acquire);

    // Find existing or add new
    for (size_t i = 0; i < currentCount; ++i) {
        if (m_hkMappings[i].hk == hk) {
            // Update existing
            m_hkMappings[i].vkCount = static_cast<uint8_t>(std::min(vks.size(), size_t(16)));
            std::memcpy(m_hkMappings[i].vks, vks.data(),
                m_hkMappings[i].vkCount * sizeof(UINT));
            return;
        }
    }

    // Add new if space available
    if (currentCount < kMaxHotkeys) {
        HotkeyMapping& mapping = m_hkMappings[currentCount];
        mapping.hk = hk;
        mapping.vkCount = static_cast<uint8_t>(std::min(vks.size(), size_t(16)));
        std::memcpy(mapping.vks, vks.data(), mapping.vkCount * sizeof(UINT));
        m_hkCount.store(currentCount + 1, std::memory_order_release);
    }
}

bool RawInputWinFilter::hotkeyDown(int hk) const noexcept
{
    // 1) Check keyboard/mouse state (optimized)
    const HotkeyMapping* mapping = findHotkey(hk);
    if (Q_LIKELY(mapping != nullptr)) [[likely]] {
        const uint8_t vkCount = mapping->vkCount;
        const UINT* vks = mapping->vks;

        // Manual unrolling for common cases
        if (vkCount == 1) {
            const UINT vk = vks[0];
            if (vk < 8) {
                const uint8_t mbIndex = kMouseButtonLUT[vk];
                if (mbIndex != 255) {
                    return m_mb[mbIndex].load(std::memory_order_relaxed) != 0;
                }
            }
            return (vk < m_vkDown.size()) &&
                m_vkDown[vk].load(std::memory_order_relaxed) != 0;
        }

        // General case with prefetching
        for (uint8_t i = 0; i < vkCount; ++i) {
            const UINT vk = vks[i];

            if (Q_UNLIKELY(vk < 8)) [[unlikely]] {
                const uint8_t mbIndex = kMouseButtonLUT[vk];
                if (mbIndex != 255 && m_mb[mbIndex].load(std::memory_order_relaxed)) {
                    return true;
                }
            }
            else if (Q_LIKELY(vk < m_vkDown.size())) [[likely]] {
                // Prefetch next key if not last
                if (i + 1 < vkCount && vks[i + 1] < m_vkDown.size()) {
                    _mm_prefetch(reinterpret_cast<const char*>(&m_vkDown[vks[i + 1]]), _MM_HINT_T0);
                }

                if (m_vkDown[vk].load(std::memory_order_relaxed)) {
                    return true;
                }
            }
        }
    }

    // 2) Check joystick state
    const QBitArray* const joyMask = m_joyHK;
    const int maskSize = joyMask->size();

    if (Q_LIKELY(static_cast<unsigned>(hk) < static_cast<unsigned>(maskSize))) [[likely]] {
        return joyMask->testBit(hk);
    }

    return false;
}

bool RawInputWinFilter::hotkeyPressed(int hk) noexcept
{
    const bool currentState = hotkeyDown(hk);
    const size_t index = static_cast<size_t>(hk) & 511;

    // Use exchange for atomic read-modify-write
    const uint8_t previousState = m_hkPrev[index].exchange(
        static_cast<uint8_t>(currentState),
        std::memory_order_acq_rel
    );

    // Rising edge detection
    return currentState && !previousState;
}

bool RawInputWinFilter::hotkeyReleased(int hk) noexcept
{
    const bool currentState = hotkeyDown(hk);
    const size_t index = static_cast<size_t>(hk) & 511;

    // Use exchange for atomic read-modify-write
    const uint8_t previousState = m_hkPrev[index].exchange(
        static_cast<uint8_t>(currentState),
        std::memory_order_acq_rel
    );

    // Falling edge detection
    return !currentState && previousState;
}