// 対応ヘッダ参照(宣言一致のため)
#include "MelonPrimeHotkeyVkBinding.h"
#include "InputConfig/InputConfigDialog.h"
// Windows判定(Win32固有処理限定のため)
#if defined(_WIN32)

namespace melonDS {

    // 定数定義(Qtマウス印字ビット検出のため)
    static constexpr int kQtMouseMark = 0xF0000000;

    // 内部関数宣言(特殊Qtキー変換のため)
    static inline bool ConvertSpecialQtKeyToVk(int qt, std::vector<UINT>& out) {
        // 変換結果初期化(追加挿入準備のため)
        out.clear();
        // Qt特殊キー分岐(制御キー正規化のため)
        switch (qt) {
            // Shift系分岐(左右両対応のため)
        case 0x01000020: /* Qt::Key_Shift */ {
            // 左右VK追加(互換確保のため)
            out.push_back(VK_LSHIFT);
            // 右Shift追加(互換確保のため)
            out.push_back(VK_RSHIFT);
            // 成功返却(呼び出し側継続のため)
            return true;
        }
                       // Control系分岐(左右両対応のため)
        case 0x01000021: /* Qt::Key_Control */ {
            // 左Control追加(互換確保のため)
            out.push_back(VK_LCONTROL);
            // 右Control追加(互換確保のため)
            out.push_back(VK_RCONTROL);
            // 成功返却(呼び出し側継続のため)
            return true;
        }
                       // Alt系分岐(左右両対応のため)
        case 0x01000023: /* Qt::Key_Alt */ {
            // 左Alt追加(互換確保のため)
            out.push_back(VK_LMENU);
            // 右Alt追加(互換確保のため)
            out.push_back(VK_RMENU);
            // 成功返却(呼び出し側継続のため)
            return true;
        }
                       // Tab分岐(直接対応のため)
        case 0x01000001: /* Qt::Key_Tab */ {
            // VK追加(直接対応のため)
            out.push_back(VK_TAB);
            // 成功返却(呼び出し側継続のため)
            return true;
        }
                       // PageUp分岐(直接対応のため)
        case 0x01000016: /* Qt::Key_PageUp */ {
            // VK追加(直接対応のため)
            out.push_back(VK_PRIOR);
            // 成功返却(呼び出し側継続のため)
            return true;
        }
                       // PageDown分岐(直接対応のため)
        case 0x01000017: /* Qt::Key_PageDown */ {
            // VK追加(直接対応のため)
            out.push_back(VK_NEXT);
            // 成功返却(呼び出し側継続のため)
            return true;
        }
                       // Space分岐(直接対応のため)
        case 0x20: /* Qt::Key_Space */ {
            // VK追加(直接対応のため)
            out.push_back(VK_SPACE);
            // 成功返却(呼び出し側継続のため)
            return true;
        }
                 // 既定分岐(未対応キーのため)
        default:
            // 失敗返却(後続パスへ回送のため)
            return false;
        }
    }

    // 関数本体定義(Qt整数→VK列変換のため)
    std::vector<UINT> MapQtKeyIntToVks(int qtKey) {
        // 出力配列定義(結果返却のため)
        std::vector<UINT> vks;
        // マウス印字判定(ボタン種別分岐のため)
        if ((qtKey & kQtMouseMark) == kQtMouseMark) {
            // 下位ボタン抽出(種別識別のため)
            const int btn = (qtKey & ~kQtMouseMark);
            // 左ボタン分岐(VK対応のため)
            if (btn == 0x00000001 /* Qt::LeftButton */) vks.push_back(VK_LBUTTON);
            // 右ボタン分岐(VK対応のため)
            else if (btn == 0x00000002 /* Qt::RightButton */) vks.push_back(VK_RBUTTON);
            // 中ボタン分岐(VK対応のため)
            else if (btn == 0x00000004 /* Qt::MiddleButton */) vks.push_back(VK_MBUTTON);
            // X1分岐(VK対応のため)
            else if (btn == 0x00000008 /* Qt::ExtraButton1 */) vks.push_back(VK_XBUTTON1);
            // X2分岐(VK対応のため)
            else if (btn == 0x00000010 /* Qt::ExtraButton2 */) vks.push_back(VK_XBUTTON2);
            // 返却処理(マウス完了のため)
            return vks;
        }
        // 特殊キー変換試行(制御キー正規化のため)
        if (ConvertSpecialQtKeyToVk(qtKey, vks)) {
            // 返却処理(特殊キー成功のため)
            return vks;
        }
        // 英数判定(ASCII域直写のため)
        if ((qtKey >= '0' && qtKey <= '9') || (qtKey >= 'A' && qtKey <= 'Z')) {
            // 直接VK化処理(互換維持のため)
            vks.push_back(static_cast<UINT>(qtKey));
            // 返却処理(英数完了のため)
            return vks;
        }
        // Fキー域判定(QtのF1開始コード同定のため)
        if (qtKey >= 0x01000030 /* Qt::Key_F1 */ && qtKey <= 0x0100004F /* Qt::Key_F35 */) {
            // Fキー番号算出処理(連番計算のため)
            const int idx = (qtKey - 0x01000030) + 1;
            // VK算出処理(Windows VKF域連結のため)
            vks.push_back(static_cast<UINT>(VK_F1 + (idx - 1)));
            // 返却処理(Fキー完了のため)
            return vks;
        }
        // 既定フォールバック処理(変換失敗保険のため)
        return vks;
    }

    // 関数本体定義(設定1項目→Raw登録のため)
    void BindOneHotkeyFromConfig(RawInputWinFilter* filter, int instance,
        const std::string& hkPath, int hkId) {
        // フィルタ存在判定(安全実行のため)
        if (!filter) return;
        // 設定テーブル取得処理(インスタンス指定のため)
        auto tbl = Config::GetLocalTable(instance);
        // Qt整数取得処理(TOML参照のため)
        const int qt = tbl.GetInt(hkPath);
        // VK列変換処理(Qt整数正規化のため)
        std::vector<UINT> vks = MapQtKeyIntToVks(qt);
        // Raw登録処理(HK→VK対応設定のため)
        filter->setHotkeyVks(hkId, vks);
    }

    // 関数本体定義(Shoot/Zoomまとめ登録のため)
    void BindShootZoomFromConfig(RawInputWinFilter* filter, int instance) {
        // フィルタ存在判定(安全実行のため)
        if (!filter) return;
        // Shoot登録処理(主射HKのため)
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidShootScan", HK_MetroidShootScan);
        // Zoom登録処理(ズームHKのため)
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidZoom", HK_MetroidZoom);
        // 併用登録処理(反転射HKのため)
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidScanShoot", HK_MetroidScanShoot);
    }

    void BindMetroidHotkeysFromConfig(RawInputWinFilter* filter, int instance)
    {
        if (!filter) return;

        // 射撃/ズーム
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidShootScan", HK_MetroidShootScan);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidScanShoot", HK_MetroidScanShoot);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidZoom", HK_MetroidZoom);

        // 移動
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidMoveForward", HK_MetroidMoveForward);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidMoveBack", HK_MetroidMoveBack);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidMoveLeft", HK_MetroidMoveLeft);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidMoveRight", HK_MetroidMoveRight);

        // アクション
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidJump", HK_MetroidJump);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidMorphBall", HK_MetroidMorphBall);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidHoldMorphBallBoost", HK_MetroidHoldMorphBallBoost);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidScanVisor", HK_MetroidScanVisor);

        // 感度調整（ゲーム内）
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidIngameSensiUp", HK_MetroidIngameSensiUp);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidIngameSensiDown", HK_MetroidIngameSensiDown);

        // 直接武器＆スペシャル
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeaponBeam", HK_MetroidWeaponBeam);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeaponMissile", HK_MetroidWeaponMissile);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeapon1", HK_MetroidWeapon1);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeapon2", HK_MetroidWeapon2);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeapon3", HK_MetroidWeapon3);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeapon4", HK_MetroidWeapon4);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeapon5", HK_MetroidWeapon5);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeapon6", HK_MetroidWeapon6);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeaponSpecial", HK_MetroidWeaponSpecial);

        // Next / Previous
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeaponNext", HK_MetroidWeaponNext);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidWeaponPrevious", HK_MetroidWeaponPrevious);

        // UI系をまとめて
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidUIOk", HK_MetroidUIOk);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidUILeft", HK_MetroidUILeft);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidUIRight", HK_MetroidUIRight);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidUIYes", HK_MetroidUIYes);
        BindOneHotkeyFromConfig(filter, instance, "Keyboard.HK_MetroidUINo", HK_MetroidUINo);
    }

} // namespace melonDS
#endif // _WIN32
