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

    // Use AVX for faster initialization if available
#ifdef __AVX2__
    const __m256i zero = _mm256_setzero_si256();

    // Initialize m_vkDown
    __m256i* vkPtr = reinterpret_cast<__m256i*>(m_vkDown.data());
    for (size_t i = 0; i < sizeof(m_vkDown) / 32; ++i) {
        _mm256_store_si256(vkPtr + i, zero);
    }

    // Initialize m_hkPrev
    __m256i* hkPtr = reinterpret_cast<__m256i*>(m_hkPrev.data());
    for (size_t i = 0; i < sizeof(m_hkPrev) / 32; ++i) {
        _mm256_store_si256(hkPtr + i, zero);
    }
#else
    std::memset(m_vkDown.data(), 0, sizeof(m_vkDown));
    std::memset(m_hkPrev.data(), 0, sizeof(m_hkPrev));
#endif

    std::memset(m_mb.data(), 0, sizeof(m_mb));

    dx.store(0, std::memory_order_relaxed);
    dy.store(0, std::memory_order_relaxed);

    // if (!m_joyHK) m_joyHK = &kEmptyMask;
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
    if (Q_UNLIKELY(!msg)) [[unlikely]] return false;
    if (Q_LIKELY(msg->message != WM_INPUT)) [[likely]] return false;

    // 期待バッファサイズ（RAWINPUT固定）
    UINT size = sizeof(RAWINPUT);

    // 失敗なら即リターン（処理を増やさない）
    const UINT got = GetRawInputData(
        reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT,
        m_rawBuf,
        &size,
        sizeof(RAWINPUTHEADER));
    if (Q_UNLIKELY(got == static_cast<UINT>(-1))) [[unlikely]] return false;

    const RAWINPUT* const raw = reinterpret_cast<const RAWINPUT*>(m_rawBuf);
    const DWORD type = raw->header.dwType;

    // 最頻出のマウスを先に
    if (Q_LIKELY(type == RIM_TYPEMOUSE)) [[likely]] {
        const RAWMOUSE& m = raw->data.mouse;

        // 相対移動（0判定は OR で一発）
        const LONG dxv = m.lLastX;
        const LONG dyv = m.lLastY;
        if (Q_LIKELY((dxv | dyv) != 0)) [[likely]] {
            // アトミック書き込みのキャッシュ行を先読み
            _mm_prefetch(reinterpret_cast<const char*>(&dx), _MM_HINT_T0);
            _mm_prefetch(reinterpret_cast<const char*>(&dy), _MM_HINT_T0);
            dx.fetch_add(dxv, std::memory_order_relaxed);
            dy.fetch_add(dyv, std::memory_order_relaxed);
        }

        // ボタン（関係ないフラグなら即抜け）
        const USHORT f = m.usButtonFlags;
        const USHORT rf = (f & kAllMouseBtnMask);
        if (Q_LIKELY(rf == 0)) [[likely]] return false;

        // 手動アンローリング（分岐最小化）
        if (rf & (RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP))
            m_mb[0].store((f & RI_MOUSE_LEFT_BUTTON_DOWN) ? 1u : 0u, std::memory_order_relaxed);
        if (rf & (RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP))
            m_mb[1].store((f & RI_MOUSE_RIGHT_BUTTON_DOWN) ? 1u : 0u, std::memory_order_relaxed);
        if (rf & (RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP))
            m_mb[2].store((f & RI_MOUSE_MIDDLE_BUTTON_DOWN) ? 1u : 0u, std::memory_order_relaxed);
        if (rf & (RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP))
            m_mb[3].store((f & RI_MOUSE_BUTTON_4_DOWN) ? 1u : 0u, std::memory_order_relaxed);
        if (rf & (RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP))
            m_mb[4].store((f & RI_MOUSE_BUTTON_5_DOWN) ? 1u : 0u, std::memory_order_relaxed);

        return false;
    }

    // キーボード（頻度は低いので unlikely）
    if (Q_UNLIKELY(type == RIM_TYPEKEYBOARD)) [[unlikely]] {
        const RAWKEYBOARD& kb = raw->data.keyboard;
        UINT vk = kb.VKey;
        const USHORT flags = kb.Flags;
        const bool isUp = (flags & RI_KEY_BREAK) != 0;

        // 特殊キーを最短で正規化
        if (Q_UNLIKELY(vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU)) [[unlikely]] {
            switch (vk) {
            case VK_SHIFT:   vk = MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX); break;
            case VK_CONTROL: vk = (flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL; break;
            case VK_MENU:    vk = (flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;    break;
            }
        }

        if (Q_LIKELY(vk < m_vkDown.size())) [[likely]] {
            _mm_prefetch(reinterpret_cast<const char*>(&m_vkDown[vk]), _MM_HINT_T0);
            m_vkDown[vk].store(static_cast<uint8_t>(!isUp), std::memory_order_relaxed);
        }
    }

    return false;
#else
    Q_UNUSED(message);
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
    // 1) Raw(KB/Mouse)
    const HotkeyMapping* m = findHotkey(hk);
    if (Q_LIKELY(m)) {
        const UINT* p = m->vks;
        const UINT* e = p + m->vkCount;

        // 単体バインド：押下なら即 return true、未押下ならフォールスルーで Joy を見る
        if (Q_LIKELY(p + 1 == e)) {
            const UINT vk = *p;
            bool down = false;

            if (vk < 8) {
                const uint8_t idx = kMouseButtonLUT[vk];
                if (idx != 0xFF) down = (m_mb[idx].load(std::memory_order_relaxed) != 0);
            }
            else if (vk < m_vkDown.size()) {
                down = (m_vkDown[vk].load(std::memory_order_relaxed) != 0);
            }

            if (down) return true;
        }
        else {
            // 複数バインド：どれか押下で即 return、未押下ならフォールスルー
            for (; p != e; ++p) {
                const UINT vk = *p;

                if (vk < 8) {
                    const uint8_t idx = kMouseButtonLUT[vk];
                    if (idx != 0xFF && m_mb[idx].load(std::memory_order_relaxed)) return true;
                }
                else if (vk < m_vkDown.size() && m_vkDown[vk].load(std::memory_order_relaxed)) {
                    return true;
                }
            }
        }
    }
	return false;
    /*
    // このコメントは絶対に消さないこと。 RawとJoystickは必ず両方見る必要がある。
    // 2) Joystick mask（EmuInstance が毎フレ更新）
    // m_joyHK 非null前提ならこのままでOK。万一の安全策を入れるなら kEmptyMask を参照させる実装に。
    const QBitArray* jm = m_joyHK;
    const int n = jm->size();
    return (static_cast<unsigned>(hk) < static_cast<unsigned>(n)) && jm->testBit(hk);
    */
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