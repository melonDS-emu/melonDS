#pragma once
#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>
#include <atomic>
#include <array>
#include <vector>
#include <cstdint>
#include <QBitArray>
#include <immintrin.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif

class RawInputWinFilter final : public QAbstractNativeEventFilter
{
public:
    RawInputWinFilter();
    ~RawInputWinFilter() override;

    inline void setJoyHotkeyMaskPtr(const QBitArray* p) noexcept {
        m_joyHK = p ? p : &kEmptyMask;
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    // Inline hot path functions
    [[gnu::hot, gnu::always_inline]] inline void fetchMouseDelta(int& outDx, int& outDy) noexcept {
        // Use relaxed ordering for better performance
        outDx = dx.exchange(0, std::memory_order_relaxed);
        outDy = dy.exchange(0, std::memory_order_relaxed);
    }

    [[gnu::hot, gnu::always_inline]] inline void discardDeltas() noexcept {
        dx.store(0, std::memory_order_relaxed);
        dy.store(0, std::memory_order_relaxed);
    }

    void resetAllKeys() noexcept;
    void resetMouseButtons() noexcept;

    void setHotkeyVks(int hk, const std::vector<UINT>& vks) noexcept;

    // Most frequently called functions
    [[gnu::hot, gnu::flatten]] bool hotkeyDown(int hk) const noexcept;
    [[gnu::hot, gnu::flatten]] bool hotkeyPressed(int hk) noexcept;
    [[gnu::hot]] bool hotkeyReleased(int hk) noexcept;

    [[gnu::always_inline]] inline void resetHotkeyEdges() noexcept {
#ifdef __AVX2__
        // Use AVX2 for fast zeroing
        const __m256i zero = _mm256_setzero_si256();
        __m256i* ptr = reinterpret_cast<__m256i*>(m_hkPrev.data());
        constexpr size_t chunks = sizeof(m_hkPrev) / 32;
        for (size_t i = 0; i < chunks; ++i) {
            _mm256_store_si256(ptr + i, zero);
        }
#else
        std::memset(m_hkPrev.data(), 0, sizeof(m_hkPrev));
#endif
    }

private:
#ifdef _WIN32
    RAWINPUTDEVICE rid[2]{};
    const QBitArray* m_joyHK = nullptr;
    inline static const QBitArray kEmptyMask{};

    // Cache line aligned buffer (128 bytes for AVX-512 compatibility)
    alignas(128) BYTE m_rawBuf[sizeof(RAWINPUT) + 128];

    // Optimized bitmask constants
    static constexpr USHORT kAllMouseBtnMask =
        RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP |
        RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP |
        RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP |
        RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP |
        RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP;

    // Button mapping structure
    struct ButtonMap {
        USHORT down;
        USHORT up;
        uint8_t idx;
        uint8_t _pad; // Padding for alignment
    };

    static constexpr ButtonMap kButtonMaps[5] = {
        {RI_MOUSE_LEFT_BUTTON_DOWN,   RI_MOUSE_LEFT_BUTTON_UP,   0, 0},
        {RI_MOUSE_RIGHT_BUTTON_DOWN,  RI_MOUSE_RIGHT_BUTTON_UP,  1, 0},
        {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, 2, 0},
        {RI_MOUSE_BUTTON_4_DOWN,      RI_MOUSE_BUTTON_4_UP,      3, 0},
        {RI_MOUSE_BUTTON_5_DOWN,      RI_MOUSE_BUTTON_5_UP,      4, 0},
    };
#endif

    // Cache line separation for atomic variables
    alignas(128) std::atomic<int> dx{ 0 };
    alignas(128) std::atomic<int> dy{ 0 };

    static constexpr size_t kMouseBtnCount = 5;
    enum : uint8_t { kMB_Left = 0, kMB_Right = 1, kMB_Middle = 2, kMB_X1 = 3, kMB_X2 = 4 };

    // Memory layout optimization with proper alignment
    alignas(64) std::array<std::atomic<uint8_t>, 256> m_vkDown{};
    alignas(64) std::array<std::atomic<uint8_t>, kMouseBtnCount> m_mb{};
    alignas(64) std::array<std::atomic<uint8_t>, 512> m_hkPrev{};

    // Use fixed-size array for better cache locality (assuming max 64 hotkeys)
    static constexpr size_t kMaxHotkeys = 64;
    struct HotkeyMapping {
        int hk;
        uint8_t vkCount;
        uint8_t _pad[3]; // Padding for alignment
        UINT vks[16]; // Fixed size array for virtual keys
    };

    alignas(64) std::array<HotkeyMapping, kMaxHotkeys> m_hkMappings{};
    std::atomic<size_t> m_hkCount{ 0 };

    // Fast lookup table for mouse buttons
    static constexpr uint8_t kMouseButtonLUT[8] = {
        255,        // VK_NULL
        kMB_Left,   // VK_LBUTTON
        kMB_Right,  // VK_RBUTTON
        255,        // VK_CANCEL
        kMB_Middle, // VK_MBUTTON
        kMB_X1,     // VK_XBUTTON1
        kMB_X2,     // VK_XBUTTON2
        255         // padding
    };

    // Helper for fast hotkey lookup
    [[gnu::hot, gnu::always_inline]] inline const HotkeyMapping* findHotkey(int hk) const noexcept {
        const size_t count = m_hkCount.load(std::memory_order_acquire);
        // Unroll for small counts
        if (count <= 8) {
            for (size_t i = 0; i < count; ++i) {
                if (m_hkMappings[i].hk == hk) {
                    return &m_hkMappings[i];
                }
            }
            return nullptr;
        }

        // Linear search for larger counts
        const HotkeyMapping* begin = m_hkMappings.data();
        const HotkeyMapping* end = begin + count;
        for (const HotkeyMapping* it = begin; it != end; ++it) {
            if (it->hk == hk) return it;
        }
        return nullptr;
    }
};