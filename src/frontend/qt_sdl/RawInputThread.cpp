// ヘッダ参照(クラス宣言の参照のため)
#include "RawInputThread.h"
// QPair使用宣言(QPair型利用のため)
#include <QPair>
// ミューテックスRAII(QMutexLocker利用のため)
#include <QMutexLocker>
// 整数幅定義(ポインタ幅と一致する整数型利用のため)
#include <cstdint>
// printfマクロ(PRI* マクロ利用のため)
#include <inttypes.h>
// C入出力(printf利用のため)
#include <cstdio>

///**
/// * 相対移動コールバック関数定義.
/// *
/// * Rawライブラリからの相対移動通知を受け取り、スレッドインスタンスへデルタを橋渡しする.
/// *
/// *
/// * @param user ユーザーデータポインタ(登録時のthis).
/// * @param axis 軸種別.
/// * @param delta 変化量.
/// */
 // 静的関数指定(コールバックとしての利用のため)
static void sample_on_rel(void* user, Raw_Axis axis, int delta)
{
    // ユーザーデータ取得(this復元のため)
    RawInputThread* self = static_cast<RawInputThread*>(user);
    // ヌルチェック(安全性確保のため)
    if (!self) return;
    // デルタ反映呼び出し(内部集計のため)
    self->internalReceiveDelta(axis, delta);
}

///**
/// * プラグイン検出コールバック関数定義.
/// *
/// * 新規デバイス検出時に連番タグを割り当ててオープンする.
/// *
/// *
/// * @param idx デバイスインデックス.
/// * @param user ユーザーデータポインタ(連番カウンタ参照用).
/// */
 // 静的関数指定(コールバックとしての利用のため)
static void sample_on_plug(int idx, void* user) {
    // 連番カウンタ参照(ポインタ幅対応のため)
    std::intptr_t* next_tag = static_cast<std::intptr_t*>(user);
    // 現在タグ取得(インクリメント前値取得のため)
    const std::intptr_t tag = *next_tag;
    // カウンタ更新(次回割当のため)
    *next_tag = tag + 1;

    // デバイスオープン(タグをvoid*に安全幅変換のため)
    raw_open(idx, reinterpret_cast<void*>(tag));
    // 情報出力(ポインタ幅安全な出力のため)
    std::printf("Device %" PRIdPTR " at idx %d plugged.\n", tag, idx);
}

///**
/// * アンプラグコールバック関数定義.
/// *
/// * 取り外し通知受領時にクローズ処理と情報出力を行う.
/// *
/// *
/// * @param tag 登録時のタグ(void*).
/// * @param user 未使用.
/// */
 // 静的関数指定(コールバックとしての利用のため)
static void sample_on_unplug(void* tag, void* /*user*/) {
    // デバイスクローズ(対応するハンドル解放のため)
    raw_close(tag);
    // 整数化出力(ポインタ幅の可搬出力のため)
    std::printf("Device %" PRIdPTR " unplugged.\n", static_cast<std::intptr_t>(reinterpret_cast<std::intptr_t>(tag)));
}

///**
/// * コンストラクタ定義.
/// *
/// * Raw入力の初期化、既存デバイスのオープン、コールバック登録を行う.
/// *
/// *
/// * @param parent 親QObject.
/// */
 // 初期化子リスト呼び出し(QThread親設定のため)
RawInputThread::RawInputThread(QObject* parent) : QThread(parent)
{
    // Raw初期化呼び出し(ライブラリ準備のため)
    raw_init();

    // デバイス数取得(列挙処理のため)
    const int dev_cnt = raw_dev_cnt();
    // 情報出力(検出結果確認のため)
    std::printf("Detected %d devices.\n", dev_cnt);

    // タグ連番初期化(一意識別のため)
    std::intptr_t next_tag = 0;
    // 既存デバイス反復(全デバイスオープンのため)
    for (int i = 0; i < dev_cnt; ++i) {
        // デバイスオープン(タグをポインタ幅で保持するため)
        raw_open(i, reinterpret_cast<void*>(next_tag++));
    }

    // 相対移動コールバック登録(this橋渡しのため)
    raw_on_rel((Raw_On_Rel)sample_on_rel, this);
    // アンプラグコールバック登録(後片付けのため)
    raw_on_unplug(sample_on_unplug, nullptr);
    // プラグインコールバック登録(動的追加対応のため)
    raw_on_plug(sample_on_plug, &next_tag);
}

///**
/// * デストラクタ定義.
/// *
/// * Raw入力の終了処理を行う.
/// */
 // デストラクタ本体定義(リソース解放のため)
RawInputThread::~RawInputThread() {
    // Raw終了呼び出し(コールバック解除と内部停止のため)
    raw_quit();
}

///**
/// * 内部デルタ受領処理定義.
/// *
/// * コールバックからの相対移動量をスレッド内バッファへ集計する.
/// *
/// *
/// * @param axis 軸種別.
/// * @param delta 変化量.
/// */
 // メンバ関数本体定義(集計更新のため)
void RawInputThread::internalReceiveDelta(Raw_Axis axis, int delta) {
    // ロック取得(RAIIで例外安全確保のため)
    QMutexLocker lock(&mouseDeltaLock);
    // X軸判定更新(X軸累積のため)
    if (axis == Raw_Axis::RA_X) {
        // X加算処理(累積のため)
        mouseDeltaX += delta;
    }
    // Y軸判定更新(Y軸累積のため)
    else if (axis == Raw_Axis::RA_Y) {
        // Y加算処理(累積のため)
        mouseDeltaY += delta;
    }
    // スコープ終端解放(自動解放のため)
}

///**
/// * マウスデルタ取得処理定義.
/// *
/// * 累積されたX/Yデルタを取り出し、内部カウンタをゼロクリアする.
/// *
/// *
/// * @return 取得した(X,Y)デルタ.
/// */
 // メンバ関数本体定義(取得とリセットのため)
QPair<int, int> RawInputThread::fetchMouseDelta() {
    // ロック取得(一貫性確保のため)
    QMutexLocker lock(&mouseDeltaLock);
    // 取得値生成(コピー回数最小化のため)
    const QPair<int, int> value(mouseDeltaX, mouseDeltaY);
    // Xリセット(次回集計のため)
    mouseDeltaX = 0;
    // Yリセット(次回集計のため)
    mouseDeltaY = 0;
    // 値返却(呼び出し側更新のため)
    return value;
}

///**
/// * スレッド本体処理定義.
/// *
/// * Rawポーリングを継続実行するループを提供する.
/// */
 // メンバ関数本体定義(ポーリング継続のため)
void RawInputThread::run()
{
    // 実行中判定ループ(スレッド継続のため)
    while (isRunning()) {
        // Rawポーリング呼び出し(イベント取得のため)
        raw_poll();
    }
}
