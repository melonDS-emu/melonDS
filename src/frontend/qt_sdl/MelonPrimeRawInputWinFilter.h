#pragma once
#ifdef _WIN32

// Qt
#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QBitArray>
#include <QtGlobal>

// Win32
#include <windows.h>
#include <hidsdi.h>

// C++/STL
#include <array>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <cstdint>
#include <cstring>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

// 強制インラインヒント
#ifndef FORCE_INLINE
#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE __attribute__((always_inline)) inline
#endif
#endif

// SSE/AVX を使わないビルドでも安全に動作するように条件分岐
// （AVX2がある場合のみストリームストア等を使用）

class RawInputWinFilter : public QAbstractNativeEventFilter
{
public:
    RawInputWinFilter();
    ~RawInputWinFilter() override;

    // Qt ネイティブイベントフック
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    // マウス相対デルタ取得（取り出し時にゼロクリア）
    void fetchMouseDelta(int& outDx, int& outDy);

    // 累積デルタの破棄
    void discardDeltas();

    // 全キー（キーボード）を未押下に
    void resetAllKeys();

    // 全マウスボタンを未押下に
    void resetMouseButtons();

    // 全ホットキーの立下り/立上りエッジ状態をリセット
    void resetHotkeyEdges();

    // HK→VK の登録（キーボード/マウス側の前計算マスクを作る）
    void setHotkeyVks(int hk, const std::vector<UINT>& vks);

    // SDL 側（EmuInstance）が毎フレ更新する joyHotkeyMask を参照させる
    // （常に有効を渡す前提ならnullチェック不要）
    //FORCE_INLINE void setJoyHotkeyMaskPtr(const QBitArray* p) noexcept { m_joyHK = p ? p : &kEmptyMask; }

    // 取得系
    bool hotkeyDown(int hk) const noexcept;
    bool hotkeyPressed(int hk) noexcept;
    bool hotkeyReleased(int hk) noexcept;

private:
    // --------- 内部型と定数 ---------
    enum MouseIndex : uint8_t { kMB_Left = 0, kMB_Right, kMB_Middle, kMB_X1, kMB_X2, kMB_Max };

    // VK(0..255) 用 256bit と、Mouse(5bit) の前計算マスク
    struct alignas(16) HotkeyMask {
        uint64_t vkMask[4];  // 256bit = 4 * 64bit
        uint8_t  mouseMask;  // L/R/M/X1/X2 -> 5bit
        uint8_t  hasMask;    // 0:空, 1:有効
        uint8_t  _pad[6];
    };

    static constexpr size_t kMaxHotkeyId = 256;

    // すべてのボタンフラグ（早期リターン用）
    static constexpr USHORT kAllMouseBtnMask =
        RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP |
        RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP |
        RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP |
        RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP |
        RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP;

    // Win32 VK_* → MouseIndex の簡易LUT（0..7だけ使用）
    static constexpr uint8_t kMouseButtonLUT[8] = {
        0xFF,       // 0 (unused)
        kMB_Left,   // VK_LBUTTON   (1)
        kMB_Right,  // VK_RBUTTON   (2)
        0xFF,       // 3
        kMB_Middle, // VK_MBUTTON   (4)
        kMB_X1,     // VK_XBUTTON1  (5)
        kMB_X2,     // VK_XBUTTON2  (6)
        0xFF        // 7
    };

    // イベント処理用の小バッファ（HIDは扱わないので十分）
    alignas(64) BYTE m_rawBuf[256] = {};

    // キー/マウス実時間状態（低コスト参照のためビットベクタ化）
    struct StateBits {
        std::atomic<uint64_t> vkDown[4];      // 256bit の押下状態
        std::atomic<uint8_t>  mouseButtons;   // 5bit の押下状態（L,R,M,X1,X2）
    } m_state{ {0,0,0,0}, 0 };

    // 低頻度API: VKの現在状態取得
    FORCE_INLINE bool getVkState(uint32_t vk) const noexcept {
        if (vk >= 256) return false;
        const uint64_t w = m_state.vkDown[vk >> 6].load(std::memory_order_relaxed);
        return (w & (1ULL << (vk & 63))) != 0ULL;
    }
    // 低頻度API: Mouseボタン状態
    FORCE_INLINE bool getMouseButton(uint8_t idx) const noexcept {
        const uint8_t b = m_state.mouseButtons.load(std::memory_order_relaxed);
        return (idx < 5) && ((b >> idx) & 1u);
    }
    // 低頻度API: VKセット/クリア
    FORCE_INLINE void setVkBit(uint32_t vk, bool down) noexcept {
        if (vk >= 256) return;
        const uint64_t mask = 1ULL << (vk & 63);
        auto& word = m_state.vkDown[vk >> 6];
        if (down) (void)word.fetch_or(mask, std::memory_order_relaxed);
        else      (void)word.fetch_and(~mask, std::memory_order_relaxed);
    }

    // 前計算済み Hotkey マスク（高速パス用）
    alignas(64) std::array<HotkeyMask, kMaxHotkeyId> m_hkMask{};

    // フォールバック用（大きいHK IDなど）
    std::unordered_map<int, std::vector<UINT>> m_hkToVk;

    // joyHotkeyMask 参照
    //inline static const QBitArray kEmptyMask{};
    //const QBitArray* m_joyHK = &kEmptyMask;

    // 立上り/立下りエッジ検出用（O(1)）
    std::array<std::atomic<uint64_t>, (kMaxHotkeyId + 63) / 64> m_hkPrev{};

    // RawInput 登録情報
    RAWINPUTDEVICE m_rid[2]{};

    // 相対デルタ
    std::atomic<int> dx{ 0 }, dy{ 0 };

    // 古い実装との互換（必要なら残す）
    std::array<std::atomic<int>, 256> m_vkDownCompat{};
    std::array<std::atomic<int>, kMB_Max> m_mbCompat{};

    // ヘルパ
    static FORCE_INLINE void addVkToMask(HotkeyMask& m, UINT vk) noexcept;
};

#endif // _WIN32
