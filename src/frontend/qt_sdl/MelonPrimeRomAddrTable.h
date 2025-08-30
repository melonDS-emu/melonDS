// インクルードガード定義(多重定義防止のため)
#ifndef MELON_PRIME_ROM_ADDR_TABLE_H
#define MELON_PRIME_ROM_ADDR_TABLE_H

// 固定幅整数型の導入(アドレス値の型統一のため)
#include <cstdint>
// 配列とサイズ計算の導入(定数テーブル構築のため)
#include <array>
// デバッグ時の範囲検証導入(不正グループの早期検出のため)
#include <cassert>

///**
/// ROMアドレス束を保持する構造体定義.
/// 
/// 
/// @brief 各ROMグループに対応する既知アドレスの集約.
///
struct RomAddrs
{
    // ChosenHunterアドレス格納(既存コードの変数名整合のため)
    uint32_t baseChosenHunterAddr;
    // inGameアドレス格納(既存コードの変数名整合のため)
    uint32_t inGameAddr;
    // プレイヤ位置アドレス格納(既存コードの変数名整合のため)
    uint32_t PlayerPosAddr;
    // AltFormアドレス格納(既存コードの変数名整合のため)
    uint32_t baseIsAltFormAddr;
    // 武器変更アドレス格納(既存コードの変数名整合のため)
    uint32_t baseWeaponChangeAddr;
    // 選択武器アドレス格納(既存コードの変数名整合のため)
    uint32_t baseSelectedWeaponAddr;
    // 照準Xアドレス格納(既存コードの変数名整合のため)
    uint32_t baseAimXAddr;
    // 照準Yアドレス格納(既存コードの変数名整合のため)
    uint32_t baseAimYAddr;
    // ADV/Multi判定アドレス格納(既存コードの変数名整合のため)
    uint32_t isInAdventureAddr;
    // マップ/ユーザ操作ポーズ判定アドレス格納(既存コードの変数名整合のため)
    uint32_t isMapOrUserActionPausedAddr;
    uint32_t unlockMapsHuntersAddr;
};

///**
/// ROMグループ列挙定義.
/// 
/// 
/// @brief 元switchの各グループ識別子を列挙で定義.
///
enum RomGroup : int {
    // US1.1グループ識別子定義(既存識別子互換のため)
    GROUP_US1_1 = 0,
    // US1.0グループ識別子定義(既存識別子互換のため)
    GROUP_US1_0,
    // EU1.1グループ識別子定義(既存識別子互換のため)
    GROUP_EU1_1,
    // EU1.0グループ識別子定義(既存識別子互換のため)
    GROUP_EU1_0,
    // JP1.0グループ識別子定義(既存識別子互換のため)
    GROUP_JP1_0,
    // JP1.1グループ識別子定義(既存識別子互換のため)
    GROUP_JP1_1,
    // KR1.0グループ識別子定義(既存識別子互換のため)
    GROUP_KR1_0,
    // 合計数ダミー定義(配列サイズ整合のため)
    GROUP_COUNT
};

//====================
// グローバルフラグ群
//====================

/*
// C++17 inline変数でヘッダに定義しても多重定義にならない
inline bool isGroupJP10 = false;
inline bool isGroupJP11 = false;
inline bool isGroupUS10 = false;
inline bool isGroupUS11 = false;
inline bool isGroupEU10 = false;
inline bool isGroupEU11 = false;
inline bool isGroupKR10 = false;

//====================
// フラグ操作関数
//====================

// 全フラグをfalseにリセット
inline void ClearRomGroupFlags() {
    isGroupJP10 = isGroupJP11 = false;
    isGroupUS10 = isGroupUS11 = false;
    isGroupEU10 = isGroupEU11 = false;
    isGroupKR10 = false;
}

// 指定されたグループに応じてフラグをtrueに設定
inline void SetRomGroupFlags(RomGroup g) {
    ClearRomGroupFlags();
    switch (g) {
    case GROUP_JP1_0: isGroupJP10 = true; break;
    case GROUP_JP1_1: isGroupJP11 = true; break;
    case GROUP_US1_0: isGroupUS10 = true; break;
    case GROUP_US1_1: isGroupUS11 = true; break;
    case GROUP_EU1_0: isGroupEU10 = true; break;
    case GROUP_EU1_1: isGroupEU11 = true; break;
    case GROUP_KR1_0: isGroupKR10 = true; break;
    default: break;
    }
}
*/

// JP1.0基準アドレス定義(他バージョン差分算出のため)
static constexpr uint32_t JP1_0_BASE_IS_ALT_FORM      = 0x020DC6D8;
// JP1.0基準アドレス定義(他バージョン差分算出のため)
static constexpr uint32_t JP1_0_BASE_WEAPON_CHANGE    = 0x020DCA9B;
// JP1.0基準アドレス定義(他バージョン差分算出のため)
static constexpr uint32_t JP1_0_BASE_SELECTED_WEAPON  = 0x020DCAA3;

// US/EU/JP/KRで共有される既知絶対アドレスの定義(表明瞭性のため)
static constexpr uint32_t ABS_US11_IS_ALT_FORM        = 0x020DB098;
// 共有既知絶対アドレス定義(表明瞭性のため)
static constexpr uint32_t ABS_US11_WEAPON_CHANGE      = 0x020DB45B;
// 共有既知絶対アドレス定義(表明瞭性のため)
static constexpr uint32_t ABS_US11_SELECTED_WEAPON    = 0x020DB463;

// グループ差分オフセット定義(保守容易性のため)
static constexpr uint32_t OFF_US10_FROM_JP10          = 0x1EC0;
// グループ差分オフセット定義(保守容易性のため)
static constexpr uint32_t OFF_EU11_FROM_JP10          = 0x15A0;
// グループ差分オフセット定義(保守容易性のため)
static constexpr uint32_t OFF_EU10_FROM_JP10          = 0x1620;
// グループ差分オフセット定義(保守容易性のため)
static constexpr uint32_t OFF_JP11_ALT_FROM_JP10      = 0x64;
// グループ差分オフセット定義(保守容易性のため)
static constexpr uint32_t OFF_JP11_WEAPON_FROM_JP10   = 0x40;
// グループ差分オフセット定義(保守容易性のため)
static constexpr uint32_t OFF_KR10_FROM_JP10          = 0x87F4;

// 事前計算テーブル定義(分岐削減と実行時演算排除のため)
static constexpr std::array<RomAddrs, GROUP_COUNT> kRomAddrTable = {{
    // GROUP_US1_1行定義(元switch絶対値の忠実反映のため)
    {
        // baseChosenHunterAddr設定
        0x020CBDA4,
        // inGameAddr設定  inGame:1
        0x020EEC40u + 0x8F0u,
        // PlayerPosAddr設定
        0x020DA538,
        // baseIsAltFormAddr設定
        ABS_US11_IS_ALT_FORM,
        // baseWeaponChangeAddr設定
        ABS_US11_WEAPON_CHANGE,
        // baseSelectedWeaponAddr設定
        ABS_US11_SELECTED_WEAPON,
        // baseAimXAddr設定
        0x020DEDA6,
        // baseAimYAddr設定
        0x020DEDAEu,
        // isInAdventureAddr設定 Read8 0x02: ADV, 0x03: Multi
        0x020E83BC,
        // isMapOrUserActionPausedAddr設定 0x00000001: true, 0x00000000 false. Read8 is enough though.
        0x020FBF18,
		// unlockMapsHuntersAddr設定
        0x020E8319
    },
    // GROUP_US1_0行定義
    {
        // baseChosenHunterAddr設定
        0x020CB51C,
        // inGameAddr設定
        0x020EE180u + 0x8F0u,
        // PlayerPosAddr設定
        0x020D9CB8,
        // baseIsAltFormAddr設定
        JP1_0_BASE_IS_ALT_FORM     - OFF_US10_FROM_JP10,
        // baseWeaponChangeAddr設定
        JP1_0_BASE_WEAPON_CHANGE   - OFF_US10_FROM_JP10,
        // baseSelectedWeaponAddr設定
        JP1_0_BASE_SELECTED_WEAPON - OFF_US10_FROM_JP10,
        // baseAimXAddr設定
        0x020DE526,
        // baseAimYAddr設定
        0x020DE52Eu,
        // isInAdventureAddr設定
        0x020E78FC,
        // isMapOrUserActionPausedAddr設定
        0x020FB458,
        // unlockMapsHuntersAddr設定
        0x020E7859
    },
    // GROUP_EU1_1行定義
    {
        // baseChosenHunterAddr設定
        0x020CBE44,
        // inGameAddr設定(元式0x020EECE0+0x8F0の展開値反映のため)
        0x020EECE0u + 0x8F0u,
        // PlayerPosAddr設定
        0x020DA5D8,
        // baseIsAltFormAddr設定
        JP1_0_BASE_IS_ALT_FORM     - OFF_EU11_FROM_JP10,
        // baseWeaponChangeAddr設定
        JP1_0_BASE_WEAPON_CHANGE   - OFF_EU11_FROM_JP10,
        // baseSelectedWeaponAddr設定
        JP1_0_BASE_SELECTED_WEAPON - OFF_EU11_FROM_JP10,
        // baseAimXAddr設定
        0x020DEE46,
        // baseAimYAddr設定
        0x020DEE4Eu,
        // isInAdventureAddr設定
        0x020E845C,
        // isMapOrUserActionPausedAddr設定
        0x020FBFB8,
        // unlockMapsHuntersAddr設定
        0x020E83B9
    },
    // GROUP_EU1_0行定義
    {
        // baseChosenHunterAddr設定
        0x020CBDC4,
        // inGameAddr設定(元式0x020EEC60+0x8F0の展開値反映のため)
        0x020EEC60u + 0x8F0u,
        // PlayerPosAddr設定
        0x020DA558,
        // baseIsAltFormAddr設定
        JP1_0_BASE_IS_ALT_FORM     - OFF_EU10_FROM_JP10,
        // baseWeaponChangeAddr設定
        JP1_0_BASE_WEAPON_CHANGE   - OFF_EU10_FROM_JP10,
        // baseSelectedWeaponAddr設定
        JP1_0_BASE_SELECTED_WEAPON - OFF_EU10_FROM_JP10,
        // baseAimXAddr設定
        0x020DEDC6,
        // baseAimYAddr設定
        0x020DEDCEu,
        // isInAdventureAddr設定
        0x020E83DC,
        // isMapOrUserActionPausedAddr設定
        0x020FBF38,
        // unlockMapsHuntersAddr設定
        0x020E8339
    },
    // GROUP_JP1_0行定義(JP1.0素値の明示反映のため)
    {
        // baseChosenHunterAddr設定
        0x020CD358,
        // inGameAddr設定
        0x020F0BB0,
        // PlayerPosAddr設定
        0x020DBB78,
        // baseIsAltFormAddr設定
        JP1_0_BASE_IS_ALT_FORM,
        // baseWeaponChangeAddr設定
        JP1_0_BASE_WEAPON_CHANGE,
        // baseSelectedWeaponAddr設定
        JP1_0_BASE_SELECTED_WEAPON,
        // baseAimXAddr設定
        0x020E03E6,
        // baseAimYAddr設定
        0x020E03EE,
        // isInAdventureAddr設定
        0x020E9A3C,
        // isMapOrUserActionPausedAddr設定
        0x020FD598,
        // unlockMapsHuntersAddr設定
        0x020E9999
    },
    // GROUP_JP1_1行定義
    {
        // baseChosenHunterAddr設定
        0x020CD318,
        // inGameAddr設定(元式0x020F0280+0x8F0の展開値反映のため)
        0x020F0280u + 0x8F0u,
        // PlayerPosAddr設定
        0x020DBB38,
        // baseIsAltFormAddr設定
        JP1_0_BASE_IS_ALT_FORM     - OFF_JP11_ALT_FROM_JP10,
        // baseWeaponChangeAddr設定
        JP1_0_BASE_WEAPON_CHANGE   - OFF_JP11_WEAPON_FROM_JP10,
        // baseSelectedWeaponAddr設定
        JP1_0_BASE_SELECTED_WEAPON - OFF_JP11_WEAPON_FROM_JP10,
        // baseAimXAddr設定
        0x020E03A6,
        // baseAimYAddr設定
        0x020E03AEu,
        // isInAdventureAddr設定
        0x020E99FC,
        // isMapOrUserActionPausedAddr設定
        0x020FD558,
        // unlockMapsHuntersAddr設定
        0x020E9959
    },
    // GROUP_KR1_0行定義
    {
        // baseChosenHunterAddr設定
        0x020C4B88,
        // inGameAddr設定
        0x020E81B4,
        // PlayerPosAddr設定
        0x020D33A9, // it's weird but "3A9" is correct.
        // baseIsAltFormAddr設定
        JP1_0_BASE_IS_ALT_FORM     - OFF_KR10_FROM_JP10,
        // baseWeaponChangeAddr設定
        JP1_0_BASE_WEAPON_CHANGE   - OFF_KR10_FROM_JP10,
        // baseSelectedWeaponAddr設定
        JP1_0_BASE_SELECTED_WEAPON - OFF_KR10_FROM_JP10,
        // baseAimXAddr設定
        0x020D7C0E,
        // baseAimYAddr設定
        0x020D7C16u,
        // isInAdventureAddr設定
        0x020E11F8,
        // isMapOrUserActionPausedAddr設定
        0x020F4CF8,
        // unlockMapsHuntersAddr設定
        0x020E1155
    }
}};

// テーブルサイズ静的検証(取り違え防止のため)
static_assert(kRomAddrTable.size() == GROUP_COUNT, "ROM address table size mismatch.");

///**
/// ROMアドレス束取得関数.
/// 
/// 
/// @param group ROMグループ識別子.
/// @return ROMアドレス束.
///
static __attribute__((always_inline, flatten)) inline RomAddrs getRomAddrs(RomGroup group) noexcept
{
    // 範囲検証実施(不正グループの即時検出のため)
    assert(static_cast<int>(group) >= 0 && static_cast<int>(group) < static_cast<int>(GROUP_COUNT));
    // 配列直接参照実施(分岐削減と低遅延化のため)
    return kRomAddrTable[static_cast<std::size_t>(group)];
}

///**
/// 既存変数へROMアドレス束を一括設定する関数.
/// 
/// 
/// @param group ROMグループ識別子.
/// @param baseChosenHunterAddr_ 出力: ChosenHunterアドレス.
/// @param inGameAddr_ 出力: inGameアドレス.
/// @param PlayerPosAddr_ 出力: プレイヤ位置アドレス.
/// @param baseIsAltFormAddr_ 出力: AltFormアドレス.
/// @param baseWeaponChangeAddr_ 出力: 武器変更アドレス.
/// @param baseSelectedWeaponAddr_ 出力: 選択武器アドレス.
/// @param baseAimXAddr_ 出力: AimXアドレス.
/// @param baseAimYAddr_ 出力: AimYアドレス.
/// @param isInAdventureAddr_ 出力: ADV/Multi判定アドレス.
/// @param isMapOrUserActionPausedAddr_ 出力: ポーズ判定アドレス.
///
static __attribute__((always_inline, flatten)) inline void detectRomAndSetAddresses_fast(
    RomGroup group,
    uint32_t& baseChosenHunterAddr_,
    uint32_t& inGameAddr_,
    uint32_t& PlayerPosAddr_,
    uint32_t& baseIsAltFormAddr_,
    uint32_t& baseWeaponChangeAddr_,
    uint32_t& baseSelectedWeaponAddr_,
    uint32_t& baseAimXAddr_,
    uint32_t& baseAimYAddr_,
    uint32_t& isInAdventureAddr_,
    uint32_t& isMapOrUserActionPausedAddr_,
    uint32_t& unlockMapsHuntersAddr
) noexcept
{
    // テーブル取得実施(分岐削減のため)
    const RomAddrs a = getRomAddrs(group);
    // ChosenHunterアドレス代入実施(既存変数名整合のため)
    baseChosenHunterAddr_        = a.baseChosenHunterAddr;
    // inGameアドレス代入実施(既存変数名整合のため)
    inGameAddr_                  = a.inGameAddr;
    // プレイヤ位置アドレス代入実施(既存変数名整合のため)
    PlayerPosAddr_               = a.PlayerPosAddr;
    // AltFormアドレス代入実施(既存変数名整合のため)
    baseIsAltFormAddr_           = a.baseIsAltFormAddr;
    // 武器変更アドレス代入実施(既存変数名整合のため)
    baseWeaponChangeAddr_        = a.baseWeaponChangeAddr;
    // 選択武器アドレス代入実施(既存変数名整合のため)
    baseSelectedWeaponAddr_      = a.baseSelectedWeaponAddr;
    // AimXアドレス代入実施(既存変数名整合のため)
    baseAimXAddr_                = a.baseAimXAddr;
    // AimYアドレス代入実施(既存変数名整合のため)
    baseAimYAddr_                = a.baseAimYAddr;
    // ADV/Multi判定アドレス代入実施(既存変数名整合のため)
    isInAdventureAddr_           = a.isInAdventureAddr;
    // ポーズ判定アドレス代入実施(既存変数名整合のため)
    isMapOrUserActionPausedAddr_ = a.isMapOrUserActionPausedAddr;
    unlockMapsHuntersAddr = a.unlockMapsHuntersAddr;
}

#endif // MELON_PRIME_ROM_ADDR_TABLE_H
