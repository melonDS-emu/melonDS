// ヘッダ参照(クラスとWin32 APIのため)
#include "RawInputWinFilter.h"
// C標準入出力(デバッグのため)
#include <cstdio>

RawInputWinFilter::RawInputWinFilter()
{
    // マウス(usage page 0x01, usage 0x02)とキーボード(0x06)登録(メッセージ受信のため)
    rid[0] = { 0x01, 0x02, 0, nullptr }; // マウス
    rid[1] = { 0x01, 0x06, 0, nullptr }; // キーボード

    // 直後にフォアグラウンド窓が変わる場合があるため、HWNDは後で補完(安全策のため)
    // ここでは一旦登録失敗は無視(次のWM_CREATE/起動後に再登録でも可)
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
}

RawInputWinFilter::~RawInputWinFilter()
{
    // 登録解除(安全終了のため)
    rid[0].dwFlags = RIDEV_REMOVE; rid[0].hwndTarget = nullptr;
    rid[1].dwFlags = RIDEV_REMOVE; rid[1].hwndTarget = nullptr;
    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
}

bool RawInputWinFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    // Win32MSG取得(メッセージ傍受のため)
    MSG* msg = static_cast<MSG*>(message);
    if (!msg) return false;

    if (msg->message == WM_INPUT) {
        // RAWINPUT取得バッファ(固定長で十分なケースが多い)
        RAWINPUT raw{};
        UINT size = sizeof(RAWINPUT);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam),
            RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
            return false;
        }

        if (raw.header.dwType == RIM_TYPEMOUSE) {
            // 相対移動のみ累積(直取りのため)
            dx.fetch_add(static_cast<int>(raw.data.mouse.lLastX), std::memory_order_relaxed);
            dy.fetch_add(static_cast<int>(raw.data.mouse.lLastY), std::memory_order_relaxed);
            // ここでボタン等を拾いたければ raw.data.mouse.usButtonFlags を見る
        }
        // Qt側に渡す(ここで true を返すとQtが処理しない。必要に応じて切替)
        return false;
    }
#else
    Q_UNUSED(eventType)
        Q_UNUSED(message)
        Q_UNUSED(result)
#endif
        return false;
}

// 関数本体定義(累積カウンタの原子的クリアのため)
void RawInputWinFilter::discardDeltas() {
    // X累積値ゼロ化(可視性確保のため)
    dx.exchange(0, std::memory_order_acq_rel);
    // Y累積値ゼロ化(可視性確保のため)
    dy.exchange(0, std::memory_order_acq_rel);
}
