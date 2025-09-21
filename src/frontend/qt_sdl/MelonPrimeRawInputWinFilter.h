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

    inline void setJoyHotkeyMaskPtr(const QBitArray* p) noexcept { m_joyHK = p ? p : &kEmptyMask; }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    void fetchMouseDelta(int& outDx, int& outDy);
    void discardDeltas();
    void resetAllKeys();
    void resetMouseButtons();

    void setHotkeyVks(int hk, const std::vector<UINT>& vks);
    bool hotkeyDown(int hk) const;
    bool hotkeyPressed(int hk)  noexcept;
    bool hotkeyReleased(int hk) noexcept;
    inline void resetHotkeyEdges() noexcept { for (auto& a : m_hkPrev) a.store(0, std::memory_order_relaxed); }

private:
#ifdef _WIN32
    RAWINPUTDEVICE rid[2]{};

    // ★ ここをデフォルト安全ポインタに（nullでも落ちない）
    const QBitArray* m_joyHK = nullptr;
    inline static const QBitArray kEmptyMask{};

    // ★ 事前確保バッファ（WM_INPUT→RAWINPUT用）
    alignas(128) BYTE m_rawBuf[sizeof(RAWINPUT)];

    // ★ ボタン系フラグ総和（ホイール等を一撃スキップ）
    static constexpr USHORT kAllMouseBtnMask =
        RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP |
        RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP |
        RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP |
        RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP |
        RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP;
#endif

    std::atomic<int> dx{ 0 }, dy{ 0 };

    static constexpr size_t kMouseBtnCount = 5;
    enum : size_t { kMB_Left = 0, kMB_Right = 1, kMB_Middle = 2, kMB_X1 = 3, kMB_X2 = 4 };

    std::array<std::atomic<uint8_t>, 256>         m_vkDown{};
    std::array<std::atomic<uint8_t>, kMouseBtnCount> m_mb{};

    std::unordered_map<int, std::vector<UINT>> m_hkToVk;
    std::array<std::atomic<uint8_t>, 512>      m_hkPrev{};
};
