#ifdef _WIN32
#include "MelonPrimeRawInputWinFilter.h"

RawInputWinFilter::RawInputWinFilter()
{
    // RawInput登録（マウス/キーボード）
    m_rid[0] = { 0x01, 0x02, 0, nullptr }; // Mouse
    m_rid[1] = { 0x01, 0x06, 0, nullptr }; // Keyboard
    RegisterRawInputDevices(m_rid, 2, sizeof(RAWINPUTDEVICE));

    // 互換配列をゼロ
    for (auto& a : m_vkDownCompat) a.store(0, std::memory_order_relaxed);
    for (auto& b : m_mbCompat)     b.store(0, std::memory_order_relaxed);

    // エッジ状態ゼロ
    for (auto& w : m_hkPrev) w.store(0, std::memory_order_relaxed);

    // 事前計算マスクをゼロ（AVXなし・移植性優先）
    std::memset(m_hkMask.data(), 0, sizeof(m_hkMask));
}

RawInputWinFilter::~RawInputWinFilter()
{
    // 登録解除
    m_rid[0].dwFlags = RIDEV_REMOVE; m_rid[0].hwndTarget = nullptr;
    m_rid[1].dwFlags = RIDEV_REMOVE; m_rid[1].hwndTarget = nullptr;
    RegisterRawInputDevices(m_rid, 2, sizeof(RAWINPUTDEVICE));
}

bool RawInputWinFilter::nativeEventFilter(const QByteArray& /*eventType*/, void* message, qintptr* /*result*/)
{
    MSG* msg = static_cast<MSG*>(message);
    if (!msg || msg->message != WM_INPUT) return false;

    UINT size = sizeof(m_rawBuf);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT, m_rawBuf, &size,
        sizeof(RAWINPUTHEADER)) == (UINT)-1) {
        return false;
    }

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(m_rawBuf);
    const DWORD type = raw->header.dwType;

    // ---- Mouse first (頻度高) ----
    if (type == RIM_TYPEMOUSE) {
        const RAWMOUSE& m = raw->data.mouse;

        // 相対移動
        const LONG dx_ = m.lLastX;
        const LONG dy_ = m.lLastY;
        if ((dx_ | dy_) != 0) {
            dx.fetch_add((int)dx_, std::memory_order_relaxed);
            dy.fetch_add((int)dy_, std::memory_order_relaxed);
        }

        const USHORT f = m.usButtonFlags;
        if ((f & kAllMouseBtnMask) == 0) return false;

        // down/up を5bitにパック（分岐最小化）
        const uint8_t downMask =
            ((f & 0x0001) ? 0x01 : 0) | ((f & 0x0004) ? 0x02 : 0) |
            ((f & 0x0010) ? 0x04 : 0) | ((f & 0x0040) ? 0x08 : 0) |
            ((f & 0x0100) ? 0x10 : 0);

        const uint8_t upMask =
            ((f & 0x0002) ? 0x01 : 0) | ((f & 0x0008) ? 0x02 : 0) |
            ((f & 0x0020) ? 0x04 : 0) | ((f & 0x0080) ? 0x08 : 0) |
            ((f & 0x0200) ? 0x10 : 0);

        // 現在のマスクを1発更新
        const uint8_t cur = m_state.mouseButtons.load(std::memory_order_relaxed);
        const uint8_t nxt = (uint8_t)((cur | downMask) & (uint8_t)~upMask);
        m_state.mouseButtons.store(nxt, std::memory_order_relaxed);

        // 互換配列（必要なら）
        if (downMask | upMask) {
            if (downMask & 0x01) m_mbCompat[kMB_Left].store(1, std::memory_order_relaxed);
            if (upMask & 0x01)   m_mbCompat[kMB_Left].store(0, std::memory_order_relaxed);
            if (downMask & 0x02) m_mbCompat[kMB_Right].store(1, std::memory_order_relaxed);
            if (upMask & 0x02)   m_mbCompat[kMB_Right].store(0, std::memory_order_relaxed);
            if (downMask & 0x04) m_mbCompat[kMB_Middle].store(1, std::memory_order_relaxed);
            if (upMask & 0x04)   m_mbCompat[kMB_Middle].store(0, std::memory_order_relaxed);
            if (downMask & 0x08) m_mbCompat[kMB_X1].store(1, std::memory_order_relaxed);
            if (upMask & 0x08)   m_mbCompat[kMB_X1].store(0, std::memory_order_relaxed);
            if (downMask & 0x10) m_mbCompat[kMB_X2].store(1, std::memory_order_relaxed);
            if (upMask & 0x10)   m_mbCompat[kMB_X2].store(0, std::memory_order_relaxed);
        }

        return false;
    }
    // ---- Keyboard ----
    else if (type == RIM_TYPEKEYBOARD) {
        const RAWKEYBOARD& kb = raw->data.keyboard;
        UINT vk = kb.VKey;
        const USHORT flags = kb.Flags;
        const bool isUp = (flags & RI_KEY_BREAK) != 0;

        // Shift/Control/Alt の正規化
        switch (vk) {
        case VK_SHIFT:
            vk = MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX);
            break;
        case VK_CONTROL:
            vk = (flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;
            break;
        case VK_MENU:
            vk = (flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;
            break;
        default:
            break;
        }

        // ビットベクタ更新
        setVkBit(vk, !isUp);

        // 互換配列（必要なら）
        if (vk < m_vkDownCompat.size())
            m_vkDownCompat[vk].store(!isUp, std::memory_order_relaxed);

        return false;
    }

    // HID等は無視
    return false;
}

void RawInputWinFilter::fetchMouseDelta(int& outDx, int& outDy)
{
    outDx = dx.exchange(0, std::memory_order_relaxed);
    outDy = dy.exchange(0, std::memory_order_relaxed);
}

void RawInputWinFilter::discardDeltas()
{
    (void)dx.exchange(0, std::memory_order_relaxed);
    (void)dy.exchange(0, std::memory_order_relaxed);
}

void RawInputWinFilter::resetAllKeys()
{
    // AVXなし：各アトミックをゼロ化
    for (auto& w : m_state.vkDown) w.store(0, std::memory_order_relaxed);
    for (auto& a : m_vkDownCompat) a.store(0, std::memory_order_relaxed);
}

void RawInputWinFilter::resetMouseButtons()
{
    m_state.mouseButtons.store(0, std::memory_order_relaxed);
    for (auto& b : m_mbCompat) b.store(0, std::memory_order_relaxed);
}

void RawInputWinFilter::resetHotkeyEdges()
{
    for (auto& w : m_hkPrev) w.store(0, std::memory_order_relaxed);
}

// ---- 前計算マスク構築 ----
FORCE_INLINE void RawInputWinFilter::addVkToMask(HotkeyMask& m, UINT vk) noexcept
{
    if (vk < 8) {
        const uint8_t b = kMouseButtonLUT[vk];
        if (b < 5) { m.mouseMask |= (1u << b); m.hasMask = 1; }
    }
    else if (vk < 256) {
        m.vkMask[vk >> 6] |= (1ULL << (vk & 63));
        m.hasMask = 1;
    }
}

void RawInputWinFilter::setHotkeyVks(int hk, const std::vector<UINT>& vks)
{
    if ((unsigned)hk < kMaxHotkeyId) {
        HotkeyMask& m = m_hkMask[hk];
        std::memset(&m, 0, sizeof(m)); // AVXなし
        const size_t n = (vks.size() > 8) ? 8 : vks.size(); // 8個までで十分
        for (size_t i = 0; i < n; ++i) addVkToMask(m, vks[i]);
        return;
    }
    // 大きいIDはフォールバックに
    m_hkToVk[hk] = vks;
}

// ---- 取得 ----
bool RawInputWinFilter::hotkeyDown(int hk) const noexcept
{
    // 1) 高速パス：前計算マスク
    if ((unsigned)hk < kMaxHotkeyId) {
        const HotkeyMask& m = m_hkMask[hk];
        if (m.hasMask) {
            const bool vkHit =
                ((m_state.vkDown[0].load(std::memory_order_relaxed) & m.vkMask[0]) |
                    (m_state.vkDown[1].load(std::memory_order_relaxed) & m.vkMask[1]) |
                    (m_state.vkDown[2].load(std::memory_order_relaxed) & m.vkMask[2]) |
                    (m_state.vkDown[3].load(std::memory_order_relaxed) & m.vkMask[3])) != 0ULL;

            const bool mouseHit =
                (m_state.mouseButtons.load(std::memory_order_relaxed) & m.mouseMask) != 0u;

            if (vkHit | mouseHit) return true;
        }
    }

    // 2) フォールバック（登録が巨大/特殊な場合のみ）
    auto it = m_hkToVk.find(hk);
    if (it != m_hkToVk.end()) {
        for (UINT vk : it->second) {
            if (vk < 8) {
                const uint8_t b = kMouseButtonLUT[vk];
                if (b < 5 && getMouseButton(b)) return true;
            }
            else if (getVkState(vk)) {
                return true;
            }
        }
    }
    return false;
}

bool RawInputWinFilter::hotkeyPressed(int hk) noexcept
{
    const bool now = hotkeyDown(hk);

    if ((unsigned)hk < kMaxHotkeyId) {
        const size_t idx = ((unsigned)hk) >> 6;
        const uint64_t bit = 1ULL << (hk & 63);
        if (now) {
            const uint64_t prev = m_hkPrev[idx].fetch_or(bit, std::memory_order_acq_rel);
            return (prev & bit) == 0;
        }
        else {
            const uint64_t prev = m_hkPrev[idx].fetch_and(~bit, std::memory_order_acq_rel);
            (void)prev;
            return false;
        }
    }

    // 大きいIDは簡易フォールバック
    static std::array<std::atomic<uint8_t>, 1024> s{};
    const size_t i = ((unsigned)hk) & 1023;
    const uint8_t prev = s[i].exchange(now ? 1u : 0u, std::memory_order_acq_rel);
    return now && !prev;
}

bool RawInputWinFilter::hotkeyReleased(int hk) noexcept
{
    const bool now = hotkeyDown(hk);

    if ((unsigned)hk < kMaxHotkeyId) {
        const size_t idx = ((unsigned)hk) >> 6;
        const uint64_t bit = 1ULL << (hk & 63);
        if (!now) {
            const uint64_t prev = m_hkPrev[idx].fetch_and(~bit, std::memory_order_acq_rel);
            return (prev & bit) != 0;
        }
        else {
            (void)m_hkPrev[idx].fetch_or(bit, std::memory_order_acq_rel);
            return false;
        }
    }

    // 大きいIDは簡易フォールバック
    static std::array<std::atomic<uint8_t>, 1024> s{};
    const size_t i = ((unsigned)hk) & 1023;
    const uint8_t prev = s[i].exchange(now ? 1u : 0u, std::memory_order_acq_rel);
    return (!now) && prev;
}

#endif // _WIN32
