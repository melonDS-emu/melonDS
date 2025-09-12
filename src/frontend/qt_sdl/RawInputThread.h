//これは現在不使用

#pragma once
// ヘッダ参照(クラス宣言の参照のため)
#include <QThread>
// アトミック使用宣言(ロックレス累積のため)
#include <atomic>
// QPair使用宣言(QPair型返却のため)
#include <QPair>

// Rawライブラリ参照(生Raw API呼び出しのため)
#include "rawinput/rawinput.h"

/**
 * RawInputThread クラス定義.
 *
 * Win32 Rawライブラリ(raw_*)をポーリングし、相対マウスデルタをロックレスで累積・取得する.
 */
class RawInputThread final : public QThread {
    Q_OBJECT
public:
    /**
     * コンストラクタ定義.
     *
     * @param parent 親QObject.
     */
    explicit RawInputThread(QObject* parent = nullptr);

    /**
     * デストラクタ定義.
     *
     * スレッドの安全停止と資源解放を行う.
     */
    ~RawInputThread() override;

    /**
     * マウスデルタ取得処理定義.
     *
     * 累積されたX/Yデルタを取り出し、内部カウンタをゼロクリアする.
     *
     * @return 取得した(X,Y)デルタ.
     */
    QPair<int, int> fetchMouseDelta();

    /**
     * 内部デルタ受領処理定義.
     *
     * Rawコールバックからの相対移動量をスレッド内バッファへ集計する.
     *
     * @param axis 軸種別.
     * @param delta 変化量.
     */
    void internalReceiveDelta(Raw_Axis axis, int delta);

protected:
    /**
     * スレッド本体処理定義.
     *
     * Raw初期化～デバイスオープン～コールバック登録～ポーリング継続～終了処理を行う.
     */
    void run() override;

private:
    // 累積デルタX(ロックレス累積のため)
    std::atomic<int> mouseDeltaX{ 0 };
    // 累積デルタY(ロックレス累積のため)
    std::atomic<int> mouseDeltaY{ 0 };
};
