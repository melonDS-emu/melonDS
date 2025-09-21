// MelonPrimeDirectInputFilter.h
#pragma once
#ifdef _WIN32
#include <Windows.h>
#include <dinput.h>
#include <array>
#include <unordered_map>
#include <atomic>
#include <cstdint>
#include <cstring>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p)=nullptr; } } while(0)
#endif

class MelonPrimeDirectInputFilter {
public:
    // XInput と同じ見た目の軸名で統一
    enum class Axis : uint8_t { LXPos, LXNeg, LYPos, LYNeg, RXPos, RXNeg, RYPos, RYNeg, LT, RT };

    struct Binding {
        enum Type : uint8_t { None, Button, Analog, POV } type = None;
        union {
            uint8_t buttonIndex;   // 0..127
            Axis    axis;          // Analog
            uint8_t povDir;        // 0:Up 1:Right 2:Down 3:Left
        } u{};
        float threshold = 0.5f;    // Analog 用(0..1)
    };

public:
    MelonPrimeDirectInputFilter() = default;
    ~MelonPrimeDirectInputFilter();

    // 生成＆最初の接続。Qtなら panel->winId() を HWND にして渡す
    bool init(HWND hwnd);
    void shutdown();

    // 毎フレ1回（最小コスト）
    void update() noexcept;

    // バインドAPI（XInput と同じシグネチャ）
    void bindButton(int hk, uint8_t diButtonIndex);
    void bindAxisThreshold(int hk, Axis axis, float threshold);
    void bindPOVDirection(int hk, uint8_t dir0123); // 0:U 1:R 2:D 3:L
    void clearBinding(int hk);

    // 取得（XInputと同名）
    bool  hotkeyDown(int hk) const noexcept;
    bool  hotkeyPressed(int hk) noexcept;
    bool  hotkeyReleased(int hk) noexcept;
    float axisValue(Axis a) const noexcept;

    // デッドゾーン設定（0..10000%）と軸範囲（-32768..32767）固定
    void setDeadzone(short left = 7849, short right = 8689, short trigPct = 300) noexcept {
        m_dzLeft = left; m_dzRight = right; m_dzTriggerPct = trigPct;
    }

    bool connected() const noexcept { return m_connected.load(std::memory_order_acquire); }

private:
    // 内部
    static BOOL CALLBACK enumCb(const DIDEVICEINSTANCE* inst, VOID* ctx);
    bool openDevice(const GUID& guid);
    void closeDevice();
    void ensureRanges(); // すべての軸を -32768..32767 に揃える
    static float normStick(LONG v, short dz) noexcept;     // [-32768..32767]→[-1..1]（デッドゾーン適用）
    static float normTrig(LONG v, short pctDZ) noexcept;   // [0..32767]→[0..1]（%DZ）

    bool evalBinding(const Binding& b) const noexcept;

private:
    // COM
    LPDIRECTINPUT8         m_di = nullptr;
    LPDIRECTINPUTDEVICE8   m_dev = nullptr;
    HWND                   m_hwnd = nullptr;

    // 状態
    DIJOYSTATE2 m_now{};   // 毎フレ update で更新
    DIJOYSTATE2 m_prev{};  // エッジ検出用
    std::atomic<bool> m_connected{ false };

    // 設定
    short m_dzLeft = 7849, m_dzRight = 8689; // スティックのDZ
    short m_dzTriggerPct = 300;              // トリガーのDZ(千分率: 300=3%)

    // HK→Binding
    std::unordered_map<int, Binding> m_bind;

    // エッジ検出
    std::array<std::atomic<uint8_t>, 512> m_prevDown{};
};

#endif // _WIN32
