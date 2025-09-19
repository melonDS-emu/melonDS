// インクルードガード宣言(多重定義防止のため)
#pragma once
// Qt抽象イベントフィルタ参照宣言(ネイティブフック実装のため)
#include <QtCore/QAbstractNativeEventFilter>
// QByteArray参照宣言(シグネチャ一致のため)
#include <QtCore/QByteArray>
// Qtグローバル参照宣言(Q_UNUSED利用のため)
#include <QtCore/QtGlobal>
// 原子操作参照宣言(ロックレス状態管理のため)
#include <atomic>
// 配列参照宣言(固定長集合保持のため)
#include <array>
// ベクタ参照宣言(HK→VKマッピング保持のため)
#include <vector>
// 連想配列参照宣言(HK→VK辞書保持のため)
#include <unordered_map>
// 整数型参照宣言(uint8_t利用のため)
#include <cstdint>

// 条件付きWin32取り込み(二重定義回避のため)
#ifdef _WIN32
// 軽量Windowsヘッダ指定(ビルド時間短縮のため)
#ifndef WIN32_LEAN_AND_MEAN
// マクロ定義(最適化のため)
#define WIN32_LEAN_AND_MEAN 1
#endif
// Windows API導入(UINTやVK_*のため)
#include <windows.h>
#endif


///**
/// * RawInputネイティブイベントフィルタクラス宣言.
/// *
/// * マウス相対デルタ／全ボタン状態／キーボードVK押下状態を収集し、
/// * HK→VKマッピングに基づく押下照会APIを提供する.
/// */
 // クラス定義本体宣言(イベントフィルタ実装のため)
class RawInputWinFilter final : public QAbstractNativeEventFilter
{
public:
    ///**
    /// * コンストラクタ宣言.
    /// *
    /// * デバイス登録と内部状態初期化を行う.
    /// */
     // コンストラクタ宣言(初期化実行のため)
    RawInputWinFilter();

    ///**
    /// * デストラクタ宣言.
    /// *
    /// * デバイス登録解除を行う.
    /// */
     // デストラクタ宣言(後始末実行のため)
    ~RawInputWinFilter() override;

    ///**
    /// * ネイティブイベントフィルタ宣言.
    /// *
    /// * WM_INPUTからRaw入力を収集する.
    /// *
    /// * @param eventType イベント種別.
    /// * @param message OSメッセージポインタ.
    /// * @param result 返却値ポインタ.
    /// * @return 伝播可否.
    /// */
     // メンバ関数宣言(イベント処理のため)
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    ///**
    /// * 相対デルタ取得関数宣言.
    /// *
    /// * 累積dx,dyを取り出してリセットする.
    /// */
     // メンバ関数宣言(デルタ受領のため)
    void fetchMouseDelta(int& outDx, int& outDy);

    ///**
    /// * 相対デルタ破棄関数宣言.
    /// *
    /// * 累積dx,dyを即時ゼロ化する.
    /// */
     // メンバ関数宣言(残差除去のため)
    void discardDeltas();

    ///**
    /// * 全キー状態リセット関数宣言.
    /// *
    /// * すべてのキーボードVKを未押下へ戻す.
    /// */
     // メンバ関数宣言(誤爆防止のため)
    void resetAllKeys();

    ///**
    /// * マウスボタン状態リセット関数宣言.
    /// *
    /// * 左/右/中/X1/X2を未押下へ戻す.
    /// */
     // メンバ関数宣言(誤爆防止のため)
    void resetMouseButtons();

    ///**
    /// * HK→VK登録関数宣言.
    /// *
    /// * 指定HKに対し対応するVK列を設定する.
    /// *
    /// * @param hk ホットキーID.
    /// * @param vks 仮想キー列.
    /// */
     // メンバ関数宣言(設定反映のため)
    void setHotkeyVks(int hk, const std::vector<UINT>& vks);

    ///**
    /// * HK押下判定関数宣言.
    /// *
    /// * 登録済みVKのいずれかが押下ならtrue.
    /// *
    /// * @param hk ホットキーID.
    /// * @return 押下状態.
    /// */
     // メンバ関数宣言(押下照会のため)
    bool hotkeyDown(int hk) const;

    ///**
    /// * 左ボタン押下参照インライン関数宣言.
    /// *
    /// * 互換API用途のため.
    /// */
     // インライン関数宣言(互換提供のため)
    inline bool leftPressed() const noexcept {
        // 読み取り処理実行(原子参照のため)
        return m_mb[kMB_Left].load(std::memory_order_relaxed);
    }

    ///**
    /// * 右ボタン押下参照インライン関数宣言.
    /// *
    /// * 互換API用途のため.
    /// */
     // インライン関数宣言(互換提供のため)
    inline bool rightPressed() const noexcept {
        // 読み取り処理実行(原子参照のため)
        return m_mb[kMB_Right].load(std::memory_order_relaxed);
    }

private:
    // Win32デバイス登録配列宣言(登録/解除管理のため)
#ifdef _WIN32
    // RAWINPUTDEVICE配列宣言(マウス/キーボードのため)
    RAWINPUTDEVICE rid[2]{};
#endif

    // 相対X累積宣言(ロックレス加算のため)
    std::atomic<int> dx{ 0 };
    // 相対Y累積宣言(ロックレス加算のため)
    std::atomic<int> dy{ 0 };

    // マウスボタン数定数宣言(配列長決定のため)
    static constexpr size_t kMouseBtnCount = 5;
    // マウスボタンインデックス列挙宣言(読みやすさ向上のため)
    enum : size_t { kMB_Left = 0, kMB_Right = 1, kMB_Middle = 2, kMB_X1 = 3, kMB_X2 = 4 };

    // キーボードVK押下状態配列宣言(256キー分のため)
    std::array<std::atomic<uint8_t>, 256> m_vkDown{};
    // マウスボタン押下状態配列宣言(左/右/中/X1/X2のため)
    std::array<std::atomic<uint8_t>, kMouseBtnCount> m_mb{};

    // HK→VK対応表宣言(押下判定解決のため)
    std::unordered_map<int, std::vector<UINT>> m_hkToVk;
};
