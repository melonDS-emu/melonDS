/*
    Copyright 2016-2024 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <vector>
#include <string>
#include <algorithm>

#include <SDL2/SDL.h>

#include "main.h"

#include "types.h"
#include "version.h"

#include "ScreenLayout.h"

#include "Args.h"
#include "NDS.h"
#include "NDSCart.h"
#include "GBACart.h"
#include "GPU.h"
#include "SPU.h"
#include "Wifi.h"
#include "Platform.h"
#include "LocalMP.h"
#include "Config.h"
#include "RTC.h"
#include "DSi.h"
#include "DSi_I2C.h"
#include "GPU3D_Soft.h"
#include "GPU3D_OpenGL.h"
#include "GPU3D_Compute.h"

#include "Savestate.h"

#include "EmuInstance.h"

// melonPrimeDS
#include <cstdint>
#include <cmath>


using namespace melonDS;


EmuThread::EmuThread(EmuInstance* inst, QObject* parent) : QThread(parent)
{
    emuInstance = inst;

    emuStatus = emuStatus_Paused;
    emuPauseStack = emuPauseStackRunning;
    emuActive = false;
}

void EmuThread::attachWindow(MainWindow* window)
{
    connect(this, SIGNAL(windowTitleChange(QString)), window, SLOT(onTitleUpdate(QString)));
    connect(this, SIGNAL(windowEmuStart()), window, SLOT(onEmuStart()));
    connect(this, SIGNAL(windowEmuStop()), window, SLOT(onEmuStop()));
    connect(this, SIGNAL(windowEmuPause(bool)), window, SLOT(onEmuPause(bool)));
    connect(this, SIGNAL(windowEmuReset()), window, SLOT(onEmuReset()));
    connect(this, SIGNAL(autoScreenSizingChange(int)), window->panel, SLOT(onAutoScreenSizingChanged(int)));
    connect(this, SIGNAL(windowFullscreenToggle()), window, SLOT(onFullscreenToggled()));
    connect(this, SIGNAL(screenEmphasisToggle()), window, SLOT(onScreenEmphasisToggled()));

    if (window->winHasMenu())
    {
        connect(this, SIGNAL(windowLimitFPSChange()), window->actLimitFramerate, SLOT(trigger()));
        connect(this, SIGNAL(swapScreensToggle()), window->actScreenSwap, SLOT(trigger()));
    }
}

void EmuThread::detachWindow(MainWindow* window)
{
    disconnect(this, SIGNAL(windowTitleChange(QString)), window, SLOT(onTitleUpdate(QString)));
    disconnect(this, SIGNAL(windowEmuStart()), window, SLOT(onEmuStart()));
    disconnect(this, SIGNAL(windowEmuStop()), window, SLOT(onEmuStop()));
    disconnect(this, SIGNAL(windowEmuPause(bool)), window, SLOT(onEmuPause(bool)));
    disconnect(this, SIGNAL(windowEmuReset()), window, SLOT(onEmuReset()));
    disconnect(this, SIGNAL(autoScreenSizingChange(int)), window->panel, SLOT(onAutoScreenSizingChanged(int)));
    disconnect(this, SIGNAL(windowFullscreenToggle()), window, SLOT(onFullscreenToggled()));
    disconnect(this, SIGNAL(screenEmphasisToggle()), window, SLOT(onScreenEmphasisToggled()));

    if (window->winHasMenu())
    {
        disconnect(this, SIGNAL(windowLimitFPSChange()), window->actLimitFramerate, SLOT(trigger()));
        disconnect(this, SIGNAL(swapScreensToggle()), window->actScreenSwap, SLOT(trigger()));
    }
}




























// melonPrime
static bool hasInitialized = false;
float mouseX;
float mouseY;
#include "MelonPrimeDef.h"
#include "MelonPrimeRomAddrTable.h"
#if defined(_WIN32)
// #include "RawInputThread.h"
#include "RawInputWinFilter.h"
#endif

/**
 * 感度値変換関数.
 *
 *
 * @param sensiVal 感度テーブルの値(uint32_t, 例: 0x00000999).
 * @return 計算された感度(double).
 */
double sensiValToSensiNum(std::uint32_t sensiVal)
{
    // 基準値定義(sensi=1.0に対応させるため)
    constexpr std::uint32_t BASE_VAL = 0x0999;
    // 増分定義(1.0感度あたりの増分を409に固定するため)
    constexpr std::uint32_t STEP_VAL = 0x0199;

    // 差分計算(BASEとの差分を得るため)
    const std::int64_t diff = static_cast<std::int64_t>(sensiVal) - static_cast<std::int64_t>(BASE_VAL);
    // 感度算出(diffをSTEPで正規化し+1.0のオフセットを加えるため)
    return static_cast<double>(diff) / static_cast<double>(STEP_VAL) + 1.0;
}

/**
 * 感度数値→テーブル値変換関数.
 *
 *
 * @param sensiNum 感度数値(double, 例: 1.0).
 * @return 感度テーブル値(uint16_tに収まる範囲). 範囲外は0x0000～0xFFFFにクリップ.
 */
std::uint16_t sensiNumToSensiVal(double sensiNum)
{
    // 基準値定義(sensi=1.0に対応させるため)
    constexpr std::uint32_t BASE_VAL = 0x0999;
    // 増分定義(1.0感度あたり+0x199=409を加算するため)
    constexpr std::uint32_t STEP_VAL = 0x0199;

    // ステップ数計算(sensiNum=1.0基準からの差をとるため)
    double steps = sensiNum - 1.0;

    // 値算出(BASEにステップ数×増分を足すため)
    double val = static_cast<double>(BASE_VAL) + steps * static_cast<double>(STEP_VAL);

    // 丸めてuint32に変換(安全な整数計算のため)
    std::uint32_t result = static_cast<std::uint32_t>(std::llround(val));

    // 下限クリップ(uint16範囲外を防ぐため)
    if (result > 0xFFFF) {
        result = 0xFFFF;
    }

    // 返却(最終的にuint16_tで返すため)
    return static_cast<std::uint16_t>(result);
}



/**
 * ヘッドフォン設定一度適用関数.
 *
 * @param NDS* nds NDS本体参照.
 * @return bool 書き込み実施有無.
 */



 /**
  * ヘッドフォン設定一度適用関数本体定義.
  *
  *
  * @param NDS* nds NDS本体参照.
  * @param Config::Table& localCfg ローカル設定参照.
  * @param uint32_t kCfgAddr 音声設定アドレス.
  * @param bool& isHeadphoneApplied 一度適用済みフラグ.
  * @return bool 書き込み実施有無.
  */
  // ヘッドフォン設定一度適用関数本体開始(起動時一回だけ反映するため)
bool ApplyHeadphoneOnce(NDS* nds, Config::Table& localCfg, uint32_t kCfgAddr, bool& isHeadphoneApplied)
{
    // 事前条件確認(ヌル参照による異常動作を避けるため)
    if (!nds) {
        // 無効参照検出リターン
        return false;
    }

    // 多重適用回避(すでに処理済みなら即リターンするため)
    if (__builtin_expect(isHeadphoneApplied, 0)) {
        return false;
    }

    // 設定有効判定(ユーザーがヘッドフォン適用を指定しなければ即リターンするため)
    if (!localCfg.GetBool("Metroid.Apply.Headphone")) {
        return false;
    }

    // 現在値読出し(差分適用と既成ビット確認のため)
    std::uint8_t oldVal = nds->ARM9Read8(kCfgAddr);

    // ターゲットビット定義(bit4:bit3領域を対象にするため)
    constexpr std::uint8_t kAudioFieldMask = 0x18;

    // 既設定判定(すでにbit4:bit3が11bであれば即リターンするため)
    if ((oldVal & kAudioFieldMask) == kAudioFieldMask) {
        // 適用済みフラグ更新(以降呼び出しで再処理しないため)
        isHeadphoneApplied = true;
        // 書き込み無しリターン
        return false;
    }

    // 新値算出(対象ビットのみ1に立てて他ビットを保持するため)
    std::uint8_t newVal = static_cast<std::uint8_t>(oldVal | kAudioFieldMask);

    // 差分無し確認(理論上newVal == oldValにはならないが安全のため)
    if (newVal == oldVal) {
        isHeadphoneApplied = true;
        return false;
    }

    // 8bit書き込み実行(必要最小限の更新で近接フィールド破壊を避けるため)
    nds->ARM9Write8(kCfgAddr, newVal);

    // 適用済みフラグ更新(以降呼び出しで再処理しないため)
    isHeadphoneApplied = true;

    // 書き込み実施リターン
    return true;
}

/**
 * ハンターライセンス色適用（装飾とランクは保持, ブランクは無視）.
 *
 * @param NDS* nds NDS本体ポインタ.
 * @param Config::Table& localCfg 設定テーブル.
 *   - Metroid.HunterLicense.Color.Apply    : bool  (適用ON/OFF).
 *   - Metroid.HunterLicense.Color.Selected : int   (0=青,1=赤,2=緑).
 * @param std::uint32_t addrRankColor ランクと色の格納アドレス（例: 0x220ECF43）.
 * @return bool 書き込み実施有無.
 */
bool applyLicenseColorStrict(NDS* nds, Config::Table& localCfg, std::uint32_t addrRankColor)
{
    // NDSが無効なら処理しない（安全性確保のため）
    if (!nds) return false;

    // 設定で適用OFFなら処理しない
    if (!localCfg.GetBool("Metroid.HunterLicense.Color.Apply"))
        return false;

    // 関数内enum（外部には露出させない）
    enum LicenseColor : int {
        Blue = 0, // bit7–6 = 00
        Red = 1, // bit7–6 = 01
        Green = 2  // bit7–6 = 10
        // Blank (3) は無視
    };

    // 設定から選択色を取得
    int sel = localCfg.GetInt("Metroid.HunterLicense.Color.Selected");

    // 範囲外（ブランク含む）は無視
    if (sel < Blue || sel > Green) return false;

    // LUT定義（色コードを対応するbitパターンに変換するため）
    constexpr std::uint8_t kColorBitsLUT[3] = {
        0x00, // Blue
        0x40, // Red
        0x80  // Green
    };

    // マスク定義（装飾bitとランク保持のため）
    constexpr std::uint8_t KEEP_MASK = 0x3F; // bit5–0
    constexpr std::uint8_t COLOR_MASK = 0xC0; // bit7–6（参照のみ）

    // 新しい色ビット
    const std::uint8_t desiredColorBits = kColorBitsLUT[sel];

    // 現在の値を読み出し
    const std::uint8_t oldVal = nds->ARM9Read8(addrRankColor);

    // 色のみ差し替え
    const std::uint8_t newVal = static_cast<std::uint8_t>((oldVal & KEEP_MASK) | desiredColorBits);

    // 差分なしなら無視
    if (newVal == oldVal) return false;

    // 書き込み実行
    nds->ARM9Write8(addrRankColor, newVal);
    return true;
}

/**
 * メインハンター適用（フェイバリット/状態は完全維持）
 *
 * @param NDS* nds
 * @param Config::Table& localCfg
 *   - Metroid.Apply.Hunter    : bool  (適用ON/OFF)
 *   - Metroid.Hunter.Selected : int   (0=Samus,1=Kanden,2=Trace,3=Sylux,4=Noxus,5=Spire,6=Weavel)
 * @param std::uint32_t addrMainHunter 例: 0x220ECF40 (8bit)
 * @return bool 書き込み実施有無
 */
bool applySelectedHunterStrict(NDS* nds, Config::Table& localCfg, std::uint32_t addrMainHunter)
{
    if (!nds) return false;
    if (!localCfg.GetBool("Metroid.HunterLicense.Hunter.Apply")) return false;

    // bit 定義
    //constexpr std::uint8_t FAVORITE_MASK = 0x80; // 参照のみ(保持)
    constexpr std::uint8_t HUNTER_MASK = 0x78; // 置換対象 bit6–3
    //constexpr std::uint8_t STATE_MASK = 0x07; // 参照のみ(保持)

    // 選択ハンター → bit6–3 パターン
    constexpr std::uint8_t kHunterBitsLUT[7] = {
        0x00, // Samus
        0x08, // Kanden
        0x10, // Trace
        0x18, // Sylux
        0x20, // Noxus
        0x28, // Spire
        0x30  // Weavel
    };
    int sel = localCfg.GetInt("Metroid.HunterLicense.Hunter.Selected");
    if (sel < 0) sel = 0;
    if (sel > 6) sel = 6;
    const std::uint8_t desiredHunterBits = static_cast<std::uint8_t>(kHunterBitsLUT[sel] & HUNTER_MASK);

    // 読み取り
    const std::uint8_t oldVal = nds->ARM9Read8(addrMainHunter);

    // ハンター種別のみ差し替え（bit7・bit2–0はそのまま）
    const std::uint8_t newVal = static_cast<std::uint8_t>((oldVal & ~HUNTER_MASK) | desiredHunterBits);

    if (newVal == oldVal) return false;      // 変更なし
    nds->ARM9Write8(addrMainHunter, newVal); // 書き込み実行
    return true;
}


/**
 * DS Name使用設定適用関数.
 *
 *
 * @param NDS* nds NDS本体参照.
 * @param Config::Table& localCfg ローカル設定参照.
 * @param std::uint32_t addrDsNameFlagAndMicVolume DS Name Flag/マイク音量共用アドレス.
 * @return bool 書き込み実施有無.
 */
bool useDsName(NDS* nds, Config::Table& localCfg, std::uint32_t addrDsNameFlagAndMicVolume)
{
    // 事前条件確認(ヌル参照による異常動作を避けるため)
    if (!nds) {
        // 無効参照検出リターン
        return false;
    }

    // 設定有効判定(ユーザーがDSの名前を使う指定をしていなければ即リターンするため)
    bool useDsNameSetting = localCfg.GetBool("Metroid.Use.Firmware.Name");

    // 現在値読み込み(フラグの状態を確認するため)
    std::uint8_t oldVal = nds->ARM9Read8(addrDsNameFlagAndMicVolume);

    // 対象ビットマスク(bit0をフラグとして扱うため)
    constexpr std::uint8_t kFlagMask = 0x01;

    // 新しい値を算出(useDsNameSettingの真偽に応じてビット操作を行うため)
    std::uint8_t newVal;
    if (useDsNameSetting) {
        // 設定がtrue → フラグをオフに設定(bit0を下ろしてDSの名前を使うようにするため)
        newVal = static_cast<std::uint8_t>(oldVal & ~kFlagMask);
    }
    else {
        // 設定がfalse → フラグをオンに設定(bit0を立ててDSの名前を使わないようにするため)
        newVal = static_cast<std::uint8_t>(oldVal | kFlagMask);
    }

    // 差分確認(値に変化がなければ何もせずリターンするため)
    if (newVal == oldVal) {
        return false;
    }

    // 8bit書き込み実行(必要な場合のみ書き込みを行うため)
    nds->ARM9Write8(addrDsNameFlagAndMicVolume, newVal);

    // 書き込み実施リターン
    return true;
}


/**
 * MPH感度設定反映関数.
 *
 * @param NDS* nds NDS本体参照.
 * @param Config::Table& localCfg ローカル設定参照.
 * @param std::uint32_t addrSensitivity 感度設定アドレス.
 * @return bool 書き込み実施有無.
 */
void ApplyMphSensitivity(NDS* nds, Config::Table& localCfg, std::uint32_t addrSensitivity)
{
    // 現在の感度数値を設定から取得(ユーザー入力を読むため)
    double mphSensitivity = localCfg.GetDouble("Metroid.Sensitivity.Mph");

    // 感度数値をROMに適用可能な形式へ変換(ゲーム内部値に一致させるため)
    std::uint32_t sensiVal = sensiNumToSensiVal(mphSensitivity);

    // NDSメモリに16bit値を書き込み(ゲーム設定を即時反映するため)
    nds->ARM9Write16(addrSensitivity, static_cast<std::uint16_t>(sensiVal));
}

/**
 * MPHハンター/マップ解除一度適用関数.
 *
 * @param NDS* nds NDS本体参照.
 * @param Config::Table& localCfg ローカル設定参照.
 * @param bool& isUnlockApplied 一度適用済みフラグ.
 * @param std::uint32_t addrUnlock1 書き込みアドレス1.
 * @param std::uint32_t addrUnlock2 書き込みアドレス2.
 * @param std::uint32_t addrUnlock3 書き込みアドレス3.
 * @param std::uint32_t addrUnlock4 書き込みアドレス4.
 * @param std::uint32_t addrUnlock5 書き込みアドレス5.
 * @return bool 書き込み実施有無.
 */
bool ApplyUnlockHuntersMaps(
    NDS* nds,
    Config::Table& localCfg,
    bool& isUnlockApplied,
    std::uint32_t addrUnlock1,
    std::uint32_t addrUnlock2,
    std::uint32_t addrUnlock3,
    std::uint32_t addrUnlock4,
    std::uint32_t addrUnlock5)
{

    // 既適用チェック(多重実行防止のため)
    if (__builtin_expect(isUnlockApplied, 0)) {
        return false;
    }

    // 設定フラグ確認(ユーザー指定OFFなら処理不要のため)
    if (!localCfg.GetBool("Metroid.Data.Unlock")) {
        return false;
    }

    // 解除データを書き込み(ゲーム内部のロック解除のため)


    // 全サウンドテストアンロック

    // 現在値読出し(既存フラグ保持のため)
    std::uint8_t cur = nds->ARM9Read8(addrUnlock1);

    // 必要ビットを立てた値を算出(既存値を壊さないため)
    std::uint8_t newVal = static_cast<std::uint8_t>(cur | 0x03);

    // 書き込み実行(解除レジスタの該当ビットを立てるため)
    nds->ARM9Write8(addrUnlock1, newVal);
			
    // 全マップアンロック
    nds->ARM9Write32(addrUnlock2, 0x07FFFFFF);
			
    // 全ハンターアンロック
    nds->ARM9Write8(addrUnlock3, 0x7F);

    // 全ギャラリーアンロック
    nds->ARM9Write32(addrUnlock4, 0xFFFFFFFF);
    nds->ARM9Write8(addrUnlock5, 0xFF);

    // 適用済みフラグ更新(再実行を避けるため)
    isUnlockApplied = true;

    // 書き込み実施結果返却(呼び出し元で処理分岐を可能にするため)
    return true;
}


// CalculatePlayerAddress Function
__attribute__((always_inline, flatten)) inline uint32_t calculatePlayerAddress(uint32_t baseAddress, uint8_t playerPosition, int32_t increment) {
    // If player position is 0, return the base address without modification
    if (playerPosition == 0) {
        return baseAddress;
    }

    // Calculate using 64-bit integers to prevent overflow
    // Use playerPosition as is (no subtraction)
    int64_t result = static_cast<int64_t>(baseAddress) + (static_cast<int64_t>(playerPosition) * increment);

    // Ensure the result is within the 32-bit range
    if (result < 0 || result > UINT32_MAX) {
        return baseAddress;  // Return the original address if out of range
    }

    return static_cast<uint32_t>(result);
}

bool isAltForm;
bool isInGame = false; // MelonPrimeDS
bool isLayoutChangePending = true;       // MelonPrimeDS layout change flag - set true to trigger on first run
bool isSensitivityChangePending = true;  // MelonPrimeDS sensitivity change flag - set true to trigger on first run
bool isSnapTapMode = false;
bool isUnlockHuntersMaps = false;
// グローバル適用フラグ定義(多重適用を避けるため)
bool isHeadphoneApplied = false;


melonDS::u32 addrBaseIsAltForm;
melonDS::u32 addrBaseLoadedSpecialWeapon;
melonDS::u32 addrBaseWeaponChange;
melonDS::u32 addrBaseSelectedWeapon;
melonDS::u32 addrBaseChosenHunter;
melonDS::u32 addrBaseJumpFlag;
melonDS::u32 addrInGame;
melonDS::u32 addrPlayerPos;
melonDS::u32 addrIsInVisorOrMap;
melonDS::u32 addrBaseAimX;
melonDS::u32 addrBaseAimY;
melonDS::u32 addrAimX;
melonDS::u32 addrAimY;
melonDS::u32 addrIsInAdventure;
melonDS::u32 addrIsMapOrUserActionPaused; // for issue in AdventureMode, Aim Stopping when SwitchingWeapon. 
melonDS::u32 addrOperationAndSound; // ToSet headphone. 8bit write
melonDS::u32 addrUnlockMapsHunters; // Sound Test Open, SFX Volum Addr
melonDS::u32 addrUnlockMapsHunters2; // All maps open addr
melonDS::u32 addrUnlockMapsHunters3; // All hunters open addr
melonDS::u32 addrUnlockMapsHunters4; // All gallery open addr
melonDS::u32 addrUnlockMapsHunters5; // All gallery open addr 2
melonDS::u32 addrSensitivity;
melonDS::u32 addrDsNameFlagAndMicVolume;
melonDS::u32 addrMainHunter;
melonDS::u32 addrRankColor;
// melonDS::u32 addrLanguage;
static bool isUnlockMapsHuntersApplied = false;

// ROM detection and address setup (self-contained within this function)
__attribute__((always_inline, flatten)) inline void detectRomAndSetAddresses(EmuInstance* emuInstance) {
    // Define ROM groups
    /*
    enum RomGroup {
        GROUP_US1_1,     // US1.1, US1.1_ENCRYPTED
        GROUP_US1_0,     // US1.0, US1.0_ENCRYPTED
        GROUP_EU1_1,     // EU1.1, EU1.1_ENCRYPTED, EU1_1_BALANCED
        GROUP_EU1_0,     // EU1.0, EU1.0_ENCRYPTED
        GROUP_JP1_0,     // JP1.0, JP1.0_ENCRYPTED
        GROUP_JP1_1,     // JP1.1, JP1.1_ENCRYPTED
        GROUP_KR1_0,     // KR1.0, KR1.0_ENCRYPTED
    };
    */

    // ROM information structure
    struct RomInfo {
        uint32_t checksum;
        const char* name;
        RomGroup group;
    };

    // Mapping of checksums to ROM info (stack-allocated)
    const RomInfo ROM_INFO_TABLE[] = {
        {RomVersions::US1_1,           "US1.1",           GROUP_US1_1},
        {RomVersions::US1_1_ENCRYPTED, "US1.1 ENCRYPTED", GROUP_US1_1},
        {RomVersions::US1_0,           "US1.0",           GROUP_US1_0},
        {RomVersions::US1_0_ENCRYPTED, "US1.0 ENCRYPTED", GROUP_US1_0},
        {RomVersions::EU1_1,           "EU1.1",           GROUP_EU1_1},
        {RomVersions::EU1_1_ENCRYPTED, "EU1.1 ENCRYPTED", GROUP_EU1_1},
        {RomVersions::EU1_1_BALANCED,  "EU1.1 BALANCED",  GROUP_EU1_1},
        {RomVersions::EU1_0,           "EU1.0",           GROUP_EU1_0},
        {RomVersions::EU1_0_ENCRYPTED, "EU1.0 ENCRYPTED", GROUP_EU1_0},
        {RomVersions::JP1_0,           "JP1.0",           GROUP_JP1_0},
        {RomVersions::JP1_0_ENCRYPTED, "JP1.0 ENCRYPTED", GROUP_JP1_0},
        {RomVersions::JP1_1,           "JP1.1",           GROUP_JP1_1},
        {RomVersions::JP1_1_ENCRYPTED, "JP1.1 ENCRYPTED", GROUP_JP1_1},
        {RomVersions::KR1_0,           "KR1.0",           GROUP_KR1_0},
        {RomVersions::KR1_0_ENCRYPTED, "KR1.0 ENCRYPTED", GROUP_KR1_0},
    };

    // Search ROM info from checksum
    const RomInfo* romInfo = nullptr;
    for (const auto& info : ROM_INFO_TABLE) {
        if (globalChecksum == info.checksum) {
            romInfo = &info;
            break;
        }
    }

    // If ROM is unsupported
    if (!romInfo) {
        return;
    }

    // ---- ここで呼ぶ！ ----
    // グループを一度だけローカル変数に保存
    RomGroup detectedGroup = romInfo->group;

    // SetRomGroupFlags(detectedGroup);   // ★ ROMグループのフラグ設定 ★

    // 既存変数への一括設定実施(分岐削減のため)
    // グローバル列挙体の完全修飾指定とキャスト実施(同名列挙体のシャドーイング回避のため)
    detectRomAndSetAddresses_fast(
        // 列挙体の完全修飾(::RomGroup)とint変換後のstatic_cast実施(型不一致解消のため)
        detectedGroup,
        // ChosenHunterアドレス引数受け渡し実施(既存変数の直接利用のため)
        addrBaseChosenHunter,
        // inGameアドレス引数受け渡し実施(既存変数の直接利用のため)
        addrInGame,
        // プレイヤ位置アドレス引数受け渡し実施(既存変数の直接利用のため)
        addrPlayerPos,
        // AltFormアドレス引数受け渡し実施(既存変数の直接利用のため)
        addrBaseIsAltForm,
        // 武器変更アドレス引数受け渡し実施(既存変数の直接利用のため)
        addrBaseWeaponChange,
        // 選択武器アドレス引数受け渡し実施(既存変数の直接利用のため)
        addrBaseSelectedWeapon,
        // AimXアドレス引数受け渡し実施(既存変数の直接利用のため)
        addrBaseAimX,
        // AimYアドレス引数受け渡し実施(既存変数の直接利用のため)
        addrBaseAimY,
        // ADV/Multi判定アドレス引数受け渡し実施(既存変数の直接利用のため)
        addrIsInAdventure,
        // ポーズ判定アドレス引数受け渡し実施(既存変数の直接利用のため)
        addrIsMapOrUserActionPaused,
        addrUnlockMapsHunters,
        addrSensitivity,
        addrMainHunter
    );

    // Addresses calculated from base values
    addrIsInVisorOrMap = addrPlayerPos - 0xABB;
    addrBaseLoadedSpecialWeapon = addrBaseIsAltForm + 0x56;
    addrBaseJumpFlag = addrBaseSelectedWeapon - 0xA;
    addrOperationAndSound = addrUnlockMapsHunters - 0x1;
    addrUnlockMapsHunters2 = addrUnlockMapsHunters + 0x3;
    addrUnlockMapsHunters3 = addrUnlockMapsHunters + 0x7;
    addrUnlockMapsHunters4 = addrUnlockMapsHunters + 0xB;
    addrUnlockMapsHunters5 = addrUnlockMapsHunters + 0xF;
    addrDsNameFlagAndMicVolume = addrUnlockMapsHunters5 + 0x1;
    // addrLanguage = addrUnlockMapsHunters - 0xB1;
    addrRankColor = addrMainHunter + 0x3;
    isRomDetected = true;

    // ROM detection message
    char message[256];
    sprintf(message, "MPH Rom version detected: %s", romInfo->name);
    emuInstance->osdAddMessage(0, message);

    // ROM判定後の処理

    // フラグリセット
    isUnlockMapsHuntersApplied = false;
    isHeadphoneApplied = false;
    // isSensitivityChangePending = true; // Aim感度リセット用 多分ここでは不要
}




































/* MelonPrimeDS function goes here */
void EmuThread::run()
{
    Config::Table& globalCfg = emuInstance->getGlobalConfig();
    Config::Table& localCfg = emuInstance->getLocalConfig();
    isSnapTapMode = localCfg.GetBool("Metroid.Operation.SnapTap"); // MelonPrimeDS
    u32 mainScreenPos[3];

    //emuInstance->updateConsole();
    // No carts are inserted when melonDS first boots

    mainScreenPos[0] = 0;
    mainScreenPos[1] = 0;
    mainScreenPos[2] = 0;
    autoScreenSizing = 0;

    //videoSettingsDirty = false;

    if (emuInstance->usesOpenGL())
    {
        emuInstance->initOpenGL(0);

        useOpenGL = true;
        videoRenderer = globalCfg.GetInt("3D.Renderer");
    }
    else
    {
        useOpenGL = false;
        videoRenderer = 0;
    }

    //updateRenderer();
    videoSettingsDirty = true;

    u32 nframes = 0;
    double perfCountsSec = 1.0 / SDL_GetPerformanceFrequency();
    double lastTime = SDL_GetPerformanceCounter() * perfCountsSec;
    double frameLimitError = 0.0;
    double lastMeasureTime = lastTime;

    u32 winUpdateCount = 0, winUpdateFreq = 1;
    u8 dsiVolumeLevel = 0x1F;

    char melontitle[100];

    bool fastforward = false;
    bool slowmo = false;
    emuInstance->fastForwardToggled = false;
    emuInstance->slowmoToggled = false;

    



    auto frameAdvanceOnce = [&]()  __attribute__((hot, always_inline, flatten)) {
        MPInterface::Get().Process();
        emuInstance->inputProcess();

        if (emuInstance->hotkeyPressed(HK_FrameLimitToggle)) emit windowLimitFPSChange();

        if (emuInstance->hotkeyPressed(HK_Pause)) emuTogglePause();
        if (emuInstance->hotkeyPressed(HK_Reset)) emuReset();
        if (emuInstance->hotkeyPressed(HK_FrameStep)) emuFrameStep();

        // MelonPrimeDS ホットキー処理部分を修正
        if (emuInstance->hotkeyPressed(HK_FullscreenToggle)) {
            emit windowFullscreenToggle();
            isLayoutChangePending = true;
        }

        if (emuInstance->hotkeyPressed(HK_SwapScreens)) {
            emit swapScreensToggle();
            isLayoutChangePending = true;
        }

        if (emuInstance->hotkeyPressed(HK_SwapScreenEmphasis)) {
            emit screenEmphasisToggle();
            isLayoutChangePending = true;
        }
        // Define minimum sensitivity as a constant to improve readability and optimization
        static constexpr int MIN_SENSITIVITY = 1;

        // Lambda function to update aim sensitivity with low latency
        static const auto updateAimSensitivity = [&](const int change) {
            // Get current sensitivity from config
            int currentSensitivity = localCfg.GetInt("Metroid.Sensitivity.Aim");

            // Calculate new sensitivity value
            int newSensitivity = currentSensitivity + change;

            // Check for minimum value threshold with early return for optimization
            if (newSensitivity < MIN_SENSITIVITY) {
                emuInstance->osdAddMessage(0, "AimSensi cannot be decreased below %d", MIN_SENSITIVITY);
                return;
            }

            // Only process if the value has actually changed
            if (newSensitivity != currentSensitivity) {
                // Update the configuration with new value
                localCfg.SetInt("Metroid.Sensitivity.Aim", newSensitivity);
                Config::Save();

                // Display message using format string instead of concatenation
                emuInstance->osdAddMessage(0, "AimSensi Updated: %d->%d", currentSensitivity, newSensitivity);
				isSensitivityChangePending = true;  // フラグを立てる
            }
            };

        // Optimize hotkey handling with a single expression
        {
            const int sensitivityChange =
                emuInstance->hotkeyReleased(HK_MetroidIngameSensiUp) ? 1 :
                emuInstance->hotkeyReleased(HK_MetroidIngameSensiDown) ? -1 : 0;

            if (sensitivityChange != 0) {
                updateAimSensitivity(sensitivityChange);
            }
        }

        if (emuStatus == emuStatus_Running || emuStatus == emuStatus_FrameStep)
        {
            if (emuStatus == emuStatus_FrameStep) emuStatus = emuStatus_Paused;

            /*
            if (emuInstance->hotkeyPressed(HK_SolarSensorDecrease))
            {
                int level = emuInstance->nds->GBACartSlot.SetInput(GBACart::Input_SolarSensorDown, true);
                if (level != -1)
                {
                    emuInstance->osdAddMessage(0, "Solar sensor level: %d", level);
                }
            }
            if (emuInstance->hotkeyPressed(HK_SolarSensorIncrease))
            {
                int level = emuInstance->nds->GBACartSlot.SetInput(GBACart::Input_SolarSensorUp, true);
                if (level != -1)
                {
                    emuInstance->osdAddMessage(0, "Solar sensor level: %d", level);
                }
            }
            */

            /*
              MelonPrimeDS commentOut
            auto handleDSiInputs = [](EmuInstance* emuInstance, double perfCountsSec) {
                if (emuInstance->nds->ConsoleType == 1)
                {
                    DSi* dsi = static_cast<DSi*>(emuInstance->nds);
                    double currentTime = SDL_GetPerformanceCounter() * perfCountsSec;

                    // Handle power button
                    if (emuInstance->hotkeyDown(HK_PowerButton))
                    {
                        dsi->I2C.GetBPTWL()->SetPowerButtonHeld(currentTime);
                    }
                    else if (emuInstance->hotkeyReleased(HK_PowerButton))
                    {
                        dsi->I2C.GetBPTWL()->SetPowerButtonReleased(currentTime);
                    }

                    // Handle volume buttons
                    if (emuInstance->hotkeyDown(HK_VolumeUp))
                    {
                        dsi->I2C.GetBPTWL()->SetVolumeSwitchHeld(DSi_BPTWL::volumeKey_Up);
                    }
                    else if (emuInstance->hotkeyReleased(HK_VolumeUp))
                    {
                        dsi->I2C.GetBPTWL()->SetVolumeSwitchReleased(DSi_BPTWL::volumeKey_Up);
                    }

                    if (emuInstance->hotkeyDown(HK_VolumeDown))
                    {
                        dsi->I2C.GetBPTWL()->SetVolumeSwitchHeld(DSi_BPTWL::volumeKey_Down);
                    }
                    else if (emuInstance->hotkeyReleased(HK_VolumeDown))
                    {
                        dsi->I2C.GetBPTWL()->SetVolumeSwitchReleased(DSi_BPTWL::volumeKey_Down);
                    }

                    dsi->I2C.GetBPTWL()->ProcessVolumeSwitchInput(currentTime);
                }
                };

            handleDSiInputs(emuInstance, perfCountsSec);

            */

            if (useOpenGL)
                emuInstance->makeCurrentGL();

            // update render settings if needed
            if (videoSettingsDirty)
            {
                emuInstance->renderLock.lock();
                if (useOpenGL)
                {
                    emuInstance->setVSyncGL(true); // is this really needed??
                    videoRenderer = globalCfg.GetInt("3D.Renderer");
                }
#ifdef OGLRENDERER_ENABLED
                else
#endif
                {
                    videoRenderer = 0;
                }

                updateRenderer();

                videoSettingsDirty = false;
                emuInstance->renderLock.unlock();
            }

            /* MelonPrimeDS comment-outed
            // process input and hotkeys
            emuInstance->nds->SetKeyMask(emuInstance->inputMask);
            */


            /* MelonPrimeDS comment-outed
            if (emuInstance->hotkeyPressed(HK_Lid))
            {
                bool lid = !emuInstance->nds->IsLidClosed();
                emuInstance->nds->SetLidClosed(lid);
                emuInstance->osdAddMessage(0, lid ? "Lid closed" : "Lid opened");
            }
            */

            // microphone input
            emuInstance->micProcess();

            // auto screen layout
            {
                mainScreenPos[2] = mainScreenPos[1];
                mainScreenPos[1] = mainScreenPos[0];
                mainScreenPos[0] = emuInstance->nds->PowerControl9 >> 15;

                int guess;
                if (mainScreenPos[0] == mainScreenPos[2] &&
                    mainScreenPos[0] != mainScreenPos[1])
                {
                    // constant flickering, likely displaying 3D on both screens
                    // TODO: when both screens are used for 2D only...???
                    guess = screenSizing_Even;
                }
                else
                {
                    if (mainScreenPos[0] == 1)
                        guess = screenSizing_EmphTop;
                    else
                        guess = screenSizing_EmphBot;
                }

                if (guess != autoScreenSizing)
                {
                    autoScreenSizing = guess;
                    emit autoScreenSizingChange(autoScreenSizing);
                }
            }


            // emulate
            u32 nlines;
            if (emuInstance->nds->GPU.GetRenderer3D().NeedsShaderCompile())
            {
                compileShaders();
                nlines = 1;
            }
            else
            {
                nlines = emuInstance->nds->RunFrame();
            }

            if (emuInstance->ndsSave)
                emuInstance->ndsSave->CheckFlush();

            if (emuInstance->gbaSave)
                emuInstance->gbaSave->CheckFlush();

            if (emuInstance->firmwareSave)
                emuInstance->firmwareSave->CheckFlush();

            if (!useOpenGL)
            {
                frontBufferLock.lock();
                frontBuffer = emuInstance->nds->GPU.FrontBuffer;
                frontBufferLock.unlock();
            }
            else
            {
                frontBuffer = emuInstance->nds->GPU.FrontBuffer;
                emuInstance->drawScreenGL();
            }

#ifdef MELONCAP
            MelonCap::Update();
#endif // MELONCAP

            winUpdateCount++;
            if (winUpdateCount >= winUpdateFreq && !useOpenGL)
            {
                emit windowUpdate();
                winUpdateCount = 0;
            }
            
            if (emuInstance->hotkeyPressed(HK_FastForwardToggle)) emuInstance->fastForwardToggled = !emuInstance->fastForwardToggled;
            if (emuInstance->hotkeyPressed(HK_SlowMoToggle)) emuInstance->slowmoToggled = !emuInstance->slowmoToggled;

            bool enablefastforward = emuInstance->hotkeyDown(HK_FastForward) | emuInstance->fastForwardToggled;
            bool enableslowmo = emuInstance->hotkeyDown(HK_SlowMo) | emuInstance->slowmoToggled;

            if (useOpenGL)
            {
                // when using OpenGL: when toggling fast-forward or slowmo, change the vsync interval
                if ((enablefastforward || enableslowmo) && !(fastforward || slowmo))
                {
                    emuInstance->setVSyncGL(false);
                }
                else if (!(enablefastforward || enableslowmo) && (fastforward || slowmo))
                {
                    emuInstance->setVSyncGL(true);
                }
            }

            fastforward = enablefastforward;
            slowmo = enableslowmo;

            if (slowmo) emuInstance->curFPS = emuInstance->slowmoFPS;
            else if (fastforward) emuInstance->curFPS = emuInstance->fastForwardFPS;
            else if (!emuInstance->doLimitFPS) emuInstance->curFPS = 1000.0;
            else emuInstance->curFPS = emuInstance->targetFPS;

            if (emuInstance->audioDSiVolumeSync && emuInstance->nds->ConsoleType == 1)
            {
                DSi* dsi = static_cast<DSi*>(emuInstance->nds);
                u8 volumeLevel = dsi->I2C.GetBPTWL()->GetVolumeLevel();
                if (volumeLevel != dsiVolumeLevel)
                {
                    dsiVolumeLevel = volumeLevel;
                    emit syncVolumeLevel();
                }

                emuInstance->audioVolume = volumeLevel * (256.0 / 31.0);
            }

            if (emuInstance->doAudioSync && !(fastforward || slowmo))
                emuInstance->audioSync();

            double frametimeStep = nlines / (emuInstance->curFPS * 263.0);

            if (frametimeStep < 0.001) frametimeStep = 0.001;

            {
                double curtime = SDL_GetPerformanceCounter() * perfCountsSec;

                frameLimitError += frametimeStep - (curtime - lastTime);
                if (frameLimitError < -frametimeStep)
                    frameLimitError = -frametimeStep;
                if (frameLimitError > frametimeStep)
                    frameLimitError = frametimeStep;

                if (round(frameLimitError * 1000.0) > 0.0)
                {
                    SDL_Delay(round(frameLimitError * 1000.0));
                    double timeBeforeSleep = curtime;
                    curtime = SDL_GetPerformanceCounter() * perfCountsSec;
                    frameLimitError -= curtime - timeBeforeSleep;
                }

                lastTime = curtime;
            }

            nframes++;
            if (nframes >= 30)
            {
                double time = SDL_GetPerformanceCounter() * perfCountsSec;
                double dt = time - lastMeasureTime;
                lastMeasureTime = time;

                u32 fps = round(nframes / dt);
                nframes = 0;

                float fpstarget = 1.0/frametimeStep;

                winUpdateFreq = fps / (u32)round(fpstarget);
                if (winUpdateFreq < 1)
                    winUpdateFreq = 1;
                    
                double actualfps = (59.8261 * 263.0) / nlines;
                int inst = emuInstance->instanceID;
                if (inst == 0)
                    sprintf(melontitle, "[%d/%.0f] melonDS " MELONDS_VERSION, fps, actualfps);
                else
                    sprintf(melontitle, "[%d/%.0f] melonDS (%d)", fps, fpstarget, inst+1);
                changeWindowTitle(melontitle);
            }
        }
        else
        {
            // paused
            nframes = 0;
            lastTime = SDL_GetPerformanceCounter() * perfCountsSec;
            lastMeasureTime = lastTime;

            emit windowUpdate();

            int inst = emuInstance->instanceID;
            if (inst == 0)
                sprintf(melontitle, "melonDS " MELONDS_VERSION);
            else
                sprintf(melontitle, "melonDS (%d)", inst+1);
            changeWindowTitle(melontitle);

            SDL_Delay(75);

            if (useOpenGL)
            {
                emuInstance->drawScreenGL();
            }
        }

        handleMessages();
    };


    /*
    auto frameAdvance = [&](int n)  __attribute__((hot, always_inline, flatten)) {
        for (int i = 0; i < n; i++) {
            frameAdvanceOnce();
        }
        };

// Define a frequently used macro to advance 2 frames
#define FRAME_ADVANCE_2 \
    do { \
        frameAdvanceOnce(); \
        frameAdvanceOnce(); \
    } while (0) \
    // Note: Why use do { ... } while (0)?
    // This is the standard safe macro form, allowing it to be treated as a single block in statements such as if-conditions.

    */

    static const auto frameAdvanceTwice = [&]() __attribute__((hot, always_inline, flatten)) {
        frameAdvanceOnce();
        frameAdvanceOnce();
    };

    // melonPrimeDS

    bool isCursorVisible = true;
    bool enableAim = true;
    bool wasLastFrameFocused = false;

    /**
     * @brief Function to show or hide the cursor on MelonPrimeDS
     *
     * Controls the mouse cursor visibility state on MelonPrimeDS. Does nothing if
     * the requested state is the same as the current state. When changing cursor
     * visibility, uses Qt::QueuedConnection to safely execute on the UI thread.
     *
     * @param show true to show the cursor, false to hide it
     */
    auto showCursorOnMelonPrimeDS = [&](bool show) __attribute__((always_inline, flatten)) {
        // Do nothing if the requested state is the same as current state (optimization)
        if (show == isCursorVisible) return;

        // Get and verify panel exists
        auto* panel = emuInstance->getMainWindow()->panel;
        if (!panel) return;

        // Use Qt::QueuedConnection to safely execute on the UI thread
        QMetaObject::invokeMethod(panel,
            [panel, show]() {
                // Set cursor visibility (normal ArrowCursor or invisible BlankCursor)
                panel->setCursor(show ? Qt::ArrowCursor : Qt::BlankCursor);
            },
            Qt::ConnectionType::QueuedConnection
        );

        // Record the state change
        isCursorVisible = show;
        };

// #define STYLUS_MODE 1 // this is for stylus user. MelonEK

#define INPUT_A 0
#define INPUT_B 1
#define INPUT_SELECT 2
#define INPUT_START 3
#define INPUT_RIGHT 4
#define INPUT_LEFT 5
#define INPUT_UP 6
#define INPUT_DOWN 7
#define INPUT_R 8
#define INPUT_L 9
#define INPUT_X 10
#define INPUT_Y 11

/*
#define FN_INPUT_PRESS(i)   emuInstance->inputMask.setBit(i, false) // No semicolon at the end here
#define FN_INPUT_RELEASE(i) emuInstance->inputMask.setBit(i, true)  // No semicolon at the end here

// Optimized macro definitions - perform direct bit operations instead of using setBit()
// #define FN_INPUT_PRESS(i)   emuInstance->inputMask[i] = false   // Direct assignment for press
// #define FN_INPUT_RELEASE(i) emuInstance->inputMask[i] = true    // Direct assignment for release

    /*
#define PERFORM_TOUCH(x, y) do { \
    emuInstance->nds->ReleaseScreen(); \
    frameAdvanceTwice(); \
    emuInstance->nds->TouchScreen(x, y); \
    frameAdvanceTwice(); \
} while(0)
*/
/*
// 
    static const auto PERFORM_TOUCH = [&](int x, int y) __attribute__((always_inline)) {
        emuInstance->nds->ReleaseScreen();
        frameAdvanceTwice();
        emuInstance->nds->TouchScreen(x, y);
        frameAdvanceTwice();
    };
    */

    /**
     * Macro to obtain a 12-bit input state from a QBitArray as a bitmask.
     * Originally defined in EmuInstanceInput.cpp.
     *
     * @param input QBitArray (must have at least 12 bits).
     * @return uint32_t Bitmask containing input state (bits 0–11).
     */
#define GET_INPUT_MASK(inputMask) (                                          \
    (static_cast<uint32_t>((inputMask).testBit(0))  << 0)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(1))  << 1)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(2))  << 2)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(3))  << 3)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(4))  << 4)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(5))  << 5)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(6))  << 6)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(7))  << 7)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(8))  << 8)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(9))  << 9)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(10)) << 10) |                 \
    (static_cast<uint32_t>((inputMask).testBit(11)) << 11)                   \
)

    uint8_t playerPosition;
    const uint16_t incrementOfPlayerAddress = 0xF30;
    const uint8_t incrementOfAimAddr = 0x48;
    uint32_t addrIsAltForm;
    uint32_t addrLoadedSpecialWeapon;
    uint32_t addrChosenHunter;
    uint32_t addrWeaponChange;
    uint32_t addrSelectedWeapon;
    uint32_t addrJumpFlag;

    uint32_t addrHavingWeapons;
    uint32_t addrCurrentWeapon;

    uint32_t addrBoostGauge;
    uint32_t addrIsBoosting;

    uint32_t addrWeaponAmmo;

    // uint32_t isPrimeHunterAddr;


    bool isRoundJustStarted;
    bool isInAdventure;
    bool isSamus;

    bool isWeavel;
    bool isPaused = false; // MelonPrimeDS

    // The QPoint class defines a point in the plane using integer precision. 
    // auto mouseRel = rawInputThread->fetchMouseDelta();
    // QPoint mouseRel;

    // Initialize Adjusted Center 


    // test
    // Lambda function to get adjusted center position based on window geometry and screen layout
#ifndef STYLUS_MODE
    static const auto getAdjustedCenter = [&]()__attribute__((hot, always_inline, flatten)) -> QPoint {
        // Cache static constants outside the function to avoid recomputation
        static constexpr float DEFAULT_ADJUSTMENT = 0.25f;
        static constexpr float HYBRID_RIGHT = 0.333203125f;  // (2133-1280)/2560 = 85/256  = (n * 85) >> 8
        static constexpr float HYBRID_LEFT = 0.166796875f;   // (1280-853)/2560 = 43/256  =  (n * 43) >> 8
        static QPoint adjustedCenter;
        static bool lastFullscreen = false;
        static bool lastSwapped = false;
        static int lastScreenSizing = -1;
        static int lastLayout = -1;

        auto& windowCfg = emuInstance->getMainWindow()->getWindowConfig();

        // Fast access to current settings - get all at once
        int currentLayout = windowCfg.GetInt("ScreenLayout");
        int currentScreenSizing = windowCfg.GetInt("ScreenSizing");
        bool currentSwapped = windowCfg.GetBool("ScreenSwap");
        bool currentFullscreen = emuInstance->getMainWindow()->isFullScreen();

        // Return cached value if settings haven't changed
        if (currentLayout == lastLayout &&
            currentScreenSizing == lastScreenSizing &&
            currentSwapped == lastSwapped &&
            currentFullscreen == lastFullscreen) {
            return adjustedCenter;
        }

        // Update cached settings
        lastLayout = currentLayout;
        lastScreenSizing = currentScreenSizing;
        lastSwapped = currentSwapped;
        lastFullscreen = currentFullscreen;

        // Get display dimensions once
        const QRect& displayRect = emuInstance->getMainWindow()->panel->geometry();
        const int displayWidth = displayRect.width();
        const int displayHeight = displayRect.height();

        // Calculate base center position
        adjustedCenter = emuInstance->getMainWindow()->panel->mapToGlobal(
            QPoint(displayWidth >> 1, displayHeight >> 1)
        );

        // Fast path for special cases
        if (currentScreenSizing == screenSizing_TopOnly) {
            return adjustedCenter;
        }

        if (currentScreenSizing == screenSizing_BotOnly) {
            if (currentFullscreen) {
                // Precompute constants to avoid repeated multiplications
                const float widthAdjust = displayWidth * 0.4f;
                const float heightAdjust = displayHeight * 0.4f;

                adjustedCenter.rx() -= static_cast<int>(widthAdjust);
                adjustedCenter.ry() -= static_cast<int>(heightAdjust);
            }
            return adjustedCenter;
        }

        // Fast path for Hybrid layout with swap
        if (currentLayout == screenLayout_Hybrid && currentSwapped) {
            // Directly compute result for this specific case
            adjustedCenter.rx() += static_cast<int>(displayWidth * HYBRID_RIGHT);
            adjustedCenter.ry() -= static_cast<int>(displayHeight * DEFAULT_ADJUSTMENT);
            return adjustedCenter;
        }

        // For other cases, determine adjustment values
        float xAdjust = 0.0f;
        float yAdjust = 0.0f;

        // Simplified switch with fewer branches
        switch (currentLayout) {
        case screenLayout_Natural:
        case screenLayout_Vertical:
            yAdjust = DEFAULT_ADJUSTMENT;
            break;
        case screenLayout_Horizontal:
            xAdjust = DEFAULT_ADJUSTMENT;
            break;
        case screenLayout_Hybrid:
            // We already handled the swapped case above
            xAdjust = HYBRID_LEFT;
            break;
        }

        // Apply non-zero adjustments only
        if (xAdjust != 0.0f) {
            // Using direct ternary operator instead of swapFactor variable
            adjustedCenter.rx() += static_cast<int>(displayWidth * xAdjust * (currentSwapped ? 1.0f : -1.0f));
        }

        if (yAdjust != 0.0f) {
            // Using direct ternary operator instead of swapFactor variable
            adjustedCenter.ry() += static_cast<int>(displayHeight * yAdjust * (currentSwapped ? 1.0f : -1.0f));
        }

        return adjustedCenter;
        };
#endif




    // processMoveInputFunction{

#ifdef COMMENTOUTTTTTTTT
        /**
         * 移動入力を処理 v1.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
        static const auto processMoveInput = [&]() __attribute__((hot, always_inline, flatten)) {
            // SnapTap状態構造体定義(キャッシュライン最適化)
            alignas(64) static struct {
                uint32_t lastInputBitmap;    // 前回入力ビットマップ保持
                uint32_t priorityInput;      // 優先入力ビットマップ保持
                uint32_t _padding[14];       // 64バイト境界確保
            } snapTapState = { 0, 0, {} };

            // 水平・垂直競合用マスク定数定義
            static constexpr uint32_t HORIZ_MASK = 0xC;   // (1<<2)|(1<<3) - LEFT|RIGHT
            static constexpr uint32_t VERT_MASK = 0x3;   // (1<<0)|(1<<1) - UP|DOWN

            // 超コンパクトLUT - L1キャッシュ効率最大化
            alignas(16) static constexpr uint8_t LUT[16] = {
                0,    // 0000: なし
                1,    // 0001: ↑ 
                2,    // 0010: ↓
                0,    // 0011: ↑↓(キャンセル)
                4,    // 0100: ←
                5,    // 0101: ↑←
                6,    // 0110: ↓←
                4,    // 0111: ←(↑↓キャンセル)
                8,    // 1000: →
                9,    // 1001: ↑→
                10,   // 1010: ↓→
                8,    // 1011: →(↑↓キャンセル)
                0,    // 1100: ←→(キャンセル)
                1,    // 1101: ↑(←→キャンセル)
                2,    // 1110: ↓(←→キャンセル)
                0     // 1111: 全キャンセル
            };

            // 超高速入力取得 - 現代コンパイラが自動最適化
            const uint32_t f = emuInstance->hotkeyDown(HK_MetroidMoveForward);
            const uint32_t b = emuInstance->hotkeyDown(HK_MetroidMoveBack);
            const uint32_t l = emuInstance->hotkeyDown(HK_MetroidMoveLeft);
            const uint32_t r = emuInstance->hotkeyDown(HK_MetroidMoveRight);

            // 入力ビットマップ生成 - 並列実行最適化
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            uint8_t finalState;

            // 分岐予測最適化 - 通常モード優先
            if (__builtin_expect(!isSnapTapMode, 1)) {
                // 通常モード - 直接配列アクセス（最速）
                finalState = LUT[curr];
            }
            else {
                // SnapTap超高速モード
                const uint32_t newPressed = curr & ~snapTapState.lastInputBitmap;

                // 並列競合判定 - XOR最適化
                const uint32_t hConflict = (curr & HORIZ_MASK) ^ HORIZ_MASK;
                const uint32_t vConflict = (curr & VERT_MASK) ^ VERT_MASK;

                // 条件最適化 - 新入力は稀
                if (__builtin_expect(newPressed != 0, 0)) {
                    // 超高速branchless更新 - 単一式統合
                    const uint32_t hMask = -(hConflict == 0);
                    const uint32_t vMask = -(vConflict == 0);

                    snapTapState.priorityInput =
                        (snapTapState.priorityInput & (~HORIZ_MASK | ~hMask) & (~VERT_MASK | ~vMask)) |
                        ((newPressed & HORIZ_MASK) & hMask) |
                        ((newPressed & VERT_MASK) & vMask);
                }

                snapTapState.priorityInput &= curr;

                // 超高速競合解決 - 単一パス処理
                uint32_t finalInput = curr;
                const uint32_t conflictMask =
                    ((hConflict == 0) ? HORIZ_MASK : 0) |
                    ((vConflict == 0) ? VERT_MASK : 0);

                if (__builtin_expect(conflictMask != 0, 0)) {
                    finalInput = (finalInput & ~conflictMask) | (snapTapState.priorityInput & conflictMask);
                }

                snapTapState.lastInputBitmap = curr;

                // 直接配列アクセス - ポインタより高速
                finalState = LUT[finalInput];
            }

            // 究極の入力適用 - コンパイラ自動最適化
            const uint32_t states = finalState;

            // QBitArray超高速更新 - ループ展開 + 最適化
            auto& mask = emuInstance->inputMask;

            // 並列実行可能な独立した操作
            mask.setBit(INPUT_UP, !(states & 1));
            mask.setBit(INPUT_DOWN, !(states & 2));
            mask.setBit(INPUT_LEFT, !(states & 4));
            mask.setBit(INPUT_RIGHT, !(states & 8));
        };

        /**
         * 移動入力を処理 v1 remake.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // SnapTap状態構造体定義(キャッシュライン最適化)
            alignas(64) static struct {
                uint32_t lastInputBitmap;    // 前回入力ビットマップ保持
                uint32_t priorityInput;      // 優先入力ビットマップ保持
                uint32_t _padding[14];       // 64バイト境界確保
            } snapTapState = { 0, 0, {} };

            // 水平・垂直競合用マスク定数定義
            static constexpr uint32_t HORIZ_MASK = 0xC;   // (1<<2)|(1<<3) - LEFT|RIGHT
            static constexpr uint32_t VERT_MASK = 0x3;   // (1<<0)|(1<<1) - UP|DOWN

            // 超コンパクトLUT - L1キャッシュ効率最大化
            alignas(16) static constexpr uint8_t LUT[16] = {
                0,    // 0000: なし
                1,    // 0001: ↑ 
                2,    // 0010: ↓
                0,    // 0011: ↑↓(キャンセル)
                4,    // 0100: ←
                5,    // 0101: ↑←
                6,    // 0110: ↓←
                4,    // 0111: ←(↑↓キャンセル)
                8,    // 1000: →
                9,    // 1001: ↑→
                10,   // 1010: ↓→
                8,    // 1011: →(↑↓キャンセル)
                0,    // 1100: ←→(キャンセル)
                1,    // 1101: ↑(←→キャンセル)
                2,    // 1110: ↓(←→キャンセル)
                0     // 1111: 全キャンセル
            };

            // 超高速入力取得 - 現代コンパイラが自動最適化
            const uint32_t f = hk[HK_MetroidMoveForward];
            const uint32_t b = hk[HK_MetroidMoveBack];
            const uint32_t l = hk[HK_MetroidMoveLeft];
            const uint32_t r = hk[HK_MetroidMoveRight];

            // 入力ビットマップ生成 - 並列実行最適化
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            uint8_t finalState;

            // 分岐予測最適化 - 通常モード優先
            if (__builtin_expect(!isSnapTapMode, 1)) {
                // 通常モード - 直接配列アクセス（最速）
                finalState = LUT[curr];
            }
            else {
                // SnapTap超高速モード
                const uint32_t newPressed = curr & ~snapTapState.lastInputBitmap;

                // 並列競合判定 - XOR最適化
                const uint32_t hConflict = (curr & HORIZ_MASK) ^ HORIZ_MASK;
                const uint32_t vConflict = (curr & VERT_MASK) ^ VERT_MASK;

                // 条件最適化 - 新入力は稀
                if (__builtin_expect(newPressed != 0, 0)) {
                    // 超高速branchless更新 - 単一式統合
                    const uint32_t hMask = -(hConflict == 0);
                    const uint32_t vMask = -(vConflict == 0);

                    snapTapState.priorityInput =
                        (snapTapState.priorityInput & (~HORIZ_MASK | ~hMask) & (~VERT_MASK | ~vMask)) |
                        ((newPressed & HORIZ_MASK) & hMask) |
                        ((newPressed & VERT_MASK) & vMask);
                }

                snapTapState.priorityInput &= curr;

                // 超高速競合解決 - 単一パス処理
                uint32_t finalInput = curr;
                const uint32_t conflictMask =
                    ((hConflict == 0) ? HORIZ_MASK : 0) |
                    ((vConflict == 0) ? VERT_MASK : 0);

                if (__builtin_expect(conflictMask != 0, 0)) {
                    finalInput = (finalInput & ~conflictMask) | (snapTapState.priorityInput & conflictMask);
                }

                snapTapState.lastInputBitmap = curr;

                // 直接配列アクセス - ポインタより高速
                finalState = LUT[finalInput];
            }

            // 究極の入力適用 - コンパイラ自動最適化
            const uint32_t states = finalState;

            // QBitArray超高速更新 - ループ展開 + 最適化

            // 並列実行可能な独立した操作
            mask.setBit(INPUT_UP, !(states & 1));
            mask.setBit(INPUT_DOWN, !(states & 2));
            mask.setBit(INPUT_LEFT, !(states & 4));
            mask.setBit(INPUT_RIGHT, !(states & 8));
        };



        /**
         * 移動入力を処理 v1 remake v2.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // SnapTap状態構造体定義(キャッシュライン最適化)
            alignas(64) static struct {
                uint32_t lastInputBitmap;    // 前回入力ビットマップ保持
                uint32_t priorityInput;      // 優先入力ビットマップ保持
                uint32_t _padding[14];       // 64バイト境界確保
            } snapTapState = { 0, 0, {} };

            // 水平・垂直競合用マスク定数定義
            static constexpr uint32_t HORIZ_MASK = 0xC;   // (1<<2)|(1<<3) - LEFT|RIGHT
            static constexpr uint32_t VERT_MASK = 0x3;   // (1<<0)|(1<<1) - UP|DOWN

            // 超コンパクトLUT - L1キャッシュ効率最大化
            alignas(16) static constexpr uint8_t LUT[16] = {
                0,    // 0000: なし
                1,    // 0001: ↑ 
                2,    // 0010: ↓
                0,    // 0011: ↑↓(キャンセル)
                4,    // 0100: ←
                5,    // 0101: ↑←
                6,    // 0110: ↓←
                4,    // 0111: ←(↑↓キャンセル)
                8,    // 1000: →
                9,    // 1001: ↑→
                10,   // 1010: ↓→
                8,    // 1011: →(↑↓キャンセル)
                0,    // 1100: ←→(キャンセル)
                1,    // 1101: ↑(←→キャンセル)
                2,    // 1110: ↓(←→キャンセル)
                0     // 1111: 全キャンセル
            };

            // 超高速入力取得 - 現代コンパイラが自動最適化
            const uint32_t f = hk[HK_MetroidMoveForward];
            const uint32_t b = hk[HK_MetroidMoveBack];
            const uint32_t l = hk[HK_MetroidMoveLeft];
            const uint32_t r = hk[HK_MetroidMoveRight];

            // 入力ビットマップ生成 - 並列実行最適化
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            uint8_t finalState;

            // 分岐予測最適化 - 通常モード優先
            if (__builtin_expect(!isSnapTapMode, 1)) {
                // 通常モード - 直接配列アクセス（最速）
                finalState = LUT[curr];

                // 並列実行可能な独立した操作
                mask.setBit(INPUT_UP, !(finalState & 1));
                mask.setBit(INPUT_DOWN, !(finalState & 2));
                mask.setBit(INPUT_LEFT, !(finalState & 4));
                mask.setBit(INPUT_RIGHT, !(finalState & 8));

                return;
            }

            // SnapTap超高速モード
            const uint32_t newPressed = curr & ~snapTapState.lastInputBitmap;

            // 並列競合判定 - XOR最適化
            const uint32_t hConflict = (curr & HORIZ_MASK) ^ HORIZ_MASK;
            const uint32_t vConflict = (curr & VERT_MASK) ^ VERT_MASK;

            // 条件最適化 - 新入力は稀
            if (__builtin_expect(newPressed != 0, 0)) {
                // 超高速branchless更新 - 単一式統合
                const uint32_t hMask = -(hConflict == 0);
                const uint32_t vMask = -(vConflict == 0);

                snapTapState.priorityInput =
                    (snapTapState.priorityInput & (~HORIZ_MASK | ~hMask) & (~VERT_MASK | ~vMask)) |
                    ((newPressed & HORIZ_MASK) & hMask) |
                    ((newPressed & VERT_MASK) & vMask);
            }

            snapTapState.priorityInput &= curr;

            // 超高速競合解決 - 単一パス処理
            uint32_t finalInput = curr;
            const uint32_t conflictMask =
                ((hConflict == 0) ? HORIZ_MASK : 0) |
                ((vConflict == 0) ? VERT_MASK : 0);

            if (__builtin_expect(conflictMask != 0, 0)) {
                finalInput = (finalInput & ~conflictMask) | (snapTapState.priorityInput & conflictMask);
            }

            snapTapState.lastInputBitmap = curr;

            // 直接配列アクセス - ポインタより高速
            finalState = LUT[finalInput];


            // QBitArray超高速更新 - ループ展開 + 最適化

            // 並列実行可能な独立した操作
            mask.setBit(INPUT_UP, !(finalState & 1));
            mask.setBit(INPUT_DOWN, !(finalState & 2));
            mask.setBit(INPUT_LEFT, !(finalState & 4));
            mask.setBit(INPUT_RIGHT, !(finalState & 8));
        };


        /**
         * 移動入力を処理 v2.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {

            // MaskLUT[16][4]：UP, DOWN, LEFT, RIGHT の有効ビットマスク
            // 1 = 無効（キャンセル） / 0 = 有効（反転型）
            // MaskLUT[curr][UP, DOWN, LEFT, RIGHT]
            alignas(16) static constexpr uint8_t MaskLUT[16][4] = {
                {1,1,1,1}, // 0000: なし
                {0,1,1,1}, // 0001: ↑
                {1,0,1,1}, // 0010: ↓
                {1,1,1,1}, // 0011: ↑↓キャンセル
                {1,1,0,1}, // 0100: ←
                {0,1,0,1}, // 0101: ↑←
                {1,0,0,1}, // 0110: ↓←
                {1,1,0,1}, // 0111: ←（↑↓キャンセル）
                {1,1,1,0}, // 1000: →
                {0,1,1,0}, // 1001: ↑→
                {1,0,1,0}, // 1010: ↓→
                {1,1,1,0}, // 1011: →（↑↓キャンセル）
                {1,1,1,1}, // 1100: ←→キャンセル
                {0,1,1,1}, // 1101: ↑（←→キャンセル）
                {1,0,1,1}, // 1110: ↓（←→キャンセル）
                {1,1,1,1}, // 1111: 全キャンセル
            };

            // SnapTap状態（下位8bit: last、上位8bit: priority）を保持（1レジスタ内）
            static uint16_t snapState = 0;

            // 入力・出力マスク参照の取得（無駄な再アクセス回避）
            // const QBitArray& hk = emuInstance->hotkeyMask;
            // QBitArray& mask = emuInstance->inputMask;

            // 現在の入力状態を4bitでエンコード（1bitずつ独立反映）
            const uint32_t curr =
                (hk[HK_MetroidMoveForward] ? 1 : 0) |
                (hk[HK_MetroidMoveBack] ? 2 : 0) |
                (hk[HK_MetroidMoveLeft] ? 4 : 0) |
                (hk[HK_MetroidMoveRight] ? 8 : 0);

            // SnapTapが無効な場合、即座にLUT参照して方向bit反映（最短4命令）
            if (__builtin_expect(!isSnapTapMode, 1)) {
                mask[INPUT_UP] = MaskLUT[curr][0];
                mask[INPUT_DOWN] = MaskLUT[curr][1];
                mask[INPUT_LEFT] = MaskLUT[curr][2];
                mask[INPUT_RIGHT] = MaskLUT[curr][3];
                return;
            }

            // 前回入力状態の復元（lastInput, priorityInput）
            const uint32_t last = snapState & 0xFF;
            const uint32_t priority = (snapState >> 8) & 0xFF;

            // 新しく押されたキー（priorityに反映候補）
            const uint32_t newPress = curr & ~last;

            // 水平・垂直方向の競合検出（キャンセル対象bitを生成）
            const uint32_t vConflict = ((curr & 0x3) == 0x3) ? 0x3 : 0;
            const uint32_t hConflict = ((curr & 0xC) == 0xC) ? 0xC : 0;
            const uint32_t cMask = vConflict | hConflict;

            // 優先度更新（新たに押された中で競合しているものだけを反映）
            const uint32_t newPriority = ((newPress & cMask) ? ((priority & ~cMask) | (newPress & cMask)) : priority) & curr;

            // 次回用に状態を保存（下位8bit: curr、上位8bit: priority）
            snapState = static_cast<uint16_t>(curr | (newPriority << 8));

            // 競合解決済み最終入力を合成（競合bitだけpriority、それ以外はcurr）
            const uint32_t finalInput = (curr & ~cMask) | (newPriority & cMask);

            // LUTを用いて最終方向マスクを直接反映（即値4命令で終端）
            mask[INPUT_UP] = MaskLUT[finalInput][0];
            mask[INPUT_DOWN] = MaskLUT[finalInput][1];
            mask[INPUT_LEFT] = MaskLUT[finalInput][2];
            mask[INPUT_RIGHT] = MaskLUT[finalInput][3];
        };


        /**
         * 移動入力を処理 v4.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
         /**
          * 移動入力処理 - 最低遅延版
          * @note x86_64向け完全最適化。分岐最小化・メモリアクセス削減
          */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // 反転型マスクLUT（0=有効, 1=無効）
            // インデックス: [Right|Left|Back|Forward]
            // 各バイト: UP, DOWN, LEFT, RIGHT
            alignas(64) static constexpr uint32_t MaskLUT[16] = {
                0x0F0F0F0F, // 0000: なし
                0x0F0F0F0E, // 0001: →
                0x0F0F0E0F, // 0010: ←
                0x0F0F0F0F, // 0011: ←→キャンセル
                0x0F0E0F0F, // 0100: ↓
                0x0F0E0F0E, // 0101: ↓→
                0x0F0E0E0F, // 0110: ↓←
                0x0F0E0F0F, // 0111: ↓
                0x0E0F0F0F, // 1000: ↑
                0x0E0F0F0E, // 1001: ↑→
                0x0E0F0E0F, // 1010: ↑←
                0x0E0F0F0F, // 1011: ↑
                0x0F0F0F0F, // 1100: ↑↓キャンセル
                0x0F0F0F0E, // 1101: →
                0x0F0F0E0F, // 1110: ←
                0x0F0F0F0F  // 1111: 全キャンセル
            };

            // SnapTap状態（16bit: last|priority）
            static uint16_t snapState = 0;

            // 入力状態を4bitエンコード（正しいマッピング）
            const uint32_t curr =
                (-hk[HK_MetroidMoveForward] & 1) |  // Forward = bit0
                (-hk[HK_MetroidMoveBack] & 2) |     // Back = bit1
                (-hk[HK_MetroidMoveLeft] & 4) |     // Left = bit2
                (-hk[HK_MetroidMoveRight] & 8);     // Right = bit3

            // 通常モード：即座にLUT適用
            if (__builtin_expect(!isSnapTapMode, 1)) {
                const uint32_t maskBits = MaskLUT[curr];
                mask[INPUT_UP] = maskBits & 1;
                mask[INPUT_DOWN] = (maskBits >> 8) & 1;
                mask[INPUT_LEFT] = (maskBits >> 16) & 1;
                mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
                return;
            }

            // SnapTapモード：競合解決
            const uint32_t last = snapState & 0xFF;
            const uint32_t priority = snapState >> 8;
            const uint32_t newPress = curr & ~last;

            // 競合検出（Forward/BackとLeft/Rightの組み合わせ）
            const uint32_t hConflict = ((curr & 3) == 3) ? 3 : 0;    // Forward/Back競合
            const uint32_t vConflict = ((curr & 12) == 12) ? 12 : 0; // Left/Right競合
            const uint32_t conflict = vConflict | hConflict;

            // 優先度更新
            const uint32_t newPriority = (newPress & conflict) ?
                ((priority & ~conflict) | (newPress & conflict)) : priority;
            const uint32_t activePriority = newPriority & curr;

            // 状態保存
            snapState = curr | (activePriority << 8);

            // 最終入力計算とLUT適用
            const uint32_t final = (curr & ~conflict) | (activePriority & conflict);
            const uint32_t maskBits = MaskLUT[final];
            mask[INPUT_UP] = maskBits & 1;
            mask[INPUT_DOWN] = (maskBits >> 8) & 1;
            mask[INPUT_LEFT] = (maskBits >> 16) & 1;
            mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
        };


        /**
         * 移動入力を処理 v4.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
         /**
          * 移動入力処理 - 最低遅延版
          * @note x86_64向け完全最適化。分岐最小化・メモリアクセス削減
          */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // 反転型マスクLUT（0=有効, 1=無効）
            // インデックス: [Right|Left|Back|Forward]
            // 各バイト: UP, DOWN, LEFT, RIGHT
            alignas(64) static constexpr uint32_t MaskLUT[16] = {
                0x0F0F0F0F, // 0000: なし
                0x0F0F0F0E, // 0001: →
                0x0F0F0E0F, // 0010: ←
                0x0F0F0F0F, // 0011: ←→キャンセル
                0x0F0E0F0F, // 0100: ↓
                0x0F0E0F0E, // 0101: ↓→
                0x0F0E0E0F, // 0110: ↓←
                0x0F0E0F0F, // 0111: ↓
                0x0E0F0F0F, // 1000: ↑
                0x0E0F0F0E, // 1001: ↑→
                0x0E0F0E0F, // 1010: ↑←
                0x0E0F0F0F, // 1011: ↑
                0x0F0F0F0F, // 1100: ↑↓キャンセル
                0x0F0F0F0E, // 1101: →
                0x0F0F0E0F, // 1110: ←
                0x0F0F0F0F  // 1111: 全キャンセル
            };

            // SnapTap状態（16bit: last|priority）
            static uint16_t snapState = 0;

            /*
            // 入力状態を4bitエンコード（正しいマッピング）
            const uint32_t curr =
                (-hk[HK_MetroidMoveForward] & 1) |  // Forward = bit0
                (-hk[HK_MetroidMoveBack] & 2) |     // Back = bit1
                (-hk[HK_MetroidMoveLeft] & 4) |     // Left = bit2
                (-hk[HK_MetroidMoveRight] & 8);     // Right = bit3
                */

                // 超高速入力取得 - 4並列ロード
            const uint32_t f = hk[HK_MetroidMoveForward];
            const uint32_t b = hk[HK_MetroidMoveBack];
            const uint32_t l = hk[HK_MetroidMoveLeft];
            const uint32_t r = hk[HK_MetroidMoveRight];

            // ビットマップ生成 - 単純OR演算
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            // 通常モード：即座にLUT適用
            if (__builtin_expect(!isSnapTapMode, 1)) {
                const uint32_t maskBits = MaskLUT[curr];
                mask[INPUT_UP] = maskBits & 1;
                mask[INPUT_DOWN] = (maskBits >> 8) & 1;
                mask[INPUT_LEFT] = (maskBits >> 16) & 1;
                mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
                return;
            }

            // SnapTapモード：競合解決
            const uint32_t last = snapState & 0xFF;
            const uint32_t priority = snapState >> 8;
            const uint32_t newPress = curr & ~last;

            // 競合検出（Forward/BackとLeft/Rightの組み合わせ）
            const uint32_t hConflict = ((curr & 3) == 3) ? 3 : 0;    // Forward/Back競合
            const uint32_t vConflict = ((curr & 12) == 12) ? 12 : 0; // Left/Right競合
            const uint32_t conflict = vConflict | hConflict;

            // 優先度更新
            const uint32_t newPriority = (newPress & conflict) ?
                ((priority & ~conflict) | (newPress & conflict)) : priority;
            const uint32_t activePriority = newPriority & curr;

            // 状態保存
            snapState = curr | (activePriority << 8);

            // 最終入力計算とLUT適用
            const uint32_t final = (curr & ~conflict) | (activePriority & conflict);
            const uint32_t maskBits = MaskLUT[final];
            mask[INPUT_UP] = maskBits & 1;
            mask[INPUT_DOWN] = (maskBits >> 8) & 1;
            mask[INPUT_LEFT] = (maskBits >> 16) & 1;
            mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
        };


        /**
         * 移動入力を処理 v5
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
         /**
          * 移動入力処理 - 最低遅延版
          * @note x86_64向け完全最適化。分岐最小化・メモリアクセス削減
          */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // 反転型マスクLUT（0=有効, 1=無効）
            // 各バイト: UP, DOWN, LEFT, RIGHT
            alignas(64) static constexpr uint32_t MaskLUT[16] = {
                0x0F0F0F0F, // 0000: なし
                0x0F0F0F0E, // 0001: →
                0x0F0F0E0F, // 0010: ←
                0x0F0F0F0F, // 0011: ←→キャンセル
                0x0F0E0F0F, // 0100: ↓
                0x0F0E0F0E, // 0101: ↓→
                0x0F0E0E0F, // 0110: ↓←
                0x0F0E0F0F, // 0111: ↓
                0x0E0F0F0F, // 1000: ↑
                0x0E0F0F0E, // 1001: ↑→
                0x0E0F0E0F, // 1010: ↑←
                0x0E0F0F0F, // 1011: ↑
                0x0F0F0F0F, // 1100: ↑↓キャンセル
                0x0F0F0F0E, // 1101: →
                0x0F0F0E0F, // 1110: ←
                0x0F0F0F0F  // 1111: 全キャンセル
            };

            // SnapTap状態（16bit: last|priority）
            static uint16_t snapState = 0;

            // 超高速入力取得 - 4並列ロード
            const uint32_t f = hk[HK_MetroidMoveForward];
            const uint32_t b = hk[HK_MetroidMoveBack];
            const uint32_t l = hk[HK_MetroidMoveLeft];
            const uint32_t r = hk[HK_MetroidMoveRight];

            // ビットマップ生成
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            // 通常モード：即座にLUT適用
            if (__builtin_expect(!isSnapTapMode, 1)) {
                const uint32_t maskBits = MaskLUT[curr];
                mask[INPUT_UP] = maskBits & 1;
                mask[INPUT_DOWN] = (maskBits >> 8) & 1;
                mask[INPUT_LEFT] = (maskBits >> 16) & 1;
                mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
                return;
            }

            // SnapTapモード
            const uint32_t last = snapState & 0xFF;
            const uint32_t priority = snapState >> 8;
            const uint32_t newPress = curr & ~last;

            // XOR最適化による競合検出 XOR最適化でブランチレス
            const uint32_t hConflict = ((curr & 3) ^ 3) ? 0 : 3;
            const uint32_t vConflict = ((curr & 12) ^ 12) ? 0 : 12;
            const uint32_t conflict = vConflict | hConflict;

            // branchless優先度更新
            const uint32_t updateMask = -(newPress & conflict != 0);
            const uint32_t newPriority = (priority & ~(conflict & updateMask)) |
                (newPress & conflict & updateMask);
            const uint32_t activePriority = newPriority & curr;

            // 状態保存
            snapState = curr | (activePriority << 8);

            // 最終入力計算とLUT適用
            const uint32_t final = (curr & ~conflict) | (activePriority & conflict);
            const uint32_t maskBits = MaskLUT[final];
            mask[INPUT_UP] = maskBits & 1;
            mask[INPUT_DOWN] = (maskBits >> 8) & 1;
            mask[INPUT_LEFT] = (maskBits >> 16) & 1;
            mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
        };


#endif

        /**
 * 移動入力処理 v6 最低遅延版.
 *
 *
 * @note x86_64向けに分岐最小化とアクセス回数削減を徹底.
 *       読み取りはtestBitで確定値取得. 書き込みはsetBit/clearBitで確定反映.
 *       snapTapの優先ロジックはビット演算で維持し、水平/垂直の競合は同値判定で分岐レスに処理.
 *       通常モードの同時押しキャンセルは既存LUT表現を厳守.
 *       snapTapでは新規押下が競合時に優先側を上書き保持.
 * .
 */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // 反転型マスクLUT定義(0=有効,1=無効 として各出力ビットのLSBを使用するための値で構成)
            // (後続のLUTロード時のキャッシュ効率確保と即時抽出のため)
            alignas(64) static constexpr uint32_t MaskLUT[16] = {
                // 入力ビット配列: [bit0=UP, bit1=DOWN, bit2=LEFT, bit3=RIGHT]
                // 各バイト順序: [UP(byte0), DOWN(byte1), LEFT(byte2), RIGHT(byte3)]
                // 各バイトのLSB(1bit)のみ評価する構成
                // 0000
                0x0F0F0F0F,
                // 0001
                0x0F0F0F0E,
                // 0010
                0x0F0F0E0F,
                // 0011 ←→キャンセル
                0x0F0F0F0F,
                // 0100
                0x0F0E0F0F,
                // 0101
                0x0F0E0F0E,
                // 0110
                0x0F0E0E0F,
                // 0111
                0x0F0E0F0F,
                // 1000
                0x0E0F0F0F,
                // 1001
                0x0E0F0F0E,
                // 1010
                0x0E0F0E0F,
                // 1011
                0x0E0F0F0F,
                // 1100 ↑↓キャンセル
                0x0F0F0F0F,
                // 1101
                0x0F0F0F0E,
                // 1110
                0x0F0F0E0F,
                // 1111 全キャンセル
                0x0F0F0F0F
            };

            // SnapTap状態格納(下位8bit=直近入力, 上位8bit=優先保持)
            // (フレーム間で切り替え優先度を維持するため)
            static uint16_t snapState = 0;

            // 入力読み取り(演算前に確定値取得のため)
            const uint32_t f = hk.testBit(HK_MetroidMoveForward);
            // 入力読み取り(演算前に確定値取得のため)
            const uint32_t b = hk.testBit(HK_MetroidMoveBack);
            // 入力読み取り(演算前に確定値取得のため)
            const uint32_t l = hk.testBit(HK_MetroidMoveLeft);
            // 入力読み取り(演算前に確定値取得のため)
            const uint32_t r = hk.testBit(HK_MetroidMoveRight);

            // 4方向の現在ビット生成(後段LUT索引用ビット圧縮のため)
            const uint32_t curr = (f) | (b << 1) | (l << 2) | (r << 3);

            // 通常モード判定(分岐予測命中率向上のため)
            if (__builtin_expect(!isSnapTapMode, 1)) {
                // LUTロード(即時マスク決定のため)
                const uint32_t mb = MaskLUT[curr];

                // 出力UP確定(LSB評価で分岐レス化のため)
                (mb & 0x00000001) ? mask.setBit(INPUT_UP) : mask.clearBit(INPUT_UP);
                // 出力DOWN確定(LSB評価で分岐レス化のため)
                ((mb >> 8) & 0x01) ? mask.setBit(INPUT_DOWN) : mask.clearBit(INPUT_DOWN);
                // 出力LEFT確定(LSB評価で分岐レス化のため)
                ((mb >> 16) & 0x01) ? mask.setBit(INPUT_LEFT) : mask.clearBit(INPUT_LEFT);
                // 出力RIGHT確定(LSB評価で分岐レス化のため)
                ((mb >> 24) & 0x01) ? mask.setBit(INPUT_RIGHT) : mask.clearBit(INPUT_RIGHT);

                // 早期return(無駄な計算回避のため)
                return;
            }

            // 直近状態抽出(優先制御に使用するため)
            const uint32_t last = snapState & 0xFFu;
            // 優先保持抽出(競合時の優先を保持するため)
            const uint32_t priority = snapState >> 8;

            // 新規押下検出(前フレームとの差分抽出のため)
            const uint32_t newPress = curr & ~last;

            // 水平競合検出(== 3 を分岐レスで表現するため)
            const uint32_t h3 = (curr & 0x3u);
            // 水平競合マスク生成(完全一致時のみ3を返すため)
            const uint32_t hConflict = (h3 ^ 0x3u) ? 0u : 0x3u;

            // 垂直競合検出(== 12 を分岐レスで表現するため)
            const uint32_t v12 = (curr & 0xCu);
            // 垂直競合マスク生成(完全一致時のみ12を返すため)
            const uint32_t vConflict = (v12 ^ 0xCu) ? 0u : 0xCu;

            // 総合競合マスク作成(水平垂直の論理和で一括処理のため)
            const uint32_t conflict = vConflict | hConflict;

            // 競合発生時の更新フラグ生成(新規押下が競合線上に存在する場合のみ1化するため)
            const uint32_t updateMask = -((newPress & conflict) != 0u);

            // 新優先の計算(PR=既存優先を該当軸でクリアし、新規押下を該当軸のみで立てるため)
            const uint32_t newPriority =
                (priority & ~(conflict & updateMask)) | (newPress & conflict & updateMask);

            // 現在入力に限定した優先(押下継続時のみ優先を有効化するため)
            const uint32_t activePriority = newPriority & curr;

            // 状態更新(次フレームの差分検出と優先保持のため)
            snapState = static_cast<uint16_t>((curr & 0xFFu) | ((activePriority & 0xFFu) << 8));

            // 競合軸の最終入力決定(非競合はそのまま、競合は優先側のみ残すため)
            const uint32_t final = (curr & ~conflict) | (activePriority & conflict);

            // LUTロード(確定入力をマスク出力に変換するため)
            const uint32_t mb = MaskLUT[final];

            // 出力UP確定(LSB評価で分岐レス化のため)
            (mb & 0x00000001) ? mask.setBit(INPUT_UP) : mask.clearBit(INPUT_UP);
            // 出力DOWN確定(LSB評価で分岐レス化のため)
            ((mb >> 8) & 0x01) ? mask.setBit(INPUT_DOWN) : mask.clearBit(INPUT_DOWN);
            // 出力LEFT確定(LSB評価で分岐レス化のため)
            ((mb >> 16) & 0x01) ? mask.setBit(INPUT_LEFT) : mask.clearBit(INPUT_LEFT);
            // 出力RIGHT確定(LSB評価で分岐レス化のため)
            ((mb >> 24) & 0x01) ? mask.setBit(INPUT_RIGHT) : mask.clearBit(INPUT_RIGHT);
        };

    // /processMoveInputFunction }









#if defined(_WIN32)
        // RawMouseInput
        //RawInputThread* rawInputThread = new RawInputThread(parent());
        //rawInputThread->start();

// ヘッダ参照(クラス宣言のため)
#include "RawInputWinFilter.h"
// アプリケーション参照(QCoreApplicationのため)
#include <QCoreApplication>

// 静的ポインタ定義(単一インスタンス保持のため)
        static RawInputWinFilter* g_rawFilter = nullptr;

        // どこか一度だけ(例: EmuThread::run の前段やMainWindow生成時)
        if (!g_rawFilter) {
            g_rawFilter = new RawInputWinFilter();
            qApp->installNativeEventFilter(g_rawFilter);
        }
#endif

    /**
     * Aim input processing (QCursor-based, structure-preserving, low-latency, drift-prevention version).
     *
     * @note Minimizes hot-path branching and reduces QPoint copying.
     *       Sensitivity recalculation is performed only once via flag monitoring.
     *       AIM_ADJUST is fixed as a safe single-evaluation macro with lightweight ±1 range snapping.
     */
    static const auto processAimInput = [&]() __attribute__((hot, always_inline, flatten)) {
#ifndef STYLUS_MODE
        // Structure definition for aim processing (to improve cache locality)
        struct alignas(64) {
            // Store center X coordinate (maintain origin for delta calculation)
            int centerX;
            // Store center Y coordinate (maintain origin for delta calculation)
            int centerY;
            // Store X-axis sensitivity factor (speed up scaling)
            float sensitivityFactor;
            // Store combined Y-axis sensitivity (reduce multiplication operations)
            float combinedSensitivityY;
        } static aimData = { 0, 0, 0.01f, 0.013333333f };

        // 感度更新マクロ定義(重複コード排除と単一責務化のため)
#define UPDATE_SENSITIVITY(localCfg, aimData, isSensitivityChangePending)               \
    do {                                                                               \
        /* 変更待ち判定(不要計算回避のため) */                                         \
        if (__builtin_expect(isSensitivityChangePending, 0)) {                         \
            /* 感度値取得(設定値を適用するため) */                                     \
            const int sens = (localCfg).GetInt("Metroid.Sensitivity.Aim");             \
            /* Y軸倍率取得(ユーザー設定を反映するため) */                              \
            const float aimYAxisScale = static_cast<float>(                           \
                (localCfg).GetDouble("Metroid.Sensitivity.AimYAxisScale")              \
            );                                                                         \
            /* X感度更新(スケーリング係数を設定するため) */                           \
            (aimData).sensitivityFactor = sens * 0.01f;                                \
            /* Y感度更新(X感度との積で計算を簡略化するため) */                         \
            (aimData).combinedSensitivityY = (aimData).sensitivityFactor * aimYAxisScale; \
            /* フラグクリア(冗長な再計算を防ぐため) */                                \
            isSensitivityChangePending = false;                                        \
        }                                                                              \
    } while (0)

        // Define macro to prevent drift by rounding (single evaluation and snap within tolerance range)
#define AIM_ADJUST(v)                                        \
        ({                                                       \
            /* Store input value (to prevent multiple evaluations) */ \
            float _v = (v);                                      \
            /* Convert to int scaled by 10 (make threshold comparison constant-domain) */ \
            int _vi = static_cast<int>(_v * 10.001f);            \
            /* Snap positive range (fix ±1 for values in 0.5–0.9 range) */ \
            (_vi >= 5 && _vi <= 9) ? 1 :                         \
            /* Snap negative range (fix ±1 for values in -0.9–-0.5 range) */ \
            (_vi >= -9 && _vi <= -5) ? -1 :                      \
            /* Otherwise truncate (to suppress drift) */         \
            static_cast<int16_t>(_v);                            \
        })



// Hot path branch (fast processing when focus is maintained and layout is unchanged)

        if (__builtin_expect(!isLayoutChangePending && wasLastFrameFocused, 1)) {
			// フォーカス時かつレイアウト変更なしの場合の処理
            int deltaX = 0, deltaY = 0;


#if defined(_WIN32)
            /* ==== Raw Input 経路==== */
            do {
                // 設定でRaw Inputが有効なら、WM_INPUT由来の相対デルタだけで処理して早期return
                // emuInstance->osdAddMessage(0, "raw");

                g_rawFilter->fetchMouseDelta(deltaX, deltaY);
            } while (0);


#else
            // Get current mouse coordinates (for delta calculation input)
            const QPoint currentPos = QCursor::pos();
            // Extract X coordinate (early retrieval from QPoint)
            const int posX = currentPos.x();
            // Extract Y coordinate (early retrieval from QPoint)
            const int posY = currentPos.y();

            // Calculate X delta (preserve raw value before scaling)
            deltaX = posX - aimData.centerX;
            // Calculate Y delta (preserve raw value before scaling)
            deltaY = posY - aimData.centerY;
#endif

            // Early exit if no movement (prevent unnecessary processing)
            //if (!(deltaX | deltaY)) return;
            if ((deltaX | deltaY) == 0) return;

            // 感度更新呼び出し(フラグ監視により一度だけ再計算するため)
            // 感度更新実行(必要なときだけ再計算するため)
            UPDATE_SENSITIVITY(localCfg, aimData, isSensitivityChangePending);

            // Calculate X scaling (apply sensitivity)
            const float scaledX = deltaX * aimData.sensitivityFactor;
            // Calculate Y scaling (apply sensitivity)
            const float scaledY = deltaY * aimData.combinedSensitivityY;

            // Calculate X output adjustment (prevent drift and snap to ±1)
            const int16_t outputX = AIM_ADJUST(scaledX);
            // Calculate Y output adjustment (prevent drift and snap to ±1)
            const int16_t outputY = AIM_ADJUST(scaledY);


            // NDS書き込み（ndsポインタ一時化で間接参照削減）
            // static NDS* const __restrict nds = emuInstance->nds;
            emuInstance->nds->ARM9Write16(addrAimX, outputX);
            emuInstance->nds->ARM9Write16(addrAimY, outputY);

            // Set aim enable flag (for conditional processing downstream)
            enableAim = true;


#if defined(_WIN32)
            if(!(emuInstance->getMainWindow()->isFullScreen())){
                // Return cursor to center (keep next delta calculation zero-based)
                QCursor::setPos(aimData.centerX, aimData.centerY);
            }
#else
            // Return cursor to center (keep next delta calculation zero-based)
            QCursor::setPos(aimData.centerX, aimData.centerY);
#endif
            // End processing (avoid unnecessary branching)
            return;
        }

        // Recalculate center coordinates (for layout changes and initialization)
        const QPoint center = getAdjustedCenter();
        // Update center X (set origin for next delta calculation)
        aimData.centerX = center.x();
        // Update center Y (set origin for next delta calculation)
        aimData.centerY = center.y();

        // Set initial cursor position (for visual consistency and zeroing delta)
        QCursor::setPos(center);
#if defined(_WIN32)
        // RAW累積捨て呼び出し(センタリング直後の残存デルタ排除のため)
        // フォーカスイン時にsetPosで中央に戻す処理が動くため、移動距離が強制で大きくなってしまうため
        g_rawFilter->discardDeltas();
#endif
        // Clear layout change flag (to return to hot path)
        isLayoutChangePending = false;

        // 感度更新呼び出し(設定変更を即時反映するため)
        UPDATE_SENSITIVITY(localCfg, aimData, isSensitivityChangePending);

#else
        // スタイラス押下分岐(タッチ入力直通処理のため)
        if (__builtin_expect(emuInstance->isTouching, 1)) {
            // タッチ送出(座標反映のため)
            emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
        }
        // 非押下分岐(タッチ解放反映のため)
        else {
            // 画面解放(入力状態リセットのため)
            emuInstance->nds->ReleaseScreen();
        }
#endif
    };



    // Define a lambda function to switch weapons
    static const auto SwitchWeapon = [&](int weaponIndex) __attribute__((hot, always_inline)) {

        // Check for Already equipped
        if (emuInstance->nds->ARM9Read8(addrSelectedWeapon) == weaponIndex) {
            // emuInstance->osdAddMessage(0, "Weapon switch unnecessary: Already equipped");
            return; // Early return if the weapon is already equipped
        }

        if (isInAdventure) {

            // Check isMapOrUserActionPaused, for the issue "If you switch weapons while the map is open, the aiming mechanism may become stuck."
            if (isPaused) {
                return;
            }
            else if (emuInstance->nds->ARM9Read8(addrIsInVisorOrMap) == 0x1) {
                // isInVisor

                // Prevent visual glitches during weapon switching in visor mode
                return;
            }
        }

        // Read the current jump flag value
        uint8_t currentJumpFlags = emuInstance->nds->ARM9Read8(addrJumpFlag);

        // Check if the upper 4 bits are odd (1 or 3)
        // this is for fixing issue: Shooting and transforming become impossible, when changing weapons at high speed while transitioning from transformed to normal form.
        bool isTransforming = currentJumpFlags & 0x10;

        uint8_t jumpFlag = currentJumpFlags & 0x0F;  // Get the lower 4 bits
        //emuInstance->osdAddMessage(0, ("JumpFlag:" + std::string(1, "0123456789ABCDEF"[emuInstance->nds->ARM9Read8(addrJumpFlag) & 0x0F])).c_str());

        bool isRestoreNeeded = false;

        // Check if in alternate form (transformed state)
        isAltForm = emuInstance->nds->ARM9Read8(addrIsAltForm) == 0x02;

        // If not jumping (jumpFlag == 0) and in normal form, temporarily set to jumped state (jumpFlag == 1)
        if (!isTransforming && jumpFlag == 0 && !isAltForm) {
            // Leave the upper 4 bits of currentJumpFlags as they are and set the lower 4 bits to 0x01
            emuInstance->nds->ARM9Write8(addrJumpFlag, (currentJumpFlags & 0xF0) | 0x01);
            isRestoreNeeded = true;
            //emuInstance->osdAddMessage(0, ("JumpFlag:" + std::string(1, "0123456789ABCDEF"[emuInstance->nds->ARM9Read8(addrJumpFlag) & 0x0F])).c_str());
            //emuInstance->osdAddMessage(0, "Done setting jumpFlag.");
        }

        // Leave the upper 4 bits of addrWeaponChange as they are, and set only the lower 4 bits to 1011 (B in hexadecimal)
        emuInstance->nds->ARM9Write8(addrWeaponChange, (emuInstance->nds->ARM9Read8(addrWeaponChange) & 0xF0) | 0x0B); // Only change the lower 4 bits to B

        // Change the weapon
        emuInstance->nds->ARM9Write8(addrSelectedWeapon, weaponIndex);  // Write the address of the corresponding weapon

        // Release the screen (for weapon change)
        emuInstance->nds->ReleaseScreen();

        // Advance frames (for reflection of ReleaseScreen, WeaponChange)
        frameAdvanceTwice();

        // Need Touch after ReleaseScreen for aiming.
#ifndef STYLUS_MODE
        emuInstance->nds->TouchScreen(128, 88);
#else
        if (emuInstance->isTouching) {
            emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
        }
#endif

        // Advance frames (for reflection of Touch. This is necessary for no jump)
        frameAdvanceTwice();

        // Restore the jump flag to its original value (if necessary)
        if (isRestoreNeeded) {
            currentJumpFlags = emuInstance->nds->ARM9Read8(addrJumpFlag);
            // Create and set a new value by combining the upper 4 bits of currentJumpFlags and the lower 4 bits of jumpFlag
            emuInstance->nds->ARM9Write8(addrJumpFlag, (currentJumpFlags & 0xF0) | jumpFlag);
            //emuInstance->osdAddMessage(0, ("JumpFlag:" + std::string(1, "0123456789ABCDEF"[emuInstance->nds->ARM9Read8(addrJumpFlag) & 0x0F])).c_str());
            //emuInstance->osdAddMessage(0, "Restored jumpFlag.");

        }

        };

    static const QBitArray& hotkeyMask = emuInstance->hotkeyMask;
    static QBitArray& inputMask = emuInstance->inputMask;
    static const QBitArray& hotkeyPress = emuInstance->hotkeyPress;

#define TOUCH_IF(PRESS, X, Y) if (hotkeyPress.testBit(PRESS)) { emuInstance->nds->ReleaseScreen(); frameAdvanceTwice(); emuInstance->nds->TouchScreen(X, Y); frameAdvanceTwice(); }


    while (emuStatus != emuStatus_Exit) {

        // MelonPrimeDS Functions START

#ifndef STYLUS_MODE
        // Mouse player
        bool isFocused = emuInstance->getMainWindow()->panel->getFocused();
#else
        // isStylus
        bool isFocused = true;
#endif


        if (!isRomDetected) {
            detectRomAndSetAddresses(emuInstance);
        }
        // No "else" here, cuz flag will be changed after detecting.

        if (__builtin_expect(isRomDetected, 1)) {
            isInGame = emuInstance->nds->ARM9Read16(addrInGame) == 0x0001;

            // Determine whether it is cursor mode in one place
            bool shouldBeCursorMode = !isInGame || (isInAdventure && isPaused);

            if (isInGame && !hasInitialized) {
                // Run once at game start

                // Set the initialization complete flag
                hasInitialized = true;

                // updateRenderer because of using softwareRenderer when not in Game.
                videoRenderer = emuInstance->getGlobalConfig().GetInt("3D.Renderer");
                updateRenderer();

                /* MelonPrimeDS test
                if (vsyncFlag) {
                    emuInstance->osdAddMessage(0, "Vsync is enabled.");
                }
                else {
                    emuInstance->osdAddMessage(0, "Vsync is disabled.");
                }
                */

                // Read the player position
                playerPosition = emuInstance->nds->ARM9Read8(addrPlayerPos);

                // get addresses
                addrIsAltForm = calculatePlayerAddress(addrBaseIsAltForm, playerPosition, incrementOfPlayerAddress);
                addrLoadedSpecialWeapon = calculatePlayerAddress(addrBaseLoadedSpecialWeapon, playerPosition, incrementOfPlayerAddress);
                addrChosenHunter = calculatePlayerAddress(addrBaseChosenHunter, playerPosition, 0x01);
                addrWeaponChange = calculatePlayerAddress(addrBaseWeaponChange, playerPosition, incrementOfPlayerAddress);

                addrSelectedWeapon = calculatePlayerAddress(addrBaseSelectedWeapon, playerPosition, incrementOfPlayerAddress); // 020DCAA3 in JP1.0
                addrCurrentWeapon = addrSelectedWeapon - 0x1; // 020DCAA2 in JP1.0
                addrHavingWeapons = addrSelectedWeapon + 0x3; // 020DCAA6 in JP1.0

                addrWeaponAmmo = addrSelectedWeapon - 0x383; // 020D720 in JP1.0 current weapon ammo. DC722 is for MissleAmmo. can read both with read32.
                addrJumpFlag = calculatePlayerAddress(addrBaseJumpFlag, playerPosition, incrementOfPlayerAddress);

                // getaddrChosenHunter
                addrChosenHunter = calculatePlayerAddress(addrBaseChosenHunter, playerPosition, 0x01);
                uint8_t hunterID = emuInstance->nds->ARM9Read8(addrChosenHunter); // Perform memory read only once
                isSamus = hunterID == 0x00;
                isWeavel = hunterID == 0x06;

                /*
                Hunter IDs:
                00 - Samus
                01 - Kanden
                02 - Trace
                03 - Sylux
                04 - Noxus
                05 - Spire
                06 - Weavel
                */

                addrBoostGauge = addrIsAltForm + 0x44;
                addrIsBoosting = addrIsAltForm + 0x46;

                // aim addresses
                addrAimX = calculatePlayerAddress(addrBaseAimX, playerPosition, incrementOfAimAddr);
                addrAimY = calculatePlayerAddress(addrBaseAimY, playerPosition, incrementOfAimAddr);


                isInAdventure = emuInstance->nds->ARM9Read8(addrIsInAdventure) == 0x02;

                // isPrimeHunterAddr = addrIsInAdventure + 0xAD; // isPrimeHunter Addr NotPrimeHunter:0xFF, PrimeHunter:0x00 220E9AE9 in JP1.0

                // emuInstance->osdAddMessage(0, "Completed address calculation.");
            }

            if (isFocused) {


                // Calculate for aim 
                // updateMouseRelativeAndRecenterCursor
                // 
                // Handle the case when the window is focused
                // Update mouse relative position and recenter cursor for aim control

                if (__builtin_expect(isInGame, 1)) {
                    // inGame

                    /*
                    * doing this in Screen.cpp
                    if(!wasLastFrameFocused){
                        showCursorOnMelonPrimeDS(false);
                    }
                    */

                    if (!isCursorMode) {
                        processAimInput();
                    }

                    // Move hunter
                    processMoveInput(hotkeyMask, inputMask);

                    // Shoot
                    inputMask.setBit(INPUT_L, !(hotkeyMask.testBit(HK_MetroidShootScan) | hotkeyMask.testBit(HK_MetroidScanShoot)));

                    // Zoom, map zoom out
                    inputMask.setBit(INPUT_R, !hotkeyMask.testBit(HK_MetroidZoom));

                    // Jump
                    inputMask.setBit(INPUT_B, !hotkeyMask.testBit(HK_MetroidJump));

                    // Alt-form
                    TOUCH_IF(HK_MetroidMorphBall, 231, 167) 

                    // Compile-time constants
                    static constexpr uint8_t WEAPON_ORDER[] = { 0, 2, 7, 6, 5, 4, 3, 1, 8 };
                    static constexpr uint16_t WEAPON_MASKS[] = { 0x001, 0x004, 0x080, 0x040, 0x020, 0x010, 0x008, 0x002, 0x100 };
                    static constexpr uint8_t MIN_AMMO[] = { 0, 0x5, 0xA, 0x4, 0x14, 0x5, 0xA, 0xA, 0 };
                    static constexpr uint8_t WEAPON_INDEX_MAP[9] = { 0, 7, 1, 6, 5, 4, 3, 2, 8 };
                    static constexpr uint8_t WEAPON_COUNT = 9;

                    // Hotkey mapping
                    static constexpr struct {
                        int hotkey;
                        uint8_t weapon;
                    } HOTKEY_MAP[] = {
                        { HK_MetroidWeaponBeam, 0 },
                        { HK_MetroidWeaponMissile, 2 },
                        { HK_MetroidWeapon1, 7 },
                        { HK_MetroidWeapon2, 6 },
                        { HK_MetroidWeapon3, 5 },
                        { HK_MetroidWeapon4, 4 },
                        { HK_MetroidWeapon5, 3 },
                        { HK_MetroidWeapon6, 1 },
                        { HK_MetroidWeaponSpecial, 0xFF }
                    };

                    // Branch prediction hints
                    #define likely(x)   __builtin_expect(!!(x), 1)
                    #define unlikely(x) __builtin_expect(!!(x), 0)

                    // Main lambda for weapon switching
                    static const auto processWeaponSwitch = [&]() -> bool {
                        bool weaponSwitched = false;

                        // Lambda: Gather hotkey states efficiently
                        static const auto gatherHotkeyStates = [&]() -> uint32_t {
                            uint32_t states = 0;
                            for (size_t i = 0; i < 9; ++i) {
                                // if (hotkeyPressed(HOTKEY_MAP[i].hotkey)) {
                                //if (hotkeyPress.testBit(HOTKEY_MAP[i].hotkey)) {
                                //    states |= (1u << i);
                                //}
                                states |= static_cast<uint32_t>(hotkeyPress.testBit(HOTKEY_MAP[i].hotkey)) << i;
                            }
                            return states;
                            };

                        // Lambda: Process hotkeys
                        static const auto processHotkeys = [&](uint32_t hotkeyStates) -> bool {
                            if (!hotkeyStates) return false;

                            const int firstSet = __builtin_ctz(hotkeyStates);

                            if (unlikely(firstSet == 8)) {
                                const uint8_t special = emuInstance->nds->ARM9Read8(addrLoadedSpecialWeapon);
                                if (special != 0xFF) {
                                    SwitchWeapon(special);
                                    return true;
                                }
                            }
                            else {
                                SwitchWeapon(HOTKEY_MAP[firstSet].weapon);
                                return true;
                            }
                            return false;
                            };

                        // Lambda: Calculate available weapons
                        static const auto getAvailableWeapons = [&]() -> uint16_t {
                            // Batch read weapon data
                            const uint16_t having = emuInstance->nds->ARM9Read16(addrHavingWeapons);
                            const uint32_t ammoData = emuInstance->nds->ARM9Read32(addrWeaponAmmo);
                            const uint16_t missileAmmo = ammoData >> 16;
                            const uint16_t weaponAmmo = ammoData & 0xFFFF;

                            uint16_t available = 0;

                            // Check each weapon
                            for (int i = 0; i < WEAPON_COUNT; ++i) {
                                const uint8_t weapon = WEAPON_ORDER[i];
                                const uint16_t mask = WEAPON_MASKS[i];

                                // Check if owned
                                if (!(having & mask)) continue;

                                // Check ammo requirements
                                bool hasAmmo = true;
                                if (weapon == 2) {
                                    hasAmmo = missileAmmo >= 0xA;
                                }
                                else if (weapon > 2 && weapon < 8) {
                                    uint8_t required = MIN_AMMO[weapon];
                                    if (weapon == 3 && isWeavel) {
                                        required = 0x5;
                                    }
                                    hasAmmo = weaponAmmo >= required;
                                }

                                if (hasAmmo) {
                                    available |= (1u << i);
                                }
                            }

                            return available;
                            };

                        // Lambda: Find next available weapon
                        static const auto findNextWeapon = [&](uint8_t current, bool forward, uint16_t available) -> int {
                            if (!available) return -1;  // No weapons available

                            uint8_t startIndex = WEAPON_INDEX_MAP[current];
                            uint8_t index = startIndex;

                            // Search for next available weapon
                            for (int attempts = 0; attempts < WEAPON_COUNT; ++attempts) {
                                if (forward) {
                                    index = (index + 1) % WEAPON_COUNT;
                                }
                                else {
                                    index = (index + WEAPON_COUNT - 1) % WEAPON_COUNT;
                                }

                                if (available & (1u << index)) {
                                    return WEAPON_ORDER[index];
                                }
                            }

                            return -1;  // Should not reach here if available != 0
                            };

                        // Lambda: Process wheel and navigation keys
                        static const auto processWheelInput = [&]() -> bool {
                            const int wheelDelta = emuInstance->getMainWindow()->panel->getDelta();
                            const bool nextKey = hotkeyPress.testBit(HK_MetroidWeaponNext);
                            const bool prevKey = hotkeyPress.testBit(HK_MetroidWeaponPrevious);

                            if (!wheelDelta && !nextKey && !prevKey) return false;

                            const bool forward = (wheelDelta < 0) || nextKey;
                            const uint8_t current = emuInstance->nds->ARM9Read8(addrCurrentWeapon);
                            const uint16_t available = getAvailableWeapons();

                            int nextWeapon = findNextWeapon(current, forward, available);
                            if (nextWeapon >= 0) {
                                SwitchWeapon(static_cast<uint8_t>(nextWeapon));
                                return true;
                            }

                            return false;
                            };

                        // Main execution flow
                        const uint32_t hotkeyStates = gatherHotkeyStates();

                        // Try hotkeys first
                        if (processHotkeys(hotkeyStates)) {
                            return true;
                        }

                        // Try wheel/navigation if no hotkey was pressed
                        if (processWheelInput()) {
                            return true;
                        }

                        return false;
                        };

                    // Execute the weapon switch logic
                    bool weaponSwitched = processWeaponSwitch();


                    // Weapon check {
                    /*
                    // TODO 終了時の武器選択を防ぐ。　マグモールになってしまう
                    static bool isWeaponCheckActive = false;

                    if (emuInstance->hotkeyDown(HK_MetroidWeaponCheck)) {
                        if (!isWeaponCheckActive) {
                            // 初回のみ実行
                            isWeaponCheckActive = true;
                            emuInstance->nds->ReleaseScreen();
                            frameAdvanceTwice();
                        }

                        // キーが押されている間は継続
                        emuInstance->nds->TouchScreen(236, 30);
                        frameAdvanceTwice();

                        // still allow movement
                        processMoveInput(hotkeyMask, inputMask);
                    }
                    else {
                        if (isWeaponCheckActive) {
                            // キーが離されたときの終了処理
                            emuInstance->nds->ReleaseScreen();
                            frameAdvanceTwice();
                            frameAdvanceTwice();
                            isWeaponCheckActive = false;
                        }
                    }
                    */
                    // } Weapon check


                    // Morph ball boost
                    // INFO If this function is not used, mouse boosting can only be done once.
                    // This is because it doesn't release from the touch state, which is necessary for aiming. 
                    // There's no way around it.
                    if (isSamus && hotkeyMask.testBit(HK_MetroidHoldMorphBallBoost))
                    {
                        isAltForm = emuInstance->nds->ARM9Read8(addrIsAltForm) == 0x02;
                        if (isAltForm) {
                            uint8_t boostGaugeValue = emuInstance->nds->ARM9Read8(addrBoostGauge);
                            bool isBoosting = emuInstance->nds->ARM9Read8(addrIsBoosting) != 0x00;

                            // boostable when gauge value is 0x05-0x0F(max)
                            bool isBoostGaugeEnough = boostGaugeValue > 0x0A;

                            // just incase
                            enableAim = false;

                            // release for boost?
                            emuInstance->nds->ReleaseScreen();

                            // Boost input determination (true during boost, false during charge)
                            inputMask.setBit(INPUT_R, (!isBoosting && isBoostGaugeEnough));

                            if (isBoosting) {
                                // touch again for aiming
#ifndef STYLUS_MODE
                                emuInstance->nds->TouchScreen(128, 88);
#else
                                if (emuInstance->isTouching) {
                                    emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
                                }
#endif
                            }

                        }
                    }

                    if (__builtin_expect(isInAdventure, 0)) {
                        // Adventure Mode Functions

                        // To determine the state of pause or user operation stop (to detect the state of map or action pause)
                        isPaused = emuInstance->nds->ARM9Read8(addrIsMapOrUserActionPaused) == 0x1;

                        // Scan Visor
                        if (hotkeyPress.testBit(HK_MetroidScanVisor)) {
                            emuInstance->nds->ReleaseScreen();
                            frameAdvanceTwice();

                            // emuInstance->osdAddMessage(0, "in visor %d", inVisor);

                            emuInstance->nds->TouchScreen(128, 173);

                            if (emuInstance->nds->ARM9Read8(addrIsInVisorOrMap) == 0x1) {
                                // isInVisor
                                frameAdvanceTwice();
                            }
                            else {
                                for (int i = 0; i < 30; i++) {
                                    // still allow movement whilst we're enabling scan visor
                                    processAimInput();
                                    processMoveInput(hotkeyMask, inputMask); 

                                    //emuInstance->nds->SetKeyMask(emuInstance->getInputMask());
                                    emuInstance->nds->SetKeyMask(GET_INPUT_MASK(inputMask));
                                    
                                    frameAdvanceOnce();
                                }
                            }

                            emuInstance->nds->ReleaseScreen();
                            frameAdvanceTwice();
                        }

                        TOUCH_IF(HK_MetroidUIOk, 128, 142) // OK (in scans and messages)
                        TOUCH_IF(HK_MetroidUILeft, 71, 141) // Left arrow (in scans and messages)
                        TOUCH_IF(HK_MetroidUIRight, 185, 141) // Right arrow (in scans and messages)
                        TOUCH_IF(HK_MetroidUIYes, 96, 142)  // Enter to Starship
                        TOUCH_IF(HK_MetroidUINo, 160, 142) // No Enter to Starship

                    } // End of Adventure Functions


#ifndef STYLUS_MODE
                    // Touch again for aiming
                    if (!wasLastFrameFocused || enableAim) {
                        // touch again for aiming
                        // When you return to melonPrimeDS or normal form

                        // emuInstance->osdAddMessage(0,"touching screen for aim");

                        // Changed Y point center(96) to 88, For fixing issue: Alt Tab switches hunter choice.
                        //emuInstance->nds->TouchScreen(128, 96); // required for aiming


                        emuInstance->nds->TouchScreen(128, 88); // required for aiming
                    }
#endif


                    // End of in-game
                }
                else {
                    // !isInGame

                    isInAdventure = false;

                    if (hasInitialized) {
                        hasInitialized = false;
                    }

                    // Resolve Menu flickering
                    if (videoRenderer != renderer3D_Software) {
                        videoRenderer = renderer3D_Software;
                        updateRenderer();
                    }

                    // L For Hunter License
                    inputMask.setBit(INPUT_L, !hotkeyPress.testBit(HK_MetroidUILeft));
                    // R For Hunter License
                    inputMask.setBit(INPUT_R, !hotkeyPress.testBit(HK_MetroidUIRight));

                    if (__builtin_expect(isRomDetected, 1)) {

                        // ヘッドフォン設定
                        ApplyHeadphoneOnce(emuInstance->nds, emuInstance->getLocalConfig(), addrOperationAndSound, isHeadphoneApplied);

                        // MPH感度設定適用処理呼び出し
                        ApplyMphSensitivity(emuInstance->nds, emuInstance->getLocalConfig(), addrSensitivity);
                        
                        // ハンター/マップ全開処理
                        ApplyUnlockHuntersMaps(
                            emuInstance->nds,
                            emuInstance->getLocalConfig(),
                            isUnlockMapsHuntersApplied,
                            addrUnlockMapsHunters,
                            addrUnlockMapsHunters2,
                            addrUnlockMapsHunters3,
                            addrUnlockMapsHunters4,
                            addrUnlockMapsHunters5
                        );

                        // DS名適用
                        useDsName(emuInstance->nds, emuInstance->getLocalConfig(), addrDsNameFlagAndMicVolume);

                        // ハンター適用
                        applySelectedHunterStrict(emuInstance->nds, emuInstance->getLocalConfig(), addrMainHunter);
						// ライセンスカラー適用
                        applyLicenseColorStrict(emuInstance->nds, emuInstance->getLocalConfig(), addrRankColor);
                    }

                }

                if (shouldBeCursorMode != isCursorMode) {
                    isCursorMode = shouldBeCursorMode;
#ifndef STYLUS_MODE
                    showCursorOnMelonPrimeDS(isCursorMode);
#endif
                }

                if (isCursorMode) {

                    if (emuInstance->isTouching) {
                        emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
                    }
                    else {
                        emuInstance->nds->ReleaseScreen();
                    }

                }

                // Start / View Match progress, points / Map(Adventure)
                inputMask.setBit(INPUT_START, !hotkeyMask.testBit(HK_MetroidMenu));


            }// END of if(isFocused)
            
            // Apply input
            // emuInstance->nds->SetKeyMask(emuInstance->getInputMask());
            emuInstance->nds->SetKeyMask(GET_INPUT_MASK(inputMask));

            // record last frame was forcused or not
            wasLastFrameFocused = isFocused;
        } // End of isRomDetected
 

        // MelonPrimeDS Functions END


        frameAdvanceOnce();

    } // End of while (emuStatus != emuStatus_Exit)
#if defined(_WIN32)
    //rawInputThread->quit(); //rawMouseInput
#endif

}

void EmuThread::sendMessage(Message msg)
{
    msgMutex.lock();
    msgQueue.enqueue(msg);
    msgMutex.unlock();
}

void EmuThread::waitMessage(int num)
{
    if (QThread::currentThread() == this) return;
    msgSemaphore.acquire(num);
}

void EmuThread::waitAllMessages()
{
    if (QThread::currentThread() == this) return;
    while (!msgQueue.empty())
        msgSemaphore.acquire();
}

void EmuThread::handleMessages()
{
    msgMutex.lock();
    while (!msgQueue.empty())
    {
        Message msg = msgQueue.dequeue();
        switch (msg.type)
        {
        case msg_Exit:
            emuStatus = emuStatus_Exit;
            emuPauseStack = emuPauseStackRunning;

            emuInstance->audioDisable();
            MPInterface::Get().End(emuInstance->instanceID);
            break;

        case msg_EmuRun:
            emuStatus = emuStatus_Running;
            emuPauseStack = emuPauseStackRunning;
            emuActive = true;

            emuInstance->audioEnable();
            emit windowEmuStart();
            break;

        case msg_EmuPause:
            emuPauseStack++;
            if (emuPauseStack > emuPauseStackPauseThreshold) break;

            prevEmuStatus = emuStatus;
            emuStatus = emuStatus_Paused;

            if (prevEmuStatus != emuStatus_Paused)
            {
                emuInstance->audioDisable();
                emit windowEmuPause(true);
                emuInstance->osdAddMessage(0, "Paused");
            }
            break;

        case msg_EmuUnpause:
            if (emuPauseStack < emuPauseStackPauseThreshold) break;

            emuPauseStack--;
            if (emuPauseStack >= emuPauseStackPauseThreshold) break;

            emuStatus = prevEmuStatus;

            if (emuStatus != emuStatus_Paused)
            {
                emuInstance->audioEnable();
                emit windowEmuPause(false);
                emuInstance->osdAddMessage(0, "Resumed");

                // MelonPrimeDS {
                // applyVideoSettings Immediately when resumed
                if (isInGame) {
                    // updateRenderer because of using softwareRenderer when not in Game.
                    videoRenderer = emuInstance->getGlobalConfig().GetInt("3D.Renderer");
                    updateRenderer();
                }

                // 感度値取得処理
                uint32_t sensiVal = emuInstance->getNDS()->ARM9Read16(addrSensitivity);
                double sensiNum = sensiValToSensiNum(sensiVal);

                // 感度値表示(ROM情報とは別に独立して表示するため)
                char message[256];
                sprintf(message, "INFO sensitivity of the game itself: 0x%04X(%.5f)", sensiVal, sensiNum);
                emuInstance->osdAddMessage(0, message);

                // reset Settings when unPaused
                isSnapTapMode = emuInstance->getLocalConfig().GetBool("Metroid.Operation.SnapTap"); // SnapTapリセット用
                isUnlockMapsHuntersApplied = false; // Unlockリセット用
                isSensitivityChangePending = true; // Aim感度リセット用
                // lastMphSensitivity = std::numeric_limits<double>::quiet_NaN(); // Mph感度リセット用
                isHeadphoneApplied = false; // ヘッドフォンリセット用

                // MelonPrimeDS }
            }
            break;

        case msg_EmuStop:
            if (msg.param.value<bool>())
                emuInstance->nds->Stop();
            emuStatus = emuStatus_Paused;
            emuActive = false;

            emuInstance->audioDisable();
            emit windowEmuStop();
            break;

        case msg_EmuFrameStep:
            emuStatus = emuStatus_FrameStep;
            break;

        case msg_EmuReset:
            emuInstance->reset();

            emuStatus = emuStatus_Running;
            emuPauseStack = emuPauseStackRunning;
            emuActive = true;

            emuInstance->audioEnable();
            emit windowEmuReset();
            emuInstance->osdAddMessage(0, "Reset");
            break;

        case msg_InitGL:
            emuInstance->initOpenGL(msg.param.value<int>());
            useOpenGL = true;
            break;

        case msg_DeInitGL:
            emuInstance->deinitOpenGL(msg.param.value<int>());
            if (msg.param.value<int>() == 0)
                useOpenGL = false;
            break;

        case msg_BootROM:
            msgResult = 0;
            if (!emuInstance->loadROM(msg.param.value<QStringList>(), true, msgError))
                break;

            assert(emuInstance->nds != nullptr);
            emuInstance->nds->Start();
            msgResult = 1;
            break;

        case msg_BootFirmware:
            msgResult = 0;
            if (!emuInstance->bootToMenu(msgError))
                break;

            assert(emuInstance->nds != nullptr);
            emuInstance->nds->Start();
            msgResult = 1;
            break;

        case msg_InsertCart:
            msgResult = 0;
            if (!emuInstance->loadROM(msg.param.value<QStringList>(), false, msgError))
                break;

            msgResult = 1;
            break;

        case msg_EjectCart:
            emuInstance->ejectCart();
            break;

        case msg_InsertGBACart:
            msgResult = 0;
            if (!emuInstance->loadGBAROM(msg.param.value<QStringList>(), msgError))
                break;

            msgResult = 1;
            break;

        case msg_InsertGBAAddon:
            msgResult = 0;
            emuInstance->loadGBAAddon(msg.param.value<int>(), msgError);
            msgResult = 1;
            break;

        case msg_EjectGBACart:
            emuInstance->ejectGBACart();
            break;

        case msg_SaveState:
            msgResult = emuInstance->saveState(msg.param.value<QString>().toStdString());
            break;

        case msg_LoadState:
            msgResult = emuInstance->loadState(msg.param.value<QString>().toStdString());
            break;

        case msg_UndoStateLoad:
            emuInstance->undoStateLoad();
            msgResult = 1;
            break;

        case msg_ImportSavefile:
            {
                msgResult = 0;
                auto f = Platform::OpenFile(msg.param.value<QString>().toStdString(), Platform::FileMode::Read);
                if (!f) break;

                u32 len = FileLength(f);

                std::unique_ptr<u8[]> data = std::make_unique<u8[]>(len);
                Platform::FileRewind(f);
                Platform::FileRead(data.get(), len, 1, f);

                assert(emuInstance->nds != nullptr);
                emuInstance->nds->SetNDSSave(data.get(), len);

                CloseFile(f);
                msgResult = 1;
            }
            break;

        case msg_EnableCheats:
            emuInstance->enableCheats(msg.param.value<bool>());
            break;
        }

        msgSemaphore.release();
    }
    msgMutex.unlock();
}

void EmuThread::changeWindowTitle(char* title)
{
    emit windowTitleChange(QString(title));
}

void EmuThread::initContext(int win)
{
    sendMessage({.type = msg_InitGL, .param = win});
    waitMessage();
}

void EmuThread::deinitContext(int win)
{
    sendMessage({.type = msg_DeInitGL, .param = win});
    waitMessage();
}

void EmuThread::emuRun()
{
    sendMessage(msg_EmuRun);
    waitMessage();
}

void EmuThread::emuPause(bool broadcast)
{
    sendMessage(msg_EmuPause);
    waitMessage();

    if (broadcast)
        emuInstance->broadcastCommand(InstCmd_Pause);
}

void EmuThread::emuUnpause(bool broadcast)
{
    sendMessage(msg_EmuUnpause);
    waitMessage();

    if (broadcast)
        emuInstance->broadcastCommand(InstCmd_Unpause);
}

void EmuThread::emuTogglePause(bool broadcast)
{
    if (emuStatus == emuStatus_Paused)
        emuUnpause(broadcast);
    else
        emuPause(broadcast);
}

void EmuThread::emuStop(bool external)
{
    sendMessage({.type = msg_EmuStop, .param = external});
    waitMessage();
}

void EmuThread::emuExit()
{
    sendMessage(msg_Exit);
    waitAllMessages();
}

void EmuThread::emuFrameStep()
{
    if (emuPauseStack < emuPauseStackPauseThreshold)
        sendMessage(msg_EmuPause);
    sendMessage(msg_EmuFrameStep);
    waitAllMessages();
}

void EmuThread::emuReset()
{
    sendMessage(msg_EmuReset);
    waitMessage();
}

bool EmuThread::emuIsRunning()
{
    return emuStatus == emuStatus_Running;
}

bool EmuThread::emuIsActive()
{
    return emuActive;
}

int EmuThread::bootROM(const QStringList& filename, QString& errorstr)
{
    sendMessage({.type = msg_BootROM, .param = filename});
    waitMessage();
    if (!msgResult)
    {
        errorstr = msgError;
        return msgResult;
    }

    sendMessage(msg_EmuRun);
    waitMessage();
    errorstr = "";
    return msgResult;
}

int EmuThread::bootFirmware(QString& errorstr)
{
    sendMessage(msg_BootFirmware);
    waitMessage();
    if (!msgResult)
    {
        errorstr = msgError;
        return msgResult;
    }

    sendMessage(msg_EmuRun);
    waitMessage();
    errorstr = "";
    return msgResult;
}

int EmuThread::insertCart(const QStringList& filename, bool gba, QString& errorstr)
{
    MessageType msgtype = gba ? msg_InsertGBACart : msg_InsertCart;

    sendMessage({.type = msgtype, .param = filename});
    waitMessage();
    errorstr = msgResult ? "" : msgError;
    return msgResult;
}

void EmuThread::ejectCart(bool gba)
{
    sendMessage(gba ? msg_EjectGBACart : msg_EjectCart);
    waitMessage();
}

int EmuThread::insertGBAAddon(int type, QString& errorstr)
{
    sendMessage({.type = msg_InsertGBAAddon, .param = type});
    waitMessage();
    errorstr = msgResult ? "" : msgError;
    return msgResult;
}

int EmuThread::saveState(const QString& filename)
{
    sendMessage({.type = msg_SaveState, .param = filename});
    waitMessage();
    return msgResult;
}

int EmuThread::loadState(const QString& filename)
{
    sendMessage({.type = msg_LoadState, .param = filename});
    waitMessage();
    return msgResult;
}

int EmuThread::undoStateLoad()
{
    sendMessage(msg_UndoStateLoad);
    waitMessage();
    return msgResult;
}

int EmuThread::importSavefile(const QString& filename)
{
    sendMessage(msg_EmuReset);
    sendMessage({.type = msg_ImportSavefile, .param = filename});
    waitMessage(2);
    return msgResult;
}

void EmuThread::enableCheats(bool enable)
{
    sendMessage({.type = msg_EnableCheats, .param = enable});
    waitMessage();
}

void EmuThread::updateRenderer()
{
    // MelonPrimeDS {

    // getVsyncFlag
    bool vsyncFlag = emuInstance->getGlobalConfig().GetBool("Screen.VSync");  // MelonPrimeDS
    // VSync Override
    emuInstance->setVSyncGL(vsyncFlag); // MelonPrimeDS

	// } MelonPrimeDS

    if (videoRenderer != lastVideoRenderer)
    {
        switch (videoRenderer)
        {
            case renderer3D_Software:
                emuInstance->nds->GPU.SetRenderer3D(std::make_unique<SoftRenderer>());
                break;
            case renderer3D_OpenGL:
                emuInstance->nds->GPU.SetRenderer3D(GLRenderer::New());
                break;
            case renderer3D_OpenGLCompute:
                emuInstance->nds->GPU.SetRenderer3D(ComputeRenderer::New());
                break;
            default: __builtin_unreachable();
        }
    }
    lastVideoRenderer = videoRenderer;

    auto& cfg = emuInstance->getGlobalConfig();
    switch (videoRenderer)
    {
        case renderer3D_Software:
            static_cast<SoftRenderer&>(emuInstance->nds->GPU.GetRenderer3D()).SetThreaded(
                    cfg.GetBool("3D.Soft.Threaded"),
                    emuInstance->nds->GPU);
            break;
        case renderer3D_OpenGL:
            static_cast<GLRenderer&>(emuInstance->nds->GPU.GetRenderer3D()).SetRenderSettings(
                    cfg.GetBool("3D.GL.BetterPolygons"),
                    cfg.GetInt("3D.GL.ScaleFactor"));
            break;
        case renderer3D_OpenGLCompute:
            static_cast<ComputeRenderer&>(emuInstance->nds->GPU.GetRenderer3D()).SetRenderSettings(
                    cfg.GetInt("3D.GL.ScaleFactor"),
                    cfg.GetBool("3D.GL.HiresCoordinates"));
            break;
        default: __builtin_unreachable();
    }
    // MelonPrimeDS {
    // VSync Override
    emuInstance->setVSyncGL(vsyncFlag); // MelonPrimeDS

    /* MelonPrimeDS test
    if (vsyncFlag) {
        emuInstance->osdAddMessage(0, "Vsync is enabled.");
    }
    else {
        emuInstance->osdAddMessage(0, "Vsync is disabled.");
    }
    */

    // } MelonPrimeDS
}

void EmuThread::compileShaders()
{
    int currentShader, shadersCount;
    u64 startTime = SDL_GetPerformanceCounter();
    // kind of hacky to look at the wallclock, though it is easier than
    // than disabling vsync
    do
    {
        emuInstance->nds->GPU.GetRenderer3D().ShaderCompileStep(currentShader, shadersCount);
    } while (emuInstance->nds->GPU.GetRenderer3D().NeedsShaderCompile() &&
             (SDL_GetPerformanceCounter() - startTime) * perfCountsSec < 1.0 / 6.0);
    emuInstance->osdAddMessage(0, "Compiling shader %d/%d", currentShader+1, shadersCount);
}
