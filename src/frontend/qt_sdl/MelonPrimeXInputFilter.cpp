#ifdef _WIN32
#include "MelonPrimeXInputFilter.h"
#include <cstring>

static HMODULE LoadOne(const wchar_t* dll) {
    HMODULE m = ::LoadLibraryW(dll);
    return m;
}

MelonPrimeXInputFilter::MelonPrimeXInputFilter() {
    for (auto& a : m_prevDown) a.store(0, std::memory_order_relaxed);
}

MelonPrimeXInputFilter::~MelonPrimeXInputFilter() {
    if (m_mod) {
        FreeLibrary(m_mod);
        m_mod = nullptr;
    }
}

void MelonPrimeXInputFilter::ensureLoaded() noexcept {
    if (m_loaded.load(std::memory_order_acquire)) return;

    // óDêÊèá: xinput1_4 Å® 1_3 Å® 9_1_0
    static const wchar_t* cands[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };
    for (auto* name : cands) {
        m_mod = LoadOne(name);
        if (m_mod) break;
    }
    if (!m_mod) { m_loaded.store(true, std::memory_order_release); return; }

    pGetState = reinterpret_cast<XInputGetState_t>(GetProcAddress(m_mod, "XInputGetState"));
    pSetState = reinterpret_cast<XInputSetState_t>(GetProcAddress(m_mod, "XInputSetState"));
    m_loaded.store(true, std::memory_order_release);
}

float MelonPrimeXInputFilter::normStick(short v, short dz) noexcept {
    const int sv = static_cast<int>(v);
    const int ad = (sv >= 0) ? sv : -sv;
    if (ad <= dz) return 0.0f;
    const float n = (ad - dz) / (32767.0f - dz);
    return (sv >= 0) ? n : -n;
}

float MelonPrimeXInputFilter::normTrig(BYTE v, BYTE dz) noexcept {
    if (v <= dz) return 0.0f;
    return (v - dz) / (255.0f - dz);
}

void MelonPrimeXInputFilter::update() noexcept {
    ensureLoaded();
    m_prev = m_now;
    if (!pGetState) { std::memset(&m_now, 0, sizeof(m_now)); m_connected.store(false, std::memory_order_relaxed); return; }

    XINPUT_STATE st{};
    DWORD r = pGetState(m_user, &st);
    if (r == ERROR_SUCCESS) {
        m_now = st;
        m_connected.store(true, std::memory_order_relaxed);
    }
    else {
        std::memset(&m_now, 0, sizeof(m_now));
        m_connected.store(false, std::memory_order_relaxed);
    }
}

void MelonPrimeXInputFilter::bindButton(int hk, WORD xbtn) {
    Binding b;
    b.type = Binding::Button;
    b.btn = xbtn;
    m_bind[hk] = b;
}

void MelonPrimeXInputFilter::bindAxisThreshold(int hk, Axis a, float th) {
    Binding x;
    x.type = Binding::Analog;          // Åö èCê≥: Binding::Axis Å® Binding::Analog
    x.axis = a;
    x.threshold = th;
    m_bind[hk] = x;
}

void MelonPrimeXInputFilter::clearBinding(int hk) {
    m_bind.erase(hk);
}

bool MelonPrimeXInputFilter::evalBinding(const Binding& b) const noexcept {
    if (b.type == Binding::Button)
        return (m_now.Gamepad.wButtons & b.btn) != 0;
    if (b.type == Binding::Analog)
        return axisValue(b.axis) >= b.threshold;  // Åö èCê≥: Binding::Axis Å® Binding::Analog
    return false;
}

bool MelonPrimeXInputFilter::hotkeyDown(int hk) const noexcept {
    auto it = m_bind.find(hk);
    if (it == m_bind.end()) return false;
    return evalBinding(it->second);
}

bool MelonPrimeXInputFilter::hotkeyPressed(int hk) noexcept {
    const bool d = hotkeyDown(hk);
    auto& p = m_prevDown[static_cast<size_t>(hk) & 511];
    const uint8_t prev = p.exchange(d, std::memory_order_acq_rel);
    return d && !prev;
}

bool MelonPrimeXInputFilter::hotkeyReleased(int hk) noexcept {
    const bool d = hotkeyDown(hk);
    auto& p = m_prevDown[static_cast<size_t>(hk) & 511];
    const uint8_t prev = p.exchange(d, std::memory_order_acq_rel);
    return (!d) && prev;
}

float MelonPrimeXInputFilter::axisValue(Axis a) const noexcept {
    const auto& g = m_now.Gamepad;
    switch (a) {
    case Axis::LXPos: return std::max(0.0f, normStick(g.sThumbLX, m_dzLeft));
    case Axis::LXNeg: return std::max(0.0f, -normStick(g.sThumbLX, m_dzLeft));
    case Axis::LYPos: return std::max(0.0f, normStick(g.sThumbLY, m_dzLeft));
    case Axis::LYNeg: return std::max(0.0f, -normStick(g.sThumbLY, m_dzLeft));
    case Axis::RXPos: return std::max(0.0f, normStick(g.sThumbRX, m_dzRight));
    case Axis::RXNeg: return std::max(0.0f, -normStick(g.sThumbRX, m_dzRight));
    case Axis::RYPos: return std::max(0.0f, normStick(g.sThumbRY, m_dzRight));
    case Axis::RYNeg: return std::max(0.0f, -normStick(g.sThumbRY, m_dzRight));
    case Axis::LT:    return normTrig(g.bLeftTrigger, m_dzTrig);
    case Axis::RT:    return normTrig(g.bRightTrigger, m_dzTrig);
    }
    return 0.0f;
}

void MelonPrimeXInputFilter::rumble(WORD low, WORD high) noexcept {
    ensureLoaded();
    if (!pSetState) return;
    XINPUT_VIBRATION v{};
    v.wLeftMotorSpeed = low;
    v.wRightMotorSpeed = high;
    pSetState(m_user, &v);
}
#endif
