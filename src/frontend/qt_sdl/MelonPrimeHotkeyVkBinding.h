// ヘッダ多重包含防止(ビルド安定のため)
#pragma once
// Windows判定(Win32固有処理限定のため)
#if defined(_WIN32)
// 標準配列参照(コンテナ利用のため)
#include <array>
// 動的配列参照(可変長VK列保持のため)
#include <vector>
// 連想配列参照(HK→VK対応表管理のため)
#include <unordered_map>
// 文字列参照(パス指定のため)
#include <string>


#include <windows.h>

// 外部ヘッダ参照(RawフィルタAPI利用のため)
#include "MelonPrimeRawInputWinFilter.h"
// 外部ヘッダ参照(設定取得のため)
#include "Config.h"

// 名前空間指定(衝突回避のため)
namespace melonDS {

    // 前方宣言(内部ユーティリティ公開のため)
    /**
     * Qtキー整数→VK列変換関数定義.
     *
     * @param qtKey Qt::Key/Qt::MouseButtonの整数.
     * @return 対応するVKコード群.
     */
     // 関数宣言(キー種別正規化のため)
    std::vector<UINT> MapQtKeyIntToVks(int qtKey);


    /**
     * 任意HK登録関数定義.
     *
     * @param filter RawInputフィルタ.
     * @param instance 設定インスタンス番号.
     * @param hkPath TOMLのHKパス(例:"Keyboard.HK_MetroidScanShoot").
     * @param hkId HK識別子(ゲーム側で用いるID).
     */
     // 関数宣言(汎用登録のため)
    void BindOneHotkeyFromConfig(RawInputWinFilter* filter, int instance,
        const std::string& hkPath, int hkId);

    // ★追加：MPH系の主要HKを一括でRawに登録
    void BindMetroidHotkeysFromConfig(RawInputWinFilter* filter, int instance);

} // namespace melonDS
#endif // _WIN32
