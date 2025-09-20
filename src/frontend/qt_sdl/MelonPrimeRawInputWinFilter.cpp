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
    // OSメッセージ取得処理(ネイティブ情報参照のため)
    MSG* msg = static_cast<MSG*>(message);

    // 早期リターン処理(WM_INPUT以外の無駄処理回避のため)
    if (!msg || msg->message != WM_INPUT) return false;

    // スタック上バッファ確保処理(ヒープ確保回避による低遅延のため)
    alignas(64) BYTE buffer[sizeof(RAWINPUT)]; // キャッシュライン境界にアライン(メモリアクセス効率化のため)
    // RAWINPUTビュー取得処理(型安全な再解釈のため)
    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
    // サイズ初期化処理(API呼び出し整合のため)
    UINT size = sizeof(RAWINPUT);

    // 入力データ取得処理(Win32 API利用のため)
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam),
        RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
        // 失敗時早期復帰処理(安定性確保のため)
        return false;
    }

    // デバイスタイプ抽出処理(処理分岐のため)
    const DWORD dwType = raw->header.dwType;

    // マウス処理分岐定義(専用経路で高速処理するため)
    if (dwType == RIM_TYPEMOUSE) {
        // マウス構造体参照処理(フィールドアクセス簡略化のため)
        const RAWMOUSE& m = raw->data.mouse;

        // 相対移動加算判定処理(無移動時の不要加算回避のため)
        if (m.lLastX | m.lLastY) { // 非ゼロ判定処理(ORで両方同時チェックのため)
            // X加算処理(ロックレス加算による低オーバーヘッドのため)
            dx.fetch_add(m.lLastX, std::memory_order_relaxed);
            // Y加算処理(ロックレス加算による低オーバーヘッドのため)
            dy.fetch_add(m.lLastY, std::memory_order_relaxed);
        }

        // ボタンフラグ取得処理(分岐回数最小化のため)
        const USHORT f = m.usButtonFlags;
        // 無フラグ早期復帰処理(不要処理回避のため)
        if (!f) return false;

        // 要素型導出処理(m_mb要素の正しい型からポインタ型を得るため)
        using MbElem = std::remove_reference_t<decltype(m_mb[0])>;
        // ポインタ型別名定義(可読性向上のため)
        using MbElemPtr = MbElem*;

        // マッピング構造体定義(型安全ポインタ保持のため)
        struct ButtonMapping {
            // 押下フラグ定義(ビット判定のため)
            USHORT downFlag;
            // 解放フラグ定義(ビット判定のため)
            USHORT upFlag;
            // 対象原子ポインタ定義(直接store実行のため)
            MbElemPtr target;
        };

        // マッピング配列定義(this結合のため非static採用のため)
        const ButtonMapping mappings[] = {
            // 左ボタン対応定義(押下/解放を統合処理するため)
            {RI_MOUSE_LEFT_BUTTON_DOWN,   RI_MOUSE_LEFT_BUTTON_UP,   &m_mb[static_cast<std::size_t>(kMB_Left)]},
            // 右ボタン対応定義(押下/解放を統合処理するため)
            {RI_MOUSE_RIGHT_BUTTON_DOWN,  RI_MOUSE_RIGHT_BUTTON_UP,  &m_mb[static_cast<std::size_t>(kMB_Right)]},
            // 中ボタン対応定義(押下/解放を統合処理するため)
            {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, &m_mb[static_cast<std::size_t>(kMB_Middle)]},
            // X1ボタン対応定義(押下/解放を統合処理するため)
            {RI_MOUSE_BUTTON_4_DOWN,      RI_MOUSE_BUTTON_4_UP,      &m_mb[static_cast<std::size_t>(kMB_X1)]},
            // X2ボタン対応定義(押下/解放を統合処理するため)
            {RI_MOUSE_BUTTON_5_DOWN,      RI_MOUSE_BUTTON_5_UP,      &m_mb[static_cast<std::size_t>(kMB_X2)]}
        };

        // ループ処理開始定義(分岐削減と局所性向上のため)
        for (const auto& map : mappings) {
            // マスク合成処理(単一ANDで関係有無判定のため)
            const USHORT mask = map.downFlag | map.upFlag;
            // 関係判定処理(該当時のみstore実行のため)
            if (f & mask) {
                // 値決定処理(押下=1/解放=0の二値決定のため)
                const uint8_t v = (f & map.downFlag) ? uint8_t(1) : uint8_t(0);
                // 原子書き込み処理(relaxedで低オーバーヘッド維持のため)
                map.target->store(v, std::memory_order_relaxed);
            }
        }
    }
    // キーボード処理分岐定義(専用経路で高速処理するため)
    else if (dwType == RIM_TYPEKEYBOARD) {
        // キーボード構造体参照処理(フィールドアクセス簡略化のため)
        const RAWKEYBOARD& kb = raw->data.keyboard;
        // 仮想キー初期取得処理(後続の正規化に備えるため)
        UINT vk = kb.VKey;
        // フラグ取得処理(押下/解放判定のため)
        const USHORT flags = kb.Flags;
        // 解放判定処理(格納値反転に用いるため)
        const bool isKeyUp = (flags & RI_KEY_BREAK) != 0;

        // 特殊キー正規化処理(左右/拡張判定整合のため)
        if (vk == VK_SHIFT) {
            // 左右Shift判別処理(スキャンコードから実キー取得のため)
            vk = MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX);
        }
        // Ctrl左右分離処理(拡張ビットで左右決定のため)
        else if (vk == VK_CONTROL) {
            // 左右Ctrl割当処理(押下キーの正確な記録のため)
            vk = (flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;
        }
        // Alt左右分離処理(拡張ビットで左右決定のため)
        else if (vk == VK_MENU) {
            // 左右Alt割当処理(押下キーの正確な記録のため)
            vk = (flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;
        }

        // 範囲防御処理(配列境界安全確保のため)
        if (vk < m_vkDown.size()) {
            // 原子書き込み処理(relaxedで低オーバーヘッド維持のため)
            m_vkDown[vk].store(static_cast<uint8_t>(!isKeyUp), std::memory_order_relaxed);
        }
    }
    // HID未処理許容処理(想定外デバイス無視のため)

    // 既定復帰処理(Qt側へ処理継続委譲のため)
    return false;
#else
    // 未使用引数抑止処理(ビルド警告回避のため)
    Q_UNUSED(eventType) Q_UNUSED(message) Q_UNUSED(result)
        // 既定復帰処理(非Windows環境互換のため)
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
    // 1) Raw（マウス/キーボード）を先にチェック
    auto it = m_hkToVk.find(hk);
    if (it != m_hkToVk.end()) {
        const auto& vks = it->second;
        for (UINT vk : vks) {
            if (vk <= VK_XBUTTON2) {
                switch (vk) {
                case VK_LBUTTON:  if (m_mb[kMB_Left].load(std::memory_order_relaxed)) return true; break;
                case VK_RBUTTON:  if (m_mb[kMB_Right].load(std::memory_order_relaxed)) return true; break;
                case VK_MBUTTON:  if (m_mb[kMB_Middle].load(std::memory_order_relaxed)) return true; break;
                case VK_XBUTTON1: if (m_mb[kMB_X1].load(std::memory_order_relaxed)) return true; break;
                case VK_XBUTTON2: if (m_mb[kMB_X2].load(std::memory_order_relaxed)) return true; break;
                }
            }
            else if (vk < m_vkDown.size() && m_vkDown[vk].load(std::memory_order_relaxed)) {
                return true;
            }
        }
    }
    return false;
    /*
    // 2) joystick（QBitArray）を後でチェック
    const QBitArray* jm = m_joyHK;
    return jm && static_cast<unsigned>(hk) < static_cast<unsigned>(jm->size()) && jm->testBit(hk);*/
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
