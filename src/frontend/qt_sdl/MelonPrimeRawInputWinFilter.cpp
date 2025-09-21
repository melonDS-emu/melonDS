// ヘッダ参照宣言(クラス定義とシグネチャ一致のため)
#include "MelonPrimeRawInputWinFilter.h"
// C標準入出力参照宣言(デバッグ時printf利用のため)
#include <cstdio>
// アルゴリズム参照宣言(補助ユーティリティのため)
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <hidsdi.h>
#include <QBitArray>
#endif



///**
/// * コンストラクタ定義.
/// *
/// * RawInput登録と内部状態初期化を行う.
/// */
 // メンバ関数本体定義(初期化処理のため)
RawInputWinFilter::RawInputWinFilter()
{
    // マウスデバイス登録設定処理(usage 0x01/0x02指定のため)
    rid[0] = { 0x01, 0x02, 0, nullptr };
    // キーボードデバイス登録設定処理(usage 0x01/0x06指定のため)
    rid[1] = { 0x01, 0x06, 0, nullptr };

    // デバイス登録呼出処理(受信開始のため)
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));

    // キー配列初期化処理(誤検知防止のため)
    for (auto& a : m_vkDown) a.store(0, std::memory_order_relaxed);
    // マウス配列初期化処理(誤検知防止のため)
    for (auto& b : m_mb)     b.store(0, std::memory_order_relaxed);
    // 相対デルタ初期化処理(残差排除のため)
    dx.store(0, std::memory_order_relaxed);
    // 相対デルタ初期化処理(残差排除のため)
    dy.store(0, std::memory_order_relaxed);
}

///**
/// * デストラクタ定義.
/// *
/// * RawInput登録解除を行う.
/// */
 // メンバ関数本体定義(後始末処理のため)
RawInputWinFilter::~RawInputWinFilter()
{
    // マウス解除指定設定処理(登録解除のため)
    rid[0].dwFlags = RIDEV_REMOVE; rid[0].hwndTarget = nullptr;
    // キーボード解除指定設定処理(登録解除のため)
    rid[1].dwFlags = RIDEV_REMOVE; rid[1].hwndTarget = nullptr;
    // 登録解除呼出処理(クリーンアップのため)
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
}


/**
 * ネイティブイベントフィルタ定義（低サイクル最適化版）
 *
 * 最適化ポイント:
 * 1. 早期リターンによる不要な処理スキップ
 * 2. ルックアップテーブルによる条件分岐削減
 * 3. ビット演算の最適化
 * 4. キャッシュ局所性の向上
 */
 /**
  * ネイティブイベントフィルタ定義（低サイクル最適化版）
  *
  * 最適化ポイント:
  * 1. 早期リターンによる不要な処理スキップ
  * 2. ルックアップテーブルによる条件分岐削減
  * 3. ビット演算の最適化
  * 4. キャッシュ局所性の向上
  */
  ///**
  /// * ネイティブイベントフィルタ定義.
  /// *
  /// * WM_INPUTからマウス/キーボード状態を収集する.
  /// */
bool RawInputWinFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef _WIN32
    MSG* msg = static_cast<MSG*>(message);
    if (!msg) return false;

    // RawInput
    if (msg->message == WM_INPUT) {
        alignas(8) BYTE buffer[sizeof(RAWINPUT)];
        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
        UINT size = sizeof(RAWINPUT);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam),
            RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
            return false;
        }

        if (raw->header.dwType == RIM_TYPEMOUSE) {
            const auto& m = raw->data.mouse;
            dx.fetch_add(m.lLastX, std::memory_order_relaxed);
            dy.fetch_add(m.lLastY, std::memory_order_relaxed);
            const USHORT f = m.usButtonFlags;
            if (f & (RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP)) m_mb[kMB_Left].store((f & RI_MOUSE_LEFT_BUTTON_DOWN) ? 1 : 0, std::memory_order_relaxed);
            if (f & (RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP)) m_mb[kMB_Right].store((f & RI_MOUSE_RIGHT_BUTTON_DOWN) ? 1 : 0, std::memory_order_relaxed);
            if (f & (RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP)) m_mb[kMB_Middle].store((f & RI_MOUSE_MIDDLE_BUTTON_DOWN) ? 1 : 0, std::memory_order_relaxed);
            if (f & (RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP)) m_mb[kMB_X1].store((f & RI_MOUSE_BUTTON_4_DOWN) ? 1 : 0, std::memory_order_relaxed);
            if (f & (RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP)) m_mb[kMB_X2].store((f & RI_MOUSE_BUTTON_5_DOWN) ? 1 : 0, std::memory_order_relaxed);
            return false;
        }

        if (raw->header.dwType == RIM_TYPEKEYBOARD) {
            const auto& kb = raw->data.keyboard;
            UINT vk = kb.VKey;
            const USHORT flags = kb.Flags;
            switch (vk) {
            case VK_SHIFT:   vk = MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX); break;
            case VK_CONTROL: vk = (flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL; break;
            case VK_MENU:    vk = (flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;    break;
            }
            if (vk < m_vkDown.size()) {
                m_vkDown[vk].store(!(flags & RI_KEY_BREAK), std::memory_order_relaxed);
            }
            return false;
        }
        return false;
    }
#else
    Q_UNUSED(eventType) Q_UNUSED(message) Q_UNUSED(result)
#endif
        return false;
}   


/**
 * TODO 追加の最適化案（クラス設計レベル）:
 *
 * 1. バッファサイズの事前確保:
 *    class RawInputWinFilter {
 *        alignas(64) BYTE m_buffer[sizeof(RAWINPUT)]; // メンバ変数として保持
 *    };
 *
 * 2. ビットマスク処理の最適化:
 *    // 複数のフラグを一度にチェック
 *    constexpr USHORT ALL_BUTTON_FLAGS =
 *        RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP |
 *        RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP |
 *        RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP |
 *        RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP |
 *        RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP;
 *
 *    if (!(f & ALL_BUTTON_FLAGS)) return false; // 全ボタン無関係なら即リターン
 *
 *
 */

///**
/// * 相対デルタ取得関数定義.
/// *
/// * 累積値を取り出しゼロクリアする.
/// */
 // メンバ関数本体定義(相対デルタ取り出しのため)
void RawInputWinFilter::fetchMouseDelta(int& outDx, int& outDy)
{
    // ★ relaxed で十分（単に最新値を回収するだけ）
    outDx = dx.exchange(0, std::memory_order_relaxed);
    outDy = dy.exchange(0, std::memory_order_relaxed);
}

///**
/// * 相対デルタ破棄関数定義.
/// *
/// * 残差を即時ゼロ化する.
/// */
 // メンバ関数本体定義(残差除去のため)
void RawInputWinFilter::discardDeltas()
{
    dx.exchange(0, std::memory_order_relaxed);
    dy.exchange(0, std::memory_order_relaxed);
}

///**
/// * 全キー状態リセット関数定義.
/// *
/// * すべて未押下へ戻す.
/// */
 // メンバ関数本体定義(誤爆抑止のため)
void RawInputWinFilter::resetAllKeys()
{
    // 配列反復ゼロ化処理(押下痕跡排除のため)
    for (auto& a : m_vkDown) a.store(0, std::memory_order_relaxed);
}

///**
/// * マウスボタン状態リセット関数定義.
/// *
/// * 左右/中/X1/X2を未押下へ戻す.
/// */
 // メンバ関数本体定義(誤爆抑止のため)
void RawInputWinFilter::resetMouseButtons()
{
    // 配列反復ゼロ化処理(押下痕跡排除のため)
    for (auto& b : m_mb) b.store(0, std::memory_order_relaxed);
}

///**
/// * HK→VK登録関数定義.
/// *
/// * 辞書を更新する.
/// */
 // メンバ関数本体定義(設定反映のため)
void RawInputWinFilter::setHotkeyVks(int hk, const std::vector<UINT>& vks)
{
    // 登録更新処理(上書き適用のため)
    m_hkToVk[hk] = vks;
}
///**
/// * HK押下判定関数定義.
/// *
/// * いずれかのVKが押下中ならtrue.
/// */
bool RawInputWinFilter::hotkeyDown(int hk) const
{
    // 1) まず Raw(KB/Mouse) 登録があればチェック
    auto it = m_hkToVk.find(hk);
    if (it != m_hkToVk.end()) {
        const auto& vks = it->second;
        for (UINT vk : vks) {
            // マウス
            if (vk <= VK_XBUTTON2) {
                switch (vk) {
                case VK_LBUTTON:  if (m_mb[kMB_Left].load(std::memory_order_relaxed))  return true; break;
                case VK_RBUTTON:  if (m_mb[kMB_Right].load(std::memory_order_relaxed)) return true; break;
                case VK_MBUTTON:  if (m_mb[kMB_Middle].load(std::memory_order_relaxed))return true; break;
                case VK_XBUTTON1: if (m_mb[kMB_X1].load(std::memory_order_relaxed))    return true; break;
                case VK_XBUTTON2: if (m_mb[kMB_X2].load(std::memory_order_relaxed))    return true; break;
                }
            }
            // キーボード
            else if (vk < m_vkDown.size() && m_vkDown[vk].load(std::memory_order_relaxed)) {
                return true;
            }
        }
    }

    // 2) 次にジョイスティック（EmuInstance が毎フレ更新する joyHotkeyMask）を参照
    const QBitArray* jm = m_joyHK;

    const int n = jm->size();
    if ((unsigned)hk < (unsigned)n && jm->testBit(hk)){
        return true;
    }

    return false;
}

bool RawInputWinFilter::hotkeyPressed(int hk) noexcept
{
    const bool down = hotkeyDown(hk);
    auto& prev = m_hkPrev[(size_t)hk & 511];
    const uint8_t was = prev.exchange(down, std::memory_order_acq_rel);
    return down && !was;
}

bool RawInputWinFilter::hotkeyReleased(int hk) noexcept
{
    const bool down = hotkeyDown(hk);
    auto& prev = m_hkPrev[(size_t)hk & 511];
    const uint8_t was = prev.exchange(down, std::memory_order_acq_rel);
    return (!down) && was;
}