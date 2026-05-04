/*
    Copyright 2016-2025 melonDS team
    (MelonPrime specific configuration extension)
*/

#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTimer>

#include "MelonPrimeInputConfig.h"
#include "ui_MelonPrimeInputConfig.h"
#include "Config.h"
#ifdef MELONPRIME_CUSTOM_HUD
#include "MelonPrimeHudRender.h"
#endif
#ifdef MELONPRIME_DS
#include "MelonPrimePatch.h"
#endif

using namespace melonDS;

namespace {
#ifdef MELONPRIME_ENABLE_DEVELOPER_FEATURES
    constexpr bool kDeveloperOnlyFeaturesEnabled = true;
#else
    constexpr bool kDeveloperOnlyFeaturesEnabled = false;
#endif
}

void MelonPrimeInputConfig::saveConfig()
{
    Config::Table& instcfg = emuInstance->getLocalConfig();
    const bool oldClipCursorToBottomScreenWhenNotInGame =
        instcfg.GetBool("Metroid.Visual.ClipCursorToBottomScreenWhenNotInGame");
    const bool oldInGameTopScreenOnly =
        instcfg.GetBool("Metroid.Visual.InGameTopScreenOnly");
    Config::Table keycfg = instcfg.GetTable("Keyboard");
    Config::Table joycfg = instcfg.GetTable("Joystick");

    for (int i = 0; i < kMetroidHotkeyCount; ++i)
    {
        const char* btn = EmuInstance::hotkeyNames[kMetroidHotkeys[i].id];
        keycfg.SetInt(btn, addonsMetroidKeyMap[i]);
        joycfg.SetInt(btn, addonsMetroidJoyMap[i]);
    }

    for (int i = 0; i < kMetroidHotkey2Count; ++i)
    {
        const char* btn = EmuInstance::hotkeyNames[kMetroidHotkeys2[i].id];
        keycfg.SetInt(btn, addonsMetroid2KeyMap[i]);
        joycfg.SetInt(btn, addonsMetroid2JoyMap[i]);
    }

    // Sensitivities
    instcfg.SetInt("Metroid.Sensitivity.Aim", ui->metroidAimSensitvitySpinBox->value());
    instcfg.SetDouble("Metroid.Sensitivity.Mph", ui->metroidMphSensitvitySpinBox->value());
    instcfg.SetDouble("Metroid.Sensitivity.AimYAxisScale", ui->metroidAimYAxisScaleSpinBox->value());
    instcfg.SetDouble("Metroid.Aim.Adjust", ui->metroidAimAdjustSpinBox->value());

    // Bug fixes
    instcfg.SetBool("Metroid.BugFix.WifiBitset", ui->cbMetroidFixWifiBitset->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.BugFix.FixShadowFreeze", ui->cbMetroidFixShadowFreeze->checkState() == Qt::Checked);
    instcfg.SetBool(
        "Metroid.BugFix.FixNoxusBladePersistence",
        ui->cbMetroidFixNoxusBladePersistence->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.BugFix.UseFirmwareLanguage", ui->cbMetroidUseFirmwareLanguage->checkState() == Qt::Checked);

    // SnapTap
    instcfg.SetBool("Metroid.Operation.SnapTap", ui->cbMetroidEnableSnapTap->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Data.Unlock", ui->cbMetroidUnlockAll->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Apply.Headphone", ui->cbMetroidApplyHeadphone->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Use.Firmware.Name", ui->cbMetroidUseFirmwareName->checkState() == Qt::Checked);

    // Hunter license
    instcfg.SetBool("Metroid.HunterLicense.Hunter.Apply", ui->cbMetroidApplyHunter->checkState() == Qt::Checked);
    instcfg.SetInt("Metroid.HunterLicense.Hunter.Selected", ui->comboMetroidSelectedHunter->currentIndex());

    instcfg.SetBool("Metroid.HunterLicense.Color.Apply", ui->cbMetroidApplyColor->checkState() == Qt::Checked);
    instcfg.SetInt("Metroid.HunterLicense.Color.Selected", ui->comboMetroidSelectedColor->currentIndex());

    // Volume
    instcfg.SetBool("Metroid.Apply.SfxVolume", ui->cbMetroidApplySfxVolume->checkState() == Qt::Checked);
    instcfg.SetInt("Metroid.Volume.SFX", ui->spinMetroidVolumeSFX->value());
    instcfg.SetBool("Metroid.Apply.MusicVolume", ui->cbMetroidApplyMusicVolume->checkState() == Qt::Checked);
    instcfg.SetInt("Metroid.Volume.Music", ui->spinMetroidVolumeMusic->value());

    // Other Metroid Settings 2 Tab
    instcfg.SetBool("Metroid.Apply.joy2KeySupport", ui->cbMetroidApplyJoy2KeySupport->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Enable.stylusMode", ui->cbMetroidEnableStylusMode->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Aim.Disable.MphAimSmoothing", ui->cbMetroidDisableMphAimSmoothing->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Aim.Enable.Accumulator", ui->cbMetroidEnableAimAccumulator->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Aim.Enable.NativeDeltaHook", ui->cbMetroidEnableNativeAimDeltaHook->checkState() == Qt::Checked);
    // Original public behavior:
    // instcfg.SetBool("Metroid.Aim.Enable.InstantAimFollow", ui->cbMetroidEnableInstantAimFollow->checkState() == Qt::Checked);
    instcfg.SetBool(
        "Metroid.Aim.Enable.InstantAimFollow",
        kDeveloperOnlyFeaturesEnabled
            && ui->cbMetroidEnableInstantAimFollow->checkState() == Qt::Checked);

    // Screen Sync Mode
    instcfg.SetInt("Metroid.Screen.SyncMode", ui->comboMetroidScreenSyncMode->currentIndex());

    // In-game scaling
    instcfg.SetBool("Metroid.Visual.InGameAspectRatio", ui->cbMetroidInGameAspectRatio->checkState() == Qt::Checked);
    instcfg.SetInt("Metroid.Visual.InGameAspectRatioMode", ui->comboMetroidInGameAspectRatioMode->currentIndex());
    const bool clipCursorToBottomScreenWhenNotInGame =
        (ui->cbMetroidClipCursorToBottomScreenWhenNotInGame->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Visual.ClipCursorToBottomScreenWhenNotInGame", clipCursorToBottomScreenWhenNotInGame);
    const bool inGameTopScreenOnly =
        (ui->cbMetroidInGameTopScreenOnly->checkState() == Qt::Checked);
    instcfg.SetBool("Metroid.Visual.InGameTopScreenOnly", inGameTopScreenOnly);
    if (oldClipCursorToBottomScreenWhenNotInGame != clipCursorToBottomScreenWhenNotInGame) {
        for (int i = 0; i < emuInstance->getNumWindows(); ++i) {
            MainWindow* win = emuInstance->getWindow(i);
            if (win && win->panel) {
                QTimer::singleShot(0, win->panel, [panel = win->panel]() {
                    panel->updateClipIfNeeded();
                });
            }
        }
    }
    if (oldInGameTopScreenOnly != inGameTopScreenOnly) {
        for (int i = 0; i < emuInstance->getNumWindows(); ++i) {
            MainWindow* win = emuInstance->getWindow(i);
            if (win && win->panel) {
                QMetaObject::invokeMethod(win->panel, "onScreenLayoutChanged", Qt::QueuedConnection);
            }
        }
    }

    // Custom HUD
    instcfg.SetBool("Metroid.Visual.CustomHUD", ui->cbMetroidEnableCustomHud->checkState() == Qt::Checked);

    // Save all programmatic HUD widgets
    for (auto& [key, widget] : m_hudWidgets) {
        if (auto* cb = qobject_cast<QCheckBox*>(widget))
            instcfg.SetBool(key, cb->isChecked());
        else if (auto* sb = qobject_cast<QSpinBox*>(widget))
            instcfg.SetInt(key, sb->value());
        else if (auto* dsb = qobject_cast<QDoubleSpinBox*>(widget))
            instcfg.SetDouble(key, dsb->value());
        else if (auto* le = qobject_cast<QLineEdit*>(widget))
            instcfg.SetString(key, le->text().toStdString());
        else if (auto* combo = qobject_cast<QComboBox*>(widget))
            instcfg.SetInt(key, combo->currentIndex());
    }

    // Section toggle states (existing UI sections)
    instcfg.SetBool("Metroid.UI.SectionInputSettings",  ui->btnToggleInputSettings->isChecked());
    instcfg.SetBool("Metroid.UI.SectionScreenSync",     ui->btnToggleScreenSync->isChecked());
    instcfg.SetBool("Metroid.UI.SectionCursorClipSettings",  ui->btnToggleCursorClipSettings->isChecked());
    instcfg.SetBool("Metroid.UI.SectionInGameApply",  ui->btnToggleInGameApply->isChecked());
    instcfg.SetBool("Metroid.UI.SectionInGameAspectRatio",  ui->btnToggleInGameAspectRatio->isChecked());
    instcfg.SetBool("Metroid.UI.SectionSensitivity",    ui->btnToggleSensitivity->isChecked());
    instcfg.SetBool("Metroid.UI.SectionBugFix",         ui->btnToggleBugFix->isChecked());
    instcfg.SetBool("Metroid.UI.SectionGameFeature",    ui->btnToggleGameFeature->isChecked());
    instcfg.SetBool("Metroid.UI.SectionGameplay",       ui->btnToggleGameplay->isChecked());
    instcfg.SetBool("Metroid.UI.SectionVideo",          ui->btnToggleVideo->isChecked());
    instcfg.SetBool("Metroid.UI.SectionVolume",         ui->btnToggleVolume->isChecked());
    instcfg.SetBool("Metroid.UI.SectionLicense",        ui->btnToggleLicense->isChecked());
    // Restore note: remove this entry if the DEVELOPER ONLY section is removed.
    instcfg.SetBool("Metroid.UI.SectionDeveloperOnly",  ui->btnToggleDeveloperOnly->isChecked());

    // HUD section toggle states (programmatic sections)
    for (auto& [btn, cfgKey] : m_hudToggles)
        instcfg.SetBool(cfgKey, btn->isChecked());

    // P-3: Invalidate cached config so next frame re-reads all values
    MelonPrime::CustomHud_InvalidateConfigCache();
#ifdef MELONPRIME_DS
    MelonPrime::OsdColor_InvalidatePatch();
    MelonPrime::ShadowFreezeRuntimeHook_NotifyConfigChanged();
    MelonPrime::FixNoxusBladePersistence_NotifyConfigChanged();
#endif
}

