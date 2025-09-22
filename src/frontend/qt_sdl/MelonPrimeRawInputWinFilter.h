#pragma once
#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>
#include <atomic>
#include <array>
#include <vector>
#include <cstdint>
#include <immintrin.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif

// Compiler-specific optimizations
#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#define RESTRICT __restrict
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define RAW_PREFETCH(addr, rw, locality) _mm_prefetch((const char*)(addr), locality)
#else
#define FORCE_INLINE __attribute__((always_inline)) inline
#define RESTRICT __restrict__
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define RAW_PREFETCH(addr, rw, locality) __builtin_prefetch(addr, rw, locality)
#endif

class RawInputWinFilter final : public QAbstractNativeEventFilter
{
public:
    RawInputWinFilter();
    ~RawInputWinFilter() override;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    // Ultra-fast mouse delta with minimal instructions
    FORCE_INLINE void fetchMouseDelta(int& outDx, int& outDy) noexcept {
        // Single xchg instruction per atomic
        outDx = m_state.dx.exchange(0, std::memory_order_relaxed);
        outDy = m_state.dy.exchange(0, std::memory_order_relaxed);
    }

    FORCE_INLINE void discardDeltas() noexcept {
        // Direct store without return value
        m_state.dx.store(0, std::memory_order_relaxed);
        m_state.dy.store(0, std::memory_order_relaxed);
    }

    void resetAllKeys() noexcept;
    void resetMouseButtons() noexcept;
    void setHotkeyVks(int hk, const std::vector<UINT>& vks) noexcept;

    // Hot functions with aggressive optimization
    [[gnu::hot, gnu::flatten]] bool hotkeyDown(int hk) const noexcept;
    [[gnu::hot, gnu::flatten]] bool hotkeyPressed(int hk) noexcept;
    [[gnu::hot, gnu::flatten]] bool hotkeyReleased(int hk) noexcept;

    FORCE_INLINE void resetHotkeyEdges() noexcept {
        // Direct memory clear for maximum speed
#ifdef __AVX512F__
        const __m512i zero = _mm512_setzero_si512();
        _mm512_store_si512(reinterpret_cast<__m512i*>(&m_hkPrev[0]), zero);
        _mm512_store_si512(reinterpret_cast<__m512i*>(&m_hkPrev[8]), zero);
        _mm512_store_si512(reinterpret_cast<__m512i*>(&m_hkPrev[16]), zero);
        _mm512_store_si512(reinterpret_cast<__m512i*>(&m_hkPrev[24]), zero);
#elif defined(__AVX2__)
        const __m256i zero = _mm256_setzero_si256();
        __m256i* RESTRICT ptr = reinterpret_cast<__m256i*>(&m_hkPrev[0]);
        // Unroll completely for 256 hotkeys = 32 bytes = 1 AVX2 store
        _mm256_store_si256(ptr, zero);
#else
        // Compiler will optimize this to rep stosq on x64
        std::memset(&m_hkPrev[0], 0, sizeof(m_hkPrev));
#endif
    }

private:
#ifdef _WIN32
    RAWINPUTDEVICE rid[2]{};

    // Aligned for optimal cache line usage
    alignas(64) BYTE m_rawBuf[128];  // Reduced to exact RAWINPUT size

    // Constants
    enum : uint8_t {
        kMB_Left = 0,
        kMB_Right = 1,
        kMB_Middle = 2,
        kMB_X1 = 3,
        kMB_X2 = 4
    };

    // Combined mask for single test
    static constexpr USHORT kAllMouseBtnMask = 0x03FF;  // All 10 button flags

    // Direct lookup table in L1 cache
    static constexpr uint8_t kMouseButtonLUT[8] = {
        0xFF, 0, 1, 0xFF, 2, 3, 4, 0xFF
    };

    // Bit position tables for branchless conversion
    static constexpr uint8_t kDownBitPos[5] = { 0, 2, 4, 6, 8 };
    static constexpr uint8_t kUpBitPos[5] = { 1, 3, 5, 7, 9 };
#endif

    // Critical path data with optimal alignment
    struct alignas(64) {
        std::atomic<int> dx{ 0 };
        std::atomic<int> dy{ 0 };
        // Pack state in same cache line
        std::atomic<uint64_t> vkDown[4]{ 0, 0, 0, 0 };  // 256-bit keyboard
        std::atomic<uint8_t> mouseButtons{ 0 };         // 5-bit mouse
        uint8_t _pad[7];
    } m_state;

    // Pre-computed masks in single cache line per hotkey
    struct alignas(32) HotkeyMask {
        uint64_t vkMask[4];    // 256-bit
        uint32_t mouseMask;    // 5-bit in 32-bit for alignment
        uint32_t hasMask;      // Flag in 32-bit for alignment
    };

    // Fixed-size array for zero-overhead indexing
    static constexpr size_t kMaxHotkeyId = 256;
    alignas(64) HotkeyMask m_hkMask[kMaxHotkeyId]{};

    // Edge detection packed in minimal space
    alignas(32) std::atomic<uint32_t> m_hkPrev[8]{};  // 256 bits total

    // Inline all bit operations for minimum cycles
    FORCE_INLINE bool getVkState(uint32_t vk) const noexcept {
        // Branchless with conditional move
        const uint32_t idx = vk >> 6;
        const uint32_t valid = vk < 256;
        const uint64_t word = m_state.vkDown[idx & 3].load(std::memory_order_relaxed);
        const uint64_t mask = uint64_t(1) << (vk & 63);
        return (word & mask) & (uint64_t(0) - valid);
    }

    FORCE_INLINE void setVkBit(uint32_t vk, bool down) noexcept {
        // Branchless update
        const uint32_t idx = vk >> 6;
        if (LIKELY(vk < 256)) {
            const uint64_t mask = uint64_t(1) << (vk & 63);
            const uint64_t neg_mask = ~mask;
            const uint64_t set_mask = mask & (uint64_t(0) - down);
            const uint64_t clear_mask = neg_mask | (uint64_t(0) - down);

            // Single RMW operation
            m_state.vkDown[idx].fetch_and(clear_mask, std::memory_order_relaxed);
            m_state.vkDown[idx].fetch_or(set_mask, std::memory_order_relaxed);
        }
    }

    FORCE_INLINE uint8_t getMouseButtons() const noexcept {
        return m_state.mouseButtons.load(std::memory_order_relaxed);
    }

    FORCE_INLINE void updateMouseButtons(uint8_t down, uint8_t up) noexcept {
        // Single atomic RMW
        uint8_t expected = m_state.mouseButtons.load(std::memory_order_relaxed);
        uint8_t desired;
        do {
            desired = (expected | down) & ~up;
        } while (!m_state.mouseButtons.compare_exchange_weak(
            expected, desired, std::memory_order_relaxed));
    }

    // Pre-compute mask addition
    static FORCE_INLINE void addVkToMask(HotkeyMask& RESTRICT m, UINT vk) noexcept {
        // Branchless mask update
        const uint32_t is_mouse = vk < 8;
        const uint32_t is_keyboard = (vk >= 8) & (vk < 256);

        // Mouse path
        const uint8_t btn_idx = kMouseButtonLUT[vk & 7];
        const uint32_t mouse_bit = (1U << btn_idx) & (uint32_t(0) - (btn_idx < 5));
        m.mouseMask |= mouse_bit & (uint32_t(0) - is_mouse);

        // Keyboard path
        const uint32_t kb_idx = vk >> 6;
        const uint64_t kb_mask = uint64_t(1) << (vk & 63);
        m.vkMask[kb_idx & 3] |= kb_mask & (uint64_t(0) - is_keyboard);

        // Set flag if either path succeeded
        m.hasMask |= is_mouse | is_keyboard;
    }

    // Ultra-fast hotkey check with SIMD
    FORCE_INLINE bool checkHotkeyMask(const HotkeyMask& RESTRICT m) const noexcept {
#ifdef __AVX2__
        // Load state and mask as vectors
        const __m256i state = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&m_state.vkDown[0]));
        const __m256i mask = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&m.vkMask[0]));

        // Single AND and test
        const __m256i result = _mm256_and_si256(state, mask);
        const int has_keys = !_mm256_testz_si256(result, result);

        // Mouse check
        const uint32_t has_mouse = (getMouseButtons() & m.mouseMask);

        return has_keys | has_mouse;
#else
        // Manual unroll for better pipelining
        const uint64_t k0 = m_state.vkDown[0].load(std::memory_order_relaxed) & m.vkMask[0];
        const uint64_t k1 = m_state.vkDown[1].load(std::memory_order_relaxed) & m.vkMask[1];
        const uint64_t k2 = m_state.vkDown[2].load(std::memory_order_relaxed) & m.vkMask[2];
        const uint64_t k3 = m_state.vkDown[3].load(std::memory_order_relaxed) & m.vkMask[3];
        const uint64_t has_keys = k0 | k1 | k2 | k3;
        const uint32_t has_mouse = getMouseButtons() & m.mouseMask;
        return has_keys | has_mouse;
#endif
    }
};