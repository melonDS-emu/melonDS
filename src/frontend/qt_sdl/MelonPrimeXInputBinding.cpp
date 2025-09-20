#ifdef _WIN32
#include "MelonPrimeXInputBinding.h"
#include "MelonPrimeXInputFilter.h"
#include "Config.h"
#include "InputConfig/InputConfigDialog.h"

// 省略形
using Axis = MelonPrimeXInputFilter::Axis;
using Config::Table;

// SDLの “ボタン index → XInput ボタン” 推定
static WORD SDLBtnToXBtn(int idx) {
    switch (idx) {
    case 0: return XINPUT_GAMEPAD_A;
    case 1: return XINPUT_GAMEPAD_B;
    case 2: return XINPUT_GAMEPAD_X;
    case 3: return XINPUT_GAMEPAD_Y;
    case 4: return XINPUT_GAMEPAD_LEFT_SHOULDER;
    case 5: return XINPUT_GAMEPAD_RIGHT_SHOULDER;
    case 6: return XINPUT_GAMEPAD_BACK;
    case 7: return XINPUT_GAMEPAD_START;
    case 8: return XINPUT_GAMEPAD_LEFT_THUMB;
    case 9: return XINPUT_GAMEPAD_RIGHT_THUMB;
    default: return 0;
    }
}

static void BindOneHK_FromJoyInt(MelonPrimeXInputFilter* xin, Table joy, const char* key, int hk) {
    if (!xin) return;
    const int v = joy.GetInt(key);
    if (v == -1) { xin->clearBinding(hk); return; }

    // ハット → DPAD
    if (v & 0x100) {
        const int hatdir = (v & 0xF);
        WORD dpad = 0;
        if (hatdir == 0x1) dpad = XINPUT_GAMEPAD_DPAD_UP;
        else if (hatdir == 0x4) dpad = XINPUT_GAMEPAD_DPAD_DOWN;
        else if (hatdir == 0x2) dpad = XINPUT_GAMEPAD_DPAD_RIGHT;
        else if (hatdir == 0x8) dpad = XINPUT_GAMEPAD_DPAD_LEFT;
        if (dpad) xin->bindButton(hk, dpad);
        return;
    }

    // 軸 → しきい値
    if (v & 0x10000) {
        const int axisnum = (v >> 24) & 0xF;
        const int axisdir = (v >> 20) & 0xF; // 0:+,1:-,2:trigger
        if (axisdir == 2) {
            // よくある割当: 5=RT, 2/4=LT
            if (axisnum == 5) xin->bindAxisThreshold(hk, Axis::RT, 0.25f);
            else              xin->bindAxisThreshold(hk, Axis::LT, 0.25f);
            return;
        }
        if (axisnum == 0) xin->bindAxisThreshold(hk, axisdir == 0 ? Axis::LXPos : Axis::LXNeg, 0.5f);
        else if (axisnum == 1) xin->bindAxisThreshold(hk, axisdir == 0 ? Axis::LYPos : Axis::LYNeg, 0.5f);
        else if (axisnum == 2) xin->bindAxisThreshold(hk, axisdir == 0 ? Axis::RXPos : Axis::RXNeg, 0.5f);
        else if (axisnum == 3) xin->bindAxisThreshold(hk, axisdir == 0 ? Axis::RYPos : Axis::RYNeg, 0.5f);
        else                    xin->bindAxisThreshold(hk, Axis::RT, 0.25f); // 不明ならRT扱い
        return;
    }

    // ボタン
    const int btn = (v & 0xFFFF);
    const WORD xb = SDLBtnToXBtn(btn);
    if (xb) xin->bindButton(hk, xb);
}

void MelonPrime_BindMetroidHotkeysFromJoystickConfig(MelonPrimeXInputFilter* xin, int instance) {
    if (!xin) return;
    Table joy = Config::GetLocalTable(instance).GetTable("Joystick");

    // 射撃/ズーム
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidShootScan", HK_MetroidShootScan);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidScanShoot", HK_MetroidScanShoot);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidZoom", HK_MetroidZoom);

    // 移動
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidMoveForward", HK_MetroidMoveForward);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidMoveBack", HK_MetroidMoveBack);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidMoveLeft", HK_MetroidMoveLeft);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidMoveRight", HK_MetroidMoveRight);

    // アクション
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidJump", HK_MetroidJump);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidMorphBall", HK_MetroidMorphBall);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidHoldMorphBallBoost", HK_MetroidHoldMorphBallBoost);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidScanVisor", HK_MetroidScanVisor);

    // UI
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidUILeft", HK_MetroidUILeft);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidUIRight", HK_MetroidUIRight);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidUIOk", HK_MetroidUIOk);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidUIYes", HK_MetroidUIYes);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidUINo", HK_MetroidUINo);

    // 武器
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeaponBeam", HK_MetroidWeaponBeam);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeaponMissile", HK_MetroidWeaponMissile);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeaponSpecial", HK_MetroidWeaponSpecial);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeaponNext", HK_MetroidWeaponNext);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeaponPrevious", HK_MetroidWeaponPrevious);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeapon1", HK_MetroidWeapon1);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeapon2", HK_MetroidWeapon2);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeapon3", HK_MetroidWeapon3);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeapon4", HK_MetroidWeapon4);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeapon5", HK_MetroidWeapon5);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidWeapon6", HK_MetroidWeapon6);

    // メニュー/感度
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidMenu", HK_MetroidMenu);

    // 感度調整（ゲーム内）
    /* これはQT側で処理する
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidIngameSensiUp", HK_MetroidIngameSensiUp);
    BindOneHK_FromJoyInt(xin, joy, "HK_MetroidIngameSensiDown", HK_MetroidIngameSensiDown);
    */
}
#endif
