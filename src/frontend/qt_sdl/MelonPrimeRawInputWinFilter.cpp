// ヘッダ参照宣言(クラス定義とシグネチャ一致のため)
#include "MelonPrimeRawInputWinFilter.h"
// C標準入出力参照宣言(デバッグ時printf利用のため)
#include <cstdio>
// アルゴリズム参照宣言(補助ユーティリティのため)
#include <algorithm>

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

///**
/// * ネイティブイベントフィルタ定義.
/// *
/// * WM_INPUTからマウス/キーボード状態を収集する.
/// */
bool RawInputWinFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef _WIN32
    // MSG取得処理(Win32メッセージ参照のため)
    MSG* msg = static_cast<MSG*>(message);
    // ヌル判定早期復帰処理(防御目的のため)
    if (!msg) return false;

    // WM_INPUT判定処理(Raw入力限定処理のため)
    if (msg->message == WM_INPUT) {
        // スタック配置RAWINPUTバッファ処理(ヒープ回避のため)
        alignas(8) BYTE buffer[sizeof(RAWINPUT)];
        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
        // サイズ初期化処理(取得長指定のため)
        UINT size = sizeof(RAWINPUT);

        // データ取得呼出処理(入力受領のため)
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam),
            RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
            // 取得失敗通過処理(Qt共存維持のため)
            return false;
        }

        // マウス分岐判定処理(相対移動/ボタン処理のため)
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            // マウスデータ直接参照処理(ポインタ削減のため)
            const auto& m = raw->data.mouse;

            // 相対座標同時更新処理(アトミック削減のため)
            dx.fetch_add(m.lLastX, std::memory_order_relaxed);
            dy.fetch_add(m.lLastY, std::memory_order_relaxed);

            // ボタンフラグ取得処理(押下/解放検出のため)
            const USHORT f = m.usButtonFlags;

            // ビットマスク一括判定処理(分岐削減のため)
            if (f & (RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP)) {
                m_mb[kMB_Left].store(f & RI_MOUSE_LEFT_BUTTON_DOWN ? 1 : 0, std::memory_order_relaxed);
            }
            if (f & (RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP)) {
                m_mb[kMB_Right].store(f & RI_MOUSE_RIGHT_BUTTON_DOWN ? 1 : 0, std::memory_order_relaxed);
            }
            if (f & (RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP)) {
                m_mb[kMB_Middle].store(f & RI_MOUSE_MIDDLE_BUTTON_DOWN ? 1 : 0, std::memory_order_relaxed);
            }
            if (f & (RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP)) {
                m_mb[kMB_X1].store(f & RI_MOUSE_BUTTON_4_DOWN ? 1 : 0, std::memory_order_relaxed);
            }
            if (f & (RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP)) {
                m_mb[kMB_X2].store(f & RI_MOUSE_BUTTON_5_DOWN ? 1 : 0, std::memory_order_relaxed);
            }

            // Qt側通過返却処理(共存維持のため)
            return false;
        }

        // キーボード分岐判定処理(VK押下状態更新のため)
        if (raw->header.dwType == RIM_TYPEKEYBOARD) {
            // RAWKEYBOARD直接参照処理(フィールド抽出のため)
            const auto& kb = raw->data.keyboard;
            // 仮想キー取得処理(正規化前VKのため)
            UINT vk = kb.VKey;
            // フラグ取得処理(判定高速化のため)
            const USHORT flags = kb.Flags;

            // 特殊キー高速判定処理(switch最適化のため)
            switch (vk) {
            case VK_SHIFT:
                // Shift正規化処理(左右識別のため)
                vk = MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX);
                break;
            case VK_CONTROL:
                // Control正規化処理(左右識別のため)
                vk = (flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;
                break;
            case VK_MENU:
                // Alt正規化処理(左右識別のため)
                vk = (flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;
                break;
            }

            // 範囲内判定処理(配列境界保護のため)
            if (vk < m_vkDown.size()) {
                // 押下状態直接書込処理(三項演算子削減のため)
                m_vkDown[vk].store(!(flags & RI_KEY_BREAK), std::memory_order_relaxed);
            }

            // Qt側通過返却処理(共存維持のため)
            return false;
        }

        // フォールバック通過返却処理(未対応種別維持のため)
        return false;
    }
#else
    // 未使用抑止処理(非Win環境の警告回避のため)
    Q_UNUSED(eventType)
        Q_UNUSED(message)
        Q_UNUSED(result)
#endif
        // 既定通過返却処理(Qt側処理継続のため)
        return false;
}

///**
/// * 相対デルタ取得関数定義.
/// *
/// * 累積値を取り出しゼロクリアする.
/// */
 // メンバ関数本体定義(相対デルタ取り出しのため)
void RawInputWinFilter::fetchMouseDelta(int& outDx, int& outDy)
{
    // X交換取得処理(現在値受領のため)
    outDx = dx.exchange(0, std::memory_order_acq_rel);
    // Y交換取得処理(現在値受領のため)
    outDy = dy.exchange(0, std::memory_order_acq_rel);
}

///**
/// * 相対デルタ破棄関数定義.
/// *
/// * 残差を即時ゼロ化する.
/// */
 // メンバ関数本体定義(残差除去のため)
void RawInputWinFilter::discardDeltas()
{
    // Xゼロ化交換処理(可視性確保のため)
    dx.exchange(0, std::memory_order_acq_rel);
    // Yゼロ化交換処理(可視性確保のため)
    dy.exchange(0, std::memory_order_acq_rel);
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
    // マップ探索処理(登録確認のため)
    auto it = m_hkToVk.find(hk);
    // 未登録早期復帰処理(安全化のため)
    if (it == m_hkToVk.end()) return false;

    // VK列参照処理(逐次判定のため)
    const auto& vks = it->second;

    // 反復判定処理(いずれか真で押下確定のため)
    for (UINT vk : vks) {
        // マウス高速判定処理(ビット演算利用のため)
        if (vk <= VK_XBUTTON2) {
            // マウスボタン直接判定処理(分岐削減のため)
            switch (vk) {
            case VK_LBUTTON:  if (m_mb[kMB_Left].load(std::memory_order_relaxed)) return true; break;
            case VK_RBUTTON:  if (m_mb[kMB_Right].load(std::memory_order_relaxed)) return true; break;
            case VK_MBUTTON:  if (m_mb[kMB_Middle].load(std::memory_order_relaxed)) return true; break;
            case VK_XBUTTON1: if (m_mb[kMB_X1].load(std::memory_order_relaxed)) return true; break;
            case VK_XBUTTON2: if (m_mb[kMB_X2].load(std::memory_order_relaxed)) return true; break;
            }
        }
        // キーボード判定処理(配列境界保護のため)
        else if (vk < m_vkDown.size() && m_vkDown[vk].load(std::memory_order_relaxed)) {
            return true;
        }
    }

    // 非押下返却処理(全否定のため)
    return false;
}

bool RawInputWinFilter::hotkeyPressed(int hk) noexcept {
    const bool down = hotkeyDown(hk); // いま押下中？
    auto& prev = m_hkPrev[static_cast<size_t>(hk) & 511];
    const uint8_t p = prev.exchange(down, std::memory_order_acq_rel);
    return down && !p;  // 今trueで前回falseなら Pressed
}

bool RawInputWinFilter::hotkeyReleased(int hk) noexcept {
    const bool down = hotkeyDown(hk);
    auto& prev = m_hkPrev[static_cast<size_t>(hk) & 511];
    const uint8_t p = prev.exchange(down, std::memory_order_acq_rel);
    return (!down) && p; // 今falseで前回trueなら Released
}

