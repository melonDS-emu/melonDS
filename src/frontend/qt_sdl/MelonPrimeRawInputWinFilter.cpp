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

    // メモリ初期化をより高速に
    std::memset(m_vkDown.data(), 0, sizeof(m_vkDown));
    std::memset(m_mb.data(), 0, sizeof(m_mb));
    std::memset(m_hkPrev.data(), 0, sizeof(m_hkPrev));

    dx.store(0, std::memory_order_relaxed);
    dy.store(0, std::memory_order_relaxed);

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
    MSG* const msg = static_cast<MSG*>(message);

    // 早期リターンで分岐予測を改善
    if (Q_UNLIKELY(!msg)) [[unlikely]] return false;
    if (Q_LIKELY(msg->message != WM_INPUT)) [[likely]] return false;

    // バッファサイズを事前計算
    constexpr UINT expectedSize = sizeof(RAWINPUT);
    UINT size = expectedSize;

    // GetRawInputDataの呼び出し最適化
    const UINT result = GetRawInputData(
        reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT,
        m_rawBuf,
        &size,
        sizeof(RAWINPUTHEADER)
    );

    if (Q_UNLIKELY(result == static_cast<UINT>(-1))) [[unlikely]] return false;

    const RAWINPUT* const raw = reinterpret_cast<const RAWINPUT*>(m_rawBuf);
    const DWORD deviceType = raw->header.dwType;

    // 最も頻繁なケースを最初に処理
    if (Q_LIKELY(deviceType == RIM_TYPEMOUSE)) [[likely]] {
        const RAWMOUSE& mouse = raw->data.mouse;

        // マウス移動の処理（最適化）
        const LONG deltaX = mouse.lLastX;
        const LONG deltaY = mouse.lLastY;

        // ビット演算で0チェックを高速化
        if (Q_LIKELY((deltaX | deltaY) != 0)) [[likely]] {
            dx.fetch_add(deltaX, std::memory_order_relaxed);
            dy.fetch_add(deltaY, std::memory_order_relaxed);
        }

        const USHORT buttonFlags = mouse.usButtonFlags;
        const USHORT relevantFlags = buttonFlags & kAllMouseBtnMask;

        // 関係ないイベントは即座にスキップ
        if (Q_LIKELY(relevantFlags == 0)) [[likely]] return false;

        // ボタン処理を展開ループで最適化
        constexpr auto& maps = kButtonMaps;

        // コンパイラの最適化を促進
#pragma GCC unroll 5
        for (size_t i = 0; i < 5; ++i) {
            const auto& map = maps[i];
            const USHORT mask = map.down | map.up;

            if (Q_UNLIKELY(relevantFlags & mask)) [[unlikely]] {
                const uint8_t state = (buttonFlags & map.down) ? 1u : 0u;
                m_mb[map.idx].store(state, std::memory_order_relaxed);
            }
        }
    }
    else if (Q_UNLIKELY(deviceType == RIM_TYPEKEYBOARD)) [[unlikely]] {
        const RAWKEYBOARD& keyboard = raw->data.keyboard;
        UINT virtualKey = keyboard.VKey;
        const USHORT flags = keyboard.Flags;
        const bool isKeyUp = (flags & RI_KEY_BREAK) != 0;

        // 修飾キーの正規化（分岐予測を改善）
        switch (virtualKey) {
        case VK_SHIFT:
            virtualKey = MapVirtualKey(keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);
            break;
        case VK_CONTROL:
            virtualKey = (flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;
            break;
        case VK_MENU:
            virtualKey = (flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;
            break;
        default:
            break;
        }

        // 境界チェックを最適化
        if (Q_LIKELY(virtualKey < m_vkDown.size())) [[likely]] {
            const uint8_t state = static_cast<uint8_t>(!isKeyUp);
            m_vkDown[virtualKey].store(state, std::memory_order_relaxed);
        }
    }

    return false;
#else
    return false;
#endif
}

void RawInputWinFilter::resetAllKeys() noexcept
{
    // SIMD使用を検討（AVX2が使える場合）
#ifdef __AVX2__
    const __m256i zero = _mm256_setzero_si256();
    uint8_t* ptr = reinterpret_cast<uint8_t*>(m_vkDown.data());

    for (size_t i = 0; i < sizeof(m_vkDown); i += 32) {
        _mm256_store_si256(reinterpret_cast<__m256i*>(ptr + i), zero);
    }
#else
    std::memset(m_vkDown.data(), 0, sizeof(m_vkDown));
#endif
}

void RawInputWinFilter::resetMouseButtons() noexcept
{
    std::memset(m_mb.data(), 0, sizeof(m_mb));
}

void RawInputWinFilter::setHotkeyVks(int hk, const std::vector<UINT>& vks)
{
    m_hkToVk[hk] = vks;
}

bool RawInputWinFilter::hotkeyDown(int hk) const noexcept
{
    // 1) キーボード・マウス状態チェック（最適化）
    const auto it = m_hkToVk.find(hk);
    if (Q_LIKELY(it != m_hkToVk.end())) [[likely]] {
        const auto& virtualKeys = it->second;

        // 小さなベクタの場合は展開ループを使用
        for (const UINT vk : virtualKeys) {
            if (Q_UNLIKELY(vk <= VK_XBUTTON2)) [[unlikely]] {
                // マウスボタンチェック（lookup tableを使用）
                static constexpr uint8_t mouseButtonMap[] = {
                    255, // VK_NULL
                    kMB_Left,   // VK_LBUTTON
                    kMB_Right,  // VK_RBUTTON
                    255,        // VK_CANCEL
                    kMB_Middle, // VK_MBUTTON
                    kMB_X1,     // VK_XBUTTON1
                    kMB_X2,     // VK_XBUTTON2
                };

                if (vk < sizeof(mouseButtonMap)) {
                    const uint8_t mbIndex = mouseButtonMap[vk];
                    if (mbIndex != 255 && m_mb[mbIndex].load(std::memory_order_relaxed)) {
                        return true;
                    }
                }
            }
            else if (Q_LIKELY(vk < m_vkDown.size())) [[likely]] {
                if (Q_UNLIKELY(m_vkDown[vk].load(std::memory_order_relaxed))) [[unlikely]] {
                    return true;
                }
            }
        }
    }

    // 2) ジョイスティック状態チェック
    const QBitArray* const joyMask = m_joyHK;
    const int maskSize = joyMask->size();

    if (Q_LIKELY(static_cast<unsigned>(hk) < static_cast<unsigned>(maskSize))) [[likely]] {
        return joyMask->testBit(hk);
    }

    return false;
}

bool RawInputWinFilter::hotkeyPressed(int hk) noexcept
{
    const bool currentState = hotkeyDown(hk);
    const size_t index = static_cast<size_t>(hk) & 511;

    // atomic exchange を使って前の状態を取得
    const uint8_t previousState = m_hkPrev[index].exchange(
        static_cast<uint8_t>(currentState),
        std::memory_order_acq_rel
    );

    // 立ち上がりエッジ検出
    return currentState && !previousState;
}

bool RawInputWinFilter::hotkeyReleased(int hk) noexcept
{
    const bool currentState = hotkeyDown(hk);
    const size_t index = static_cast<size_t>(hk) & 511;

    // atomic exchange を使って前の状態を取得
    const uint8_t previousState = m_hkPrev[index].exchange(
        static_cast<uint8_t>(currentState),
        std::memory_order_acq_rel
    );

    // 立ち下がりエッジ検出
    return !currentState && previousState;
}