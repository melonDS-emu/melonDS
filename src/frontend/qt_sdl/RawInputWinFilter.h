#pragma once
// Qtイベントフィルタ定義(Win32メッセージ傍受のため)
#include <QAbstractNativeEventFilter>
// アトミック(ロックレス累積のため)
#include <atomic>
// Win32 API(登録と取得のため)
#include <windows.h>
 #include <QAbstractNativeEventFilter>  // インタフェース参照のため
 #include <QByteArray>                  // 引数型参照のため
 #include <QtCore/qglobal.h>            // qintptr 定義のため

/**
 * RawInputWinFilter クラス定義.
 *
 * Win32のWM_INPUTから相対マウス移動を取得し、ロックレスで累積・取得する.
 */
class RawInputWinFilter final : public QAbstractNativeEventFilter {
public:
    /**
     * コンストラクタ定義.
     *
     * 登録と初期化を行う.
     */
    RawInputWinFilter();

    /**
     * デストラクタ定義.
     *
     * 登録解除等の後処理を行う.
     */
    ~RawInputWinFilter() override;

    /**
     * ネイティブイベントフィルタ本体定義.
     *
     * @return 処理した場合true.
     */
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    /**
     * マウスデルタ取得処理定義.
     *
     * 累積されたX/Yデルタを取り出しリセットする.
     */
    inline void fetchMouseDelta(int& outDx, int& outDy) {
        outDx = dx.exchange(0, std::memory_order_acq_rel);
        outDy = dy.exchange(0, std::memory_order_acq_rel);
    }

private:
    // アトミックデルタX(ロックレス累積のため)
    std::atomic<int> dx{ 0 };
    // アトミックデルタY(ロックレス累積のため)
    std::atomic<int> dy{ 0 };

    // RAWINPUT用デバイス(マウス/キーボード)
    RAWINPUTDEVICE rid[2]{};
};
