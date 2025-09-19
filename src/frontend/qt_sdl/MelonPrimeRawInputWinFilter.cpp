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
 // メンバ関数本体定義(Win32 RAW処理のため)
bool RawInputWinFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef _WIN32
    // MSG取得処理(Win32メッセージ参照のため)
    MSG* msg = static_cast<MSG*>(message);
    // ヌル判定早期復帰処理(防御目的のため)
    if (!msg) return false;

    // WM_INPUT判定処理(Raw入力限定処理のため)
    if (msg->message == WM_INPUT) {
        // RAWINPUT受領バッファ確保処理(固定長受領のため)
        RAWINPUT raw{};
        // サイズ初期化処理(取得長指定のため)
        UINT size = sizeof(RAWINPUT);
        // データ取得呼出処理(入力受領のため)
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam),
            RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
            // 取得失敗通過処理(Qt共存維持のため)
            return false;
        }

        // マウス分岐判定処理(相対移動/ボタン処理のため)
        if (raw.header.dwType == RIM_TYPEMOUSE) {
            // 相対X累積加算処理(ロックレス更新のため)
            dx.fetch_add(static_cast<int>(raw.data.mouse.lLastX), std::memory_order_relaxed);
            // 相対Y累積加算処理(ロックレス更新のため)
            dy.fetch_add(static_cast<int>(raw.data.mouse.lLastY), std::memory_order_relaxed);

            // ボタンフラグ取得処理(押下/解放検出のため)
            const USHORT f = raw.data.mouse.usButtonFlags;
            // 左押下反映処理(状態更新のため)
            if (f & RI_MOUSE_LEFT_BUTTON_DOWN)   m_mb[kMB_Left].store(1, std::memory_order_relaxed);
            // 左解放反映処理(状態更新のため)
            if (f & RI_MOUSE_LEFT_BUTTON_UP)     m_mb[kMB_Left].store(0, std::memory_order_relaxed);
            // 右押下反映処理(状態更新のため)
            if (f & RI_MOUSE_RIGHT_BUTTON_DOWN)  m_mb[kMB_Right].store(1, std::memory_order_relaxed);
            // 右解放反映処理(状態更新のため)
            if (f & RI_MOUSE_RIGHT_BUTTON_UP)    m_mb[kMB_Right].store(0, std::memory_order_relaxed);
            // 中押下反映処理(状態更新のため)
            if (f & RI_MOUSE_MIDDLE_BUTTON_DOWN) m_mb[kMB_Middle].store(1, std::memory_order_relaxed);
            // 中解放反映処理(状態更新のため)
            if (f & RI_MOUSE_MIDDLE_BUTTON_UP)   m_mb[kMB_Middle].store(0, std::memory_order_relaxed);
            // X1押下反映処理(状態更新のため)
            if (f & RI_MOUSE_BUTTON_4_DOWN)      m_mb[kMB_X1].store(1, std::memory_order_relaxed);
            // X1解放反映処理(状態更新のため)
            if (f & RI_MOUSE_BUTTON_4_UP)        m_mb[kMB_X1].store(0, std::memory_order_relaxed);
            // X2押下反映処理(状態更新のため)
            if (f & RI_MOUSE_BUTTON_5_DOWN)      m_mb[kMB_X2].store(1, std::memory_order_relaxed);
            // X2解放反映処理(状態更新のため)
            if (f & RI_MOUSE_BUTTON_5_UP)        m_mb[kMB_X2].store(0, std::memory_order_relaxed);

            // Qt側通過返却処理(共存維持のため)
            return false;
        }

        // キーボード分岐判定処理(VK押下状態更新のため)
        if (raw.header.dwType == RIM_TYPEKEYBOARD) {
            // RAWKEYBOARD参照処理(フィールド抽出のため)
            const RAWKEYBOARD& kb = raw.data.keyboard;
            // 仮想キー一時取得処理(正規化前VKのため)
            UINT vk = static_cast<UINT>(kb.VKey);
            // E0拡張判定処理(修飾識別のため)
            const bool e0 = (kb.Flags & RI_KEY_E0) != 0;
            // 解放判定処理(押下/解放識別のため)
            const bool isUp = (kb.Flags & RI_KEY_BREAK) != 0;

            // Shift正規化処理(左右識別のため)
            if (vk == VK_SHIFT)   vk = MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX);
            // Control正規化処理(左右識別のため)
            if (vk == VK_CONTROL) vk = e0 ? VK_RCONTROL : VK_LCONTROL;
            // Alt正規化処理(左右識別のため)
            if (vk == VK_MENU)    vk = e0 ? VK_RMENU : VK_LMENU;

            // 範囲内判定処理(配列境界保護のため)
            if (vk < m_vkDown.size()) {
                // 押下状態書込処理(ロックレス更新のため)
                m_vkDown[vk].store(isUp ? 0 : 1, std::memory_order_relaxed);
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
        // 未使用抑止処理(非Win環境の警告回避のため)
        Q_UNUSED(message)
        // 未使用抑止処理(非Win環境の警告回避のため)
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
 // メンバ関数本体定義(総合照会のため)
bool RawInputWinFilter::hotkeyDown(int hk) const
{
    // マップ探索処理(登録確認のため)
    auto it = m_hkToVk.find(hk);
    // 未登録早期復帰処理(安全化のため)
    if (it == m_hkToVk.end()) return false;

    // VK列参照処理(逐次判定のため)
    const auto& vks = it->second;

    // マウスVK→インデックス変換ラムダ定義(分岐簡略化のため)
    auto mouseIndex = [](UINT vk) -> size_t {
        // 左判定処理(インデックス返却のため)
        if (vk == VK_LBUTTON)  return kMB_Left;
        // 右判定処理(インデックス返却のため)
        if (vk == VK_RBUTTON)  return kMB_Right;
        // 中判定処理(インデックス返却のため)
        if (vk == VK_MBUTTON)  return kMB_Middle;
        // X1判定処理(インデックス返却のため)
        if (vk == VK_XBUTTON1) return kMB_X1;
        // X2判定処理(インデックス返却のため)
        if (vk == VK_XBUTTON2) return kMB_X2;
        // 非該当返却処理(番兵利用のため)
        return SIZE_MAX;
        };

    // 反復判定処理(いずれか真で押下確定のため)
    for (UINT vk : vks) {
        // マウス判定処理(専用配列参照のため)
        const size_t mi = mouseIndex(vk);
        // マウス分岐処理(ボタン状態参照のため)
        if (mi != SIZE_MAX) {
            // 押下参照処理(押下検知のため)
            if (m_mb[mi].load(std::memory_order_relaxed)) return true;
        }
        else {
            // 範囲内判定処理(配列境界保護のため)
            if (vk < m_vkDown.size()) {
                // 押下参照処理(押下検知のため)
                if (m_vkDown[vk].load(std::memory_order_relaxed)) return true;
            }
        }
    }

    // 非押下返却処理(全否定のため)
    return false;
}
