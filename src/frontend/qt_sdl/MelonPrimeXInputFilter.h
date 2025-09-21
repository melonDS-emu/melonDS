#pragma once
#ifdef _WIN32
#include <Windows.h>
#include <Xinput.h>
#include <unordered_map>
#include <array>
#include <atomic>
#include <cstdint>

#ifndef XINPUT_GAMEPAD_GUIDE
#define XINPUT_GAMEPAD_GUIDE 0x0400
#endif

class MelonPrimeXInputFilter {
public:
    enum class Axis : uint8_t { LXPos, LXNeg, LYPos, LYNeg, RXPos, RXNeg, RYPos, RYNeg, LT, RT };

    struct Binding {
        // ★列挙子名を Axis → Analog に変更（型Axisとの衝突回避）
        enum Type : uint8_t { None = 0, Button, Analog } type = None;
        WORD  btn = 0;
        Axis  axis = Axis::LXPos;
        float threshold = 0.5f; // Axis用(0..1) ※LT/RTは通常0.2~0.3程度がおすすめ
    };

    MelonPrimeXInputFilter();
    ~MelonPrimeXInputFilter();

    void setUserIndex(DWORD idx) noexcept { m_user = (idx <= 3) ? idx : 0; }
    void setDeadzone(short left = 7849, short right = 8689, BYTE trig = 30) noexcept { m_dzLeft = left; m_dzRight = right; m_dzTrig = trig; }

    // 毎フレーム一回
    void update() noexcept;

    // バインドAPI
    void bindButton(int hk, WORD xbtn);
    void bindAxisThreshold(int hk, Axis axis, float threshold);
    void clearBinding(int hk);

    // 取得
    bool  hotkeyDown(int hk) const noexcept;
    bool  hotkeyPressed(int hk) noexcept;
    bool  hotkeyReleased(int hk) noexcept;
    float axisValue(Axis a) const noexcept;

    // 震動(任意)
    void rumble(WORD low, WORD high) noexcept;

private:
    // 動的ロード
    using XInputGetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    using XInputSetState_t = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);
    void ensureLoaded() noexcept;
    static float normStick(short v, short dz) noexcept;
    static float normTrig(BYTE  v, BYTE  dz) noexcept;
    bool evalBinding(const Binding& b) const noexcept;

private:
    XInputGetState_t pGetState = nullptr;
    XInputSetState_t pSetState = nullptr;
    HMODULE m_mod = nullptr;
    std::atomic<bool> m_loaded{ false };

    DWORD m_user = 0;
    short m_dzLeft = 7849, m_dzRight = 8689; BYTE m_dzTrig = 30;

    XINPUT_STATE m_now{}, m_prev{};
    std::atomic<bool> m_connected{ false };

    std::unordered_map<int, Binding> m_bind;
    std::array<std::atomic<uint8_t>, 512> m_prevDown{};
};
#endif
