/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.
    (MelonPrime specific configuration extension)
*/

#ifndef MELONPRIMEINPUTCONFIG_H
#define MELONPRIMEINPUTCONFIG_H

#include <QWidget>
#include <QList>
#include <QTabWidget>
#include <QPushButton>
#include <QVariantMap>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>
#include "EmuInstance.h"
#include "Config.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace Ui { class MelonPrimeInputConfig; }

// A hotkey binding: ID (from EmuInstance) paired with its display label.
struct HotkeyEntry
{
    int         id;
    const char* label;
};

static constexpr HotkeyEntry kMetroidHotkeys[] =
{
    {HK_MetroidMoveForward,         "[Metroid] (W) Move Forward"},
    {HK_MetroidMoveBack,            "[Metroid] (S) Move Back"},
    {HK_MetroidMoveLeft,            "[Metroid] (A) Move Left"},
    {HK_MetroidMoveRight,           "[Metroid] (D) Move Right"},
    {HK_MetroidShootScan,           "[Metroid] (Mouse Left) Shoot/Scan, Map Zoom In"},
    {HK_MetroidScanShoot,           "[Metroid] (V) Scan/Shoot, Map Zoom In"},
    {HK_MetroidZoom,                "[Metroid] (Mouse Right) Imperialist Zoom, Map Zoom Out, Morph Ball Boost"},
    {HK_MetroidJump,                "[Metroid] (Space) Jump"},
    {HK_MetroidMorphBall,           "[Metroid] (L. Ctrl) Transform"},
    {HK_MetroidHoldMorphBallBoost,  "[Metroid] (Shift) Hold to Fast Morph Ball Boost"},
    {HK_MetroidWeaponBeam,          "[Metroid] (Mouse 5, Side Top) Weapon Beam"},
    {HK_MetroidWeaponMissile,       "[Metroid] (Mouse 4, Side Bottom) Weapon Missile"},
    {HK_MetroidWeapon1,             "[Metroid] (1) Weapon 1. ShockCoil"},
    {HK_MetroidWeapon2,             "[Metroid] (2) Weapon 2. Magmaul"},
    {HK_MetroidWeapon3,             "[Metroid] (3) Weapon 3. Judicator"},
    {HK_MetroidWeapon4,             "[Metroid] (4) Weapon 4. Imperialist"},
    {HK_MetroidWeapon5,             "[Metroid] (5) Weapon 5. Battlehammer"},
    {HK_MetroidWeapon6,             "[Metroid] (6) Weapon 6. VoltDriver"},
    {HK_MetroidWeaponSpecial,       "[Metroid] (R) Affinity Weapon (Last used Weapon/Omega cannon)"},
    {HK_MetroidMenu,                "[Metroid] (Tab) Menu/Map"},
};

static constexpr HotkeyEntry kMetroidHotkeys2[] =
{
    {HK_MetroidIngameSensiUp,    "[Metroid] (PgUp) AimSensitivity Up"},
    {HK_MetroidIngameSensiDown,  "[Metroid] (PgDown) AimSensitivity Down"},
    {HK_MetroidWeaponNext,       "[Metroid] (J) Next Weapon in the sorted order"},
    {HK_MetroidWeaponPrevious,   "[Metroid] (K) Previous Weapon in the sorted order"},
    {HK_MetroidScanVisor,        "[Metroid] (C) Scan Visor"},
    {HK_MetroidUILeft,           "[Metroid] (Z) UI Left (Adventure Left Arrow / Hunter License L)"},
    {HK_MetroidUIRight,          "[Metroid] (X) UI Right (Adventure Right Arrow / Hunter License R)"},
    {HK_MetroidUIOk,             "[Metroid] (F) UI Ok"},
    {HK_MetroidUIYes,            "[Metroid] (G) UI Yes (Enter Starship)"},
    {HK_MetroidUINo,             "[Metroid] (H) UI No (Enter Starship)"},
    {HK_MetroidWeaponCheck,      "[Metroid] (Y) Weapon Check"},
};

// Compile-time counts for array sizing.
constexpr int kMetroidHotkeyCount  = static_cast<int>(sizeof(kMetroidHotkeys)  / sizeof(kMetroidHotkeys[0]));
constexpr int kMetroidHotkey2Count = static_cast<int>(sizeof(kMetroidHotkeys2) / sizeof(kMetroidHotkeys2[0]));

class MelonPrimeInputConfig : public QWidget
{
    Q_OBJECT

public:
    explicit MelonPrimeInputConfig(EmuInstance* emu, QWidget* parent = nullptr);
    ~MelonPrimeInputConfig();

    void saveConfig();
    void restoreVisualSnapshot();
    void snapshotVisualConfig();
    void refreshAfterHudEditSave();
    QTabWidget* getTabWidget();

private slots:
    void on_metroidResetSensitivityValues_clicked();
    void on_metroidSetVideoQualityToLow_clicked();
    void on_metroidSetVideoQualityToHigh_clicked();
    void on_metroidSetVideoQualityToHigh2_clicked();

    void on_cbMetroidEnableSnapTap_stateChanged(int state);
    void on_cbMetroidUnlockAll_stateChanged(int state);
    void on_cbMetroidApplyHeadphone_stateChanged(int state);
    void on_cbMetroidUseFirmwareName_stateChanged(int state);
    void on_cbMetroidEnableCustomHud_stateChanged(int state);
    void on_cbMetroidEnableStylusMode_stateChanged(int state);
    void on_cbMetroidDisableMphAimSmoothing_stateChanged(int state);
    void on_btnEditHudLayout_clicked();
    void applyVisualPreview();

private:
    Ui::MelonPrimeInputConfig* ui;
    EmuInstance* emuInstance;

    int addonsMetroidKeyMap[kMetroidHotkeyCount];
    int addonsMetroidJoyMap[kMetroidHotkeyCount];

    int addonsMetroid2KeyMap[kMetroidHotkey2Count];
    int addonsMetroid2JoyMap[kMetroidHotkey2Count];

    // Constructor setup helpers
    void setupKeyBindings(Config::Table& instcfg, Config::Table& keycfg, Config::Table& joycfg);
    void setupSensitivityAndToggles(Config::Table& instcfg);
    void setupCollapsibleSections(Config::Table& instcfg);
    void setupPreviewConnections();
    void setupCustomHudCode();
    void setupCustomHudWidgets(Config::Table& instcfg);
    void updateAimControlsForStylusMode(bool stylusEnabled);

    void populatePage(QWidget* page, const HotkeyEntry* entries, int count, int* keymap, int* joymap);
    QString buildCustomHudCode() const;
    bool applyCustomHudCode(const QString& code, QString* errorMessage = nullptr);
    void refreshCustomHudCodeOutput();
    void setCustomHudCodeStatus(const QString& text, bool isError);
    void invalidateHudAndRefreshPreviews();

    QVariantMap m_visualSnapshot;
    bool m_applyPreviewEnabled = false;
    bool m_applyPreviewActive = false;

    // Programmatic HUD settings widgets (config key → widget)
    std::unordered_map<std::string, QWidget*> m_hudWidgets;
    // Section toggle buttons (button, config key for collapse state)
    std::vector<std::pair<QPushButton*, std::string>> m_hudToggles;
    // Preview widgets that need refresh on config change
    QList<QWidget*> m_hudPreviews;
};

#endif // MELONPRIMEINPUTCONFIG_H

