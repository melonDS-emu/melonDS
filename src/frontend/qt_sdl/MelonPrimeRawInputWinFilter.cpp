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
bool RawInputWinFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef _WIN32
    MSG* msg = static_cast<MSG*>(message);

    // 早期リターン：WM_INPUT以外は即座に除外
    if (Q_LIKELY(msg->message != WM_INPUT)) return false;

    // 最小限のスタックバッファ（128バイト境界アライン）
    alignas(128) BYTE buffer[256];
    UINT size = sizeof(buffer);

    // RAWINPUTデータ取得（失敗時即座にリターン）
    if (Q_UNLIKELY(GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)) {
        return false;
    }

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
    const DWORD dwType = raw->header.dwType;

    // マウス処理（最も頻繁な処理を先に配置）
    if (Q_LIKELY(dwType == RIM_TYPEMOUSE)) {
        const RAWMOUSE& m = raw->data.mouse;

        // 移動処理：ゼロチェックを単一比較で実行
        const LONG deltaX = m.lLastX;
        const LONG deltaY = m.lLastY;
        if (Q_LIKELY((deltaX | deltaY) != 0)) {
            dx.fetch_add(deltaX, std::memory_order_relaxed);
            dy.fetch_add(deltaY, std::memory_order_relaxed);
        }

        // ボタン処理：フラグが0なら即座にリターン
        const USHORT flags = m.usButtonFlags;
        if (Q_LIKELY(flags == 0)) return false;

        // ボタンマッピング：コンパイル時定数配列
        static constexpr struct {
            USHORT downFlag, upFlag;
            size_t index;
        } buttonMap[] = {
            {RI_MOUSE_LEFT_BUTTON_DOWN,   RI_MOUSE_LEFT_BUTTON_UP,   kMB_Left},
            {RI_MOUSE_RIGHT_BUTTON_DOWN,  RI_MOUSE_RIGHT_BUTTON_UP,  kMB_Right},
            {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, kMB_Middle},
            {RI_MOUSE_BUTTON_4_DOWN,      RI_MOUSE_BUTTON_4_UP,      kMB_X1},
            {RI_MOUSE_BUTTON_5_DOWN,      RI_MOUSE_BUTTON_5_UP,      kMB_X2}
        };

        // ループアンロール：各ボタンを個別処理
        for (const auto& btn : buttonMap) {
            const USHORT mask = btn.downFlag | btn.upFlag;
            if (Q_UNLIKELY(flags & mask)) {
                m_mb[btn.index].store((flags & btn.downFlag) ? 1 : 0,
                    std::memory_order_relaxed);
            }
        }
    }
    // キーボード処理
    else if (Q_UNLIKELY(dwType == RIM_TYPEKEYBOARD)) {
        const RAWKEYBOARD& kb = raw->data.keyboard;
        UINT vk = kb.VKey;
        const USHORT flags = kb.Flags;
        const bool isUp = (flags & RI_KEY_BREAK) != 0;

        // 特殊キー正規化：分岐を最小化
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
        }

        // 範囲チェック後に原子更新
        if (Q_LIKELY(vk < m_vkDown.size())) {
            m_vkDown[vk].store(static_cast<uint8_t>(!isUp),
                std::memory_order_relaxed);
        }
    }

    return false;
#else
    Q_UNUSED(eventType) Q_UNUSED(message) Q_UNUSED(result)
        return false;
#endif
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