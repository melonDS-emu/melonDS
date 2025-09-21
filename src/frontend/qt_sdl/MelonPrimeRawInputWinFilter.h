#pragma once
#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>
#include <atomic>
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <QBitArray>

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

    // インライン化でコール時間を削減
    [[gnu::hot]] inline void fetchMouseDelta(int& outDx, int& outDy) noexcept {
        outDx = dx.exchange(0, std::memory_order_relaxed);
        outDy = dy.exchange(0, std::memory_order_relaxed);
    }

    [[gnu::hot]] inline void discardDeltas() noexcept {
        dx.store(0, std::memory_order_relaxed);
        dy.store(0, std::memory_order_relaxed);
    }

    void resetAllKeys() noexcept;
    void resetMouseButtons() noexcept;

    void setHotkeyVks(int hk, const std::vector<UINT>& vks);

    // 最も頻繁に呼ばれる関数を最適化
    [[gnu::hot]] bool hotkeyDown(int hk) const noexcept;
    [[gnu::hot]] bool hotkeyPressed(int hk) noexcept;
    [[gnu::hot]] bool hotkeyReleased(int hk) noexcept;

    inline void resetHotkeyEdges() noexcept {
        // メモリ最適化: 0埋めをより高速に
        std::memset(m_hkPrev.data(), 0, sizeof(m_hkPrev));
    }

private:
#ifdef _WIN32
    RAWINPUTDEVICE rid[2]{};
    const QBitArray* m_joyHK = nullptr;
    inline static const QBitArray kEmptyMask{};

    // キャッシュライン境界に整列（64バイト）
    alignas(64) BYTE m_rawBuf[sizeof(RAWINPUT) + 64];

    // ビットマスク最適化: constexprで計算時間を削減
    static constexpr USHORT kAllMouseBtnMask =
        RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP |
        RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP |
        RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP |
        RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP |
        RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP;

    // マッピング構造体をconstexprで最適化（アグリゲート初期化を使用）
    struct ButtonMap {
        USHORT down;
        USHORT up;
        uint8_t idx;
    };

    static constexpr ButtonMap kButtonMaps[5] = {
        {RI_MOUSE_LEFT_BUTTON_DOWN,   RI_MOUSE_LEFT_BUTTON_UP,   0},
        {RI_MOUSE_RIGHT_BUTTON_DOWN,  RI_MOUSE_RIGHT_BUTTON_UP,  1},
        {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, 2},
        {RI_MOUSE_BUTTON_4_DOWN,      RI_MOUSE_BUTTON_4_UP,      3},
        {RI_MOUSE_BUTTON_5_DOWN,      RI_MOUSE_BUTTON_5_UP,      4},
    };
#endif

    // キャッシュライン分離でfalse sharingを回避
    alignas(64) std::atomic<int> dx{ 0 };
    alignas(64) std::atomic<int> dy{ 0 };

    static constexpr size_t kMouseBtnCount = 5;
    enum : uint8_t { kMB_Left = 0, kMB_Right = 1, kMB_Middle = 2, kMB_X1 = 3, kMB_X2 = 4 };

    // メモリレイアウト最適化
    alignas(64) std::array<std::atomic<uint8_t>, 256> m_vkDown{};
    alignas(64) std::array<std::atomic<uint8_t>, kMouseBtnCount> m_mb{};
    alignas(64) std::array<std::atomic<uint8_t>, 512> m_hkPrev{};

    // ホットパス用にflat_mapを使用（検索が高速）
    std::unordered_map<int, std::vector<UINT>> m_hkToVk;
};