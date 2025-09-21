#include "MelonPrimeRawInputWinFilter.h"
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <hidsdi.h>
#endif

RawInputWinFilter::RawInputWinFilter()
{
    rid[0] = { 0x01, 0x02, 0, nullptr }; // mouse
    rid[1] = { 0x01, 0x06, 0, nullptr }; // keyboard
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));

    for (auto& a : m_vkDown) a.store(0, std::memory_order_relaxed);
    for (auto& b : m_mb)     b.store(0, std::memory_order_relaxed);
    dx.store(0, std::memory_order_relaxed);
    dy.store(0, std::memory_order_relaxed);

    // joyMask未設定でも安全に読めるようにしておく
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
    MSG* msg = static_cast<MSG*>(message);
    if (Q_LIKELY(!msg || msg->message != WM_INPUT)) return false;

    // ★ クラス内事前確保バッファを使用（毎回のスタック確保/サイズ計算回避）
    UINT size = sizeof(m_rawBuf);
    if (Q_UNLIKELY(GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT, m_rawBuf, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)) {
        return false;
    }

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(m_rawBuf);
    const DWORD t = raw->header.dwType;

    if (Q_LIKELY(t == RIM_TYPEMOUSE)) {
        const RAWMOUSE& m = raw->data.mouse;

        const LONG dx_ = m.lLastX;
        const LONG dy_ = m.lLastY;
        if (Q_LIKELY((dx_ | dy_) != 0)) {
            dx.fetch_add(dx_, std::memory_order_relaxed);
            dy.fetch_add(dy_, std::memory_order_relaxed);
        }

        const USHORT flags = m.usButtonFlags;

        // ★ ボタン系に関係ない（例: ホイールのみ等）なら即スキップ
        const USHORT relevant = static_cast<USHORT>(flags & kAllMouseBtnMask);
        if (Q_LIKELY(relevant == 0)) return false;

        // マッピング配列はconstexprで最適化
        static constexpr struct { USHORT d, u; size_t idx; } map[] = {
            {RI_MOUSE_LEFT_BUTTON_DOWN,   RI_MOUSE_LEFT_BUTTON_UP,   kMB_Left},
            {RI_MOUSE_RIGHT_BUTTON_DOWN,  RI_MOUSE_RIGHT_BUTTON_UP,  kMB_Right},
            {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, kMB_Middle},
            {RI_MOUSE_BUTTON_4_DOWN,      RI_MOUSE_BUTTON_4_UP,      kMB_X1},
            {RI_MOUSE_BUTTON_5_DOWN,      RI_MOUSE_BUTTON_5_UP,      kMB_X2},
        };

        // ループは短いのでそのまま：分岐はrelevantに限定
        for (const auto& e : map) {
            const USHORT mask = static_cast<USHORT>(e.d | e.u);
            if (Q_UNLIKELY(relevant & mask)) {
                m_mb[e.idx].store((flags & e.d) ? 1u : 0u, std::memory_order_relaxed);
            }
        }
    }
    else if (Q_UNLIKELY(t == RIM_TYPEKEYBOARD)) {
        const RAWKEYBOARD& kb = raw->data.keyboard;
        UINT vk = kb.VKey;
        const USHORT fl = kb.Flags;
        const bool up = (fl & RI_KEY_BREAK) != 0;

        // 修飾キーは右左を正規化
        switch (vk) {
        case VK_SHIFT:   vk = MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX); break;
        case VK_CONTROL: vk = (fl & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;   break;
        case VK_MENU:    vk = (fl & RI_KEY_E0) ? VK_RMENU : VK_LMENU;      break;
        default: break;
        }

        if (Q_LIKELY(vk < m_vkDown.size()))
            m_vkDown[vk].store(static_cast<uint8_t>(!up), std::memory_order_relaxed);
    }

    return false;
#else
    return false;
#endif
}

void RawInputWinFilter::fetchMouseDelta(int& outDx, int& outDy)
{
    outDx = dx.exchange(0, std::memory_order_relaxed);
    outDy = dy.exchange(0, std::memory_order_relaxed);
}

void RawInputWinFilter::discardDeltas()
{
    dx.exchange(0, std::memory_order_relaxed);
    dy.exchange(0, std::memory_order_relaxed);
}

void RawInputWinFilter::resetAllKeys()
{
    for (auto& a : m_vkDown) a.store(0, std::memory_order_relaxed);
}

void RawInputWinFilter::resetMouseButtons()
{
    for (auto& b : m_mb) b.store(0, std::memory_order_relaxed);
}

void RawInputWinFilter::setHotkeyVks(int hk, const std::vector<UINT>& vks)
{
    m_hkToVk[hk] = vks;
}

bool RawInputWinFilter::hotkeyDown(int hk) const
{
    // 1) Raw(KB/Mouse) 直読
    auto it = m_hkToVk.find(hk);
    if (it != m_hkToVk.end()) {
        const auto& vks = it->second;
        for (UINT vk : vks) {
            if (vk <= VK_XBUTTON2) {
                switch (vk) {
                case VK_LBUTTON:  if (m_mb[kMB_Left].load(std::memory_order_relaxed))  return true; break;
                case VK_RBUTTON:  if (m_mb[kMB_Right].load(std::memory_order_relaxed)) return true; break;
                case VK_MBUTTON:  if (m_mb[kMB_Middle].load(std::memory_order_relaxed))return true; break;
                case VK_XBUTTON1: if (m_mb[kMB_X1].load(std::memory_order_relaxed))    return true; break;
                case VK_XBUTTON2: if (m_mb[kMB_X2].load(std::memory_order_relaxed))    return true; break;
                }
            }
            else if (vk < m_vkDown.size() && m_vkDown[vk].load(std::memory_order_relaxed)) {
                return true;
            }
        }
    }

    // 2) SDL/ジョイスティック側ビット（EmuInstanceが毎フレ更新）
    const QBitArray* jm = m_joyHK ? m_joyHK : &kEmptyMask;
    const int n = jm->size();
    if ((unsigned)hk < (unsigned)n && jm->testBit(hk)) return true;

    return false;
}

bool RawInputWinFilter::hotkeyPressed(int hk) noexcept
{
    const bool d = hotkeyDown(hk);
    auto& p = m_hkPrev[(size_t)hk & 511];
    const uint8_t prev = p.exchange(d, std::memory_order_acq_rel);
    return d && !prev;
}

bool RawInputWinFilter::hotkeyReleased(int hk) noexcept
{
    const bool d = hotkeyDown(hk);
    auto& p = m_hkPrev[(size_t)hk & 511];
    const uint8_t prev = p.exchange(d, std::memory_order_acq_rel);
    return (!d) && prev;
}
