// ヘッダ参照(クラス宣言と生API参照のため)
#include "RawInputThread.h"
// C標準入出力(デバッグ出力のため)
#include <cstdio>
// 整数幅定義(PRI* マクロ利用のため)
#include <inttypes.h>
// 整数ポインタ幅定義(タグ可搬表現のため)
#include <cstdint>

/**
 * 相対移動コールバック関数定義.
 *
 * ライブラリ側のRaw_On_Relは「void(*)(void*, Raw_Axis, int, void*)」の4引数.
 * 第4引数のユーザーデータ(ctx)にthisを渡す前提で受領し、selfへ橋渡しする.
 *
 *
 * @param user ライブラリ側コンテキスト(未使用またはフォールバック).
 * @param axis 軸種別.
 * @param delta 変化量.
 * @param ctx   登録時のユーザーデータ(this).
 */
static void sample_on_rel(void* user, Raw_Axis axis, int delta, void* ctx)
{
    // ユーザーデータ復元(登録時のthis優先のため)
    RawInputThread* self = static_cast<RawInputThread*>(ctx ? ctx : user);
    // ヌルガード(安全性確保のため)
    if (!self) return;
    // デルタ反映(集計更新のため)
    self->internalReceiveDelta(axis, delta);
}

/**
 * プラグイン検出コールバック関数定義.
 *
 * 新規デバイス検出時に連番タグを割り当ててオープンする.
 *
 *
 * @param idx デバイスインデックス.
 * @param user 連番カウンタ参照用ポインタ.
 */
static void sample_on_plug(int idx, void* user)
{
    // 連番カウンタ取得(一意タグ付与のため)
    auto* next_tag = static_cast<std::intptr_t*>(user);
    const std::intptr_t tag = *next_tag;
    *next_tag = tag + 1;

    // デバイスオープン(タグをvoid*化のため)
    raw_open(idx, reinterpret_cast<void*>(tag));
    // 情報出力(検証容易化のため)
    std::printf("Device %" PRIdPTR " at idx %d plugged.\n", tag, idx);
}

/**
 * アンプラグコールバック関数定義.
 *
 * デバイス取り外し時のクローズと情報出力を行う.
 *
 *
 * @param tag 登録時のタグ(void*).
 * @param user 未使用.
 */
static void sample_on_unplug(void* tag, void* /*user*/)
{
    // デバイスクローズ(対応解放のため)
    raw_close(tag);
    // 出力簡素化(ポインタ幅可搬出力のため)
    std::printf("Device %" PRIdPTR " unplugged.\n", reinterpret_cast<std::intptr_t>(tag));
}

/**
 * コンストラクタ定義.
 *
 * 親QThread初期化のみ行う(実初期化はrun内で実施).
 *
 *
 * @param parent 親QObject.
 */
RawInputThread::RawInputThread(QObject* parent) : QThread(parent) {}

/**
 * デストラクタ定義.
 *
 * リクエスト割り込み→合流(wait)で安全終了する.
 */
RawInputThread::~RawInputThread()
{
    // 停止要求送出(ループ脱出のため)
    requestInterruption();
    // 合流待機(安全終了のため)
    wait();
    // raw_quitはrun末尾で同一スレッドから呼ぶ(競合防止のため)
}

/**
 * 内部デルタ受領処理定義.
 *
 * Rawコールバックからの相対移動量をロックレスに累積する.
 *
 *
 * @param axis 軸種別.
 * @param delta 変化量.
 */
void RawInputThread::internalReceiveDelta(Raw_Axis axis, int delta)
{
    // X軸判定(ロックレス加算のため)
    if (axis == Raw_Axis::RA_X) {
        mouseDeltaX.fetch_add(delta, std::memory_order_relaxed);
    }
    // Y軸判定(ロックレス加算のため)
    else if (axis == Raw_Axis::RA_Y) {
        mouseDeltaY.fetch_add(delta, std::memory_order_relaxed);
    }
}

/**
 * マウスデルタ取得処理定義.
 *
 * 累積されたX/Yデルタを原子的に取り出しゼロクリアする.
 *
 *
 * @return 取得した(X,Y)デルタ.
 */
QPair<int, int> RawInputThread::fetchMouseDelta()
{
    // 原子交換で取り出し(一貫性確保のため)
    const int dx = mouseDeltaX.exchange(0, std::memory_order_acq_rel);
    const int dy = mouseDeltaY.exchange(0, std::memory_order_acq_rel);
    // 値返却(呼出側適用のため)
    return QPair<int, int>(dx, dy);
}

/**
 * スレッド本体処理定義.
 *
 * Raw初期化→コールバック登録→既存デバイスオープン→ポーリング→終了を一貫実施する.
 */
void RawInputThread::run()
{
    // Raw初期化呼び出し(内部状態準備のため)
    raw_init();

    // 連番タグ初期化(一意識別付与のため)
    std::intptr_t next_tag = 0;

    // 先にコールバック登録(イベント取りこぼし防止のため)
    raw_on_rel(sample_on_rel, this);
    raw_on_unplug(sample_on_unplug, nullptr);
    raw_on_plug(sample_on_plug, &next_tag);

    // デバイス数取得(既存デバイス走査のため)
    const int dev_cnt = raw_dev_cnt();
    std::printf("Detected %d devices.\n", dev_cnt);

    // 既存デバイス初期オープン(タグ付与のため)
    for (int i = 0; i < dev_cnt; ++i) {
        raw_open(i, reinterpret_cast<void*>(next_tag++));
    }

    // ポーリングループ開始(割り込み要求監視のため)
    while (!isInterruptionRequested()) {
        raw_poll();
    }

    // Raw終了呼び出し(登録解除と内部停止のため)
    raw_quit();
}
