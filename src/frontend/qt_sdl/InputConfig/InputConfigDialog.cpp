/*
    Copyright 2016-2026 melonDS team

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

#include <utility>
#include <vector>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QKeyEvent>
#include <QDebug>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <SDL2/SDL.h>

#include "types.h"
#include "Platform.h"

#include "InputConfigDialog.h"
#include "ui_InputConfigDialog.h"
#include "MapButton.h"


using namespace melonDS;
InputConfigDialog* InputConfigDialog::currentDlg = nullptr;

const int dskeyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 2, 3};
const char* dskeylabels[12] = {"A", "B", "X", "Y", "Left", "Right", "Up", "Down", "L", "R", "Select", "Start"};

InputConfigDialog::InputConfigDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InputConfigDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    Config::Table& instcfg = emuInstance->getLocalConfig();
    Config::Table keycfg = instcfg.GetTable("Keyboard");
    Config::Table joycfg = instcfg.GetTable("Joystick");

    for (int i = 0; i < keypad_num; i++)
    {
        const char* btn = EmuInstance::buttonNames[dskeyorder[i]];
        keypadKeyMap[i] = keycfg.GetInt(btn);
        keypadJoyMap[i] = joycfg.GetInt(btn);
    }

    int i = 0;
    for (int hotkey : hk_addons)
    {
        const char* btn = EmuInstance::hotkeyNames[hotkey];
        addonsKeyMap[i] = keycfg.GetInt(btn);
        addonsJoyMap[i] = joycfg.GetInt(btn);
        i++;
    }

    i = 0;
    for (int hotkey : hk_general)
    {
        const char* btn = EmuInstance::hotkeyNames[hotkey];
        hkGeneralKeyMap[i] = keycfg.GetInt(btn);
        hkGeneralJoyMap[i] = joycfg.GetInt(btn);
        i++;
    }

    populatePage(ui->tabAddons, hk_addons_labels, addonsKeyMap, addonsJoyMap);
    populatePage(ui->tabHotkeysGeneral, hk_general_labels, hkGeneralKeyMap, hkGeneralJoyMap);

    joystickID = instcfg.GetInt("JoystickID");

    int njoy = SDL_NumJoysticks();
    if (njoy > 0)
    {
        for (int i = 0; i < njoy; i++)
        {
            const char* name = SDL_JoystickNameForIndex(i);
            ui->cbxJoystick->addItem(QString(name));
        }
        ui->cbxJoystick->setCurrentIndex(joystickID);
    }
    else
    {
        ui->cbxJoystick->addItem("(no joysticks available)");
        ui->cbxJoystick->setEnabled(false);
    }

    setupKeypadPage();
    setupPokeTypePage();

    int inst = emuInstance->getInstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(QString("Configuring mappings for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();
}

InputConfigDialog::~InputConfigDialog()
{
    delete ui;
}

void InputConfigDialog::setupKeypadPage()
{
    for (int i = 0; i < keypad_num; i++)
    {
        QPushButton* pushButtonKey = this->findChild<QPushButton*>(QStringLiteral("btnKey") + dskeylabels[i]);
        QPushButton* pushButtonJoy = this->findChild<QPushButton*>(QStringLiteral("btnJoy") + dskeylabels[i]);

        KeyMapButton* keyMapButtonKey = new KeyMapButton(&keypadKeyMap[i], false);
        JoyMapButton* keyMapButtonJoy = new JoyMapButton(&keypadJoyMap[i], false);

        pushButtonKey->parentWidget()->layout()->replaceWidget(pushButtonKey, keyMapButtonKey);
        pushButtonJoy->parentWidget()->layout()->replaceWidget(pushButtonJoy, keyMapButtonJoy);

        delete pushButtonKey;
        delete pushButtonJoy;

        if (ui->cbxJoystick->isEnabled())
        {
            ui->stackMapping->setCurrentIndex(1);
        }
    }
}

static QString regionDisplayName(melonDS::PokeTypeKeyboard::Region r)
{
    switch (r)
    {
    case melonDS::PokeTypeKeyboard::Region::Europe:  return "Europe (UZPP)";
    case melonDS::PokeTypeKeyboard::Region::France:  return "France (UZPF)";
    case melonDS::PokeTypeKeyboard::Region::Germany: return "Germany (UZPD)";
    case melonDS::PokeTypeKeyboard::Region::Italy:   return "Italy (UZPI)";
    case melonDS::PokeTypeKeyboard::Region::Spain:   return "Spain (UZPS)";
    case melonDS::PokeTypeKeyboard::Region::Japan:   return "Japan (UZPJ)";
    default:                                         return "";
    }
}

void InputConfigDialog::setupPokeTypePage()
{
    Config::Table& instcfg = emuInstance->getLocalConfig();

    // no-op without a supported cart or saved bindings; the tab then shows defaults
    emuInstance->pokeTypeLoadBindings();

    pokeTypeBindings = emuInstance->pokeTypeBindings;
    pokeTypeReleaseKey = pokeTypeBindings.releaseKey;
    // 0 (never set) shows as "None", like -1
    if (pokeTypeReleaseKey == 0) pokeTypeReleaseKey = -1;

    QVBoxLayout* lay = ui->layTypingKeyboard;

    QHBoxLayout* enableRow = new QHBoxLayout();

    chkPokeTypeEnable = new QCheckBox("Enable for Learn with Pokémon: Typing Adventure");
    chkPokeTypeEnable->setChecked(instcfg.GetBool("PokeType.Enabled"));
    // connected after setChecked, so setup doesn't count as an edit
    connect(chkPokeTypeEnable, &QCheckBox::toggled,
            this, &InputConfigDialog::pokeTypeEdited);
    enableRow->addWidget(chkPokeTypeEnable);
    enableRow->addStretch();

    enableRow->addWidget(new QLabel("Release keyboard"));
    KeyMapButton* btnPokeTypeRelease = new KeyMapButton(&pokeTypeReleaseKey, true, true);
    connect(btnPokeTypeRelease, &KeyMapButton::clicked,
            this, &InputConfigDialog::pokeTypeKeyCaptured);
    enableRow->addWidget(btnPokeTypeRelease);
    lay->addLayout(enableRow);

    lblPokeTypeNoRelease = new QLabel(
        "<b>Nothing is bound.</b> With no release key the game keeps the "
        "keyboard and the emulator's hotkeys stay unreachable.");
    lblPokeTypeNoRelease->setVisible(!pokeTypeBindings.releaseKeyBound());
    lay->addWidget(lblPokeTypeNoRelease);

    chkPokeTypeAutoSendFn = new QCheckBox("Automatically send Fn on start");
    chkPokeTypeAutoSendFn->setChecked(instcfg.GetBool("PokeType.AutoSendFn"));
    connect(chkPokeTypeAutoSendFn, &QCheckBox::toggled,
            this, &InputConfigDialog::pokeTypeEdited);
    lay->addWidget(chkPokeTypeAutoSendFn);
    lay->addWidget(new QLabel(
        "The game asks you to turn the keyboard on while holding Fn when it "
        "registers its wireless keyboard. With this on, melonDS holds it for "
        "you and the prompt clears by itself; with it off, the prompt waits "
        "until you press your Fn key."));

    static const char* modeLabels[3] =
    {
        "Follow my system layout",
        "Follow my system layout, with overrides below",
        "Positional -- use my bindings only",
    };

    QHBoxLayout* modeRow = new QHBoxLayout();
    grpPokeTypeMode = new QButtonGroup(this);
    for (int i = 0; i < 3; i++)
    {
        radPokeTypeMode[i] = new QRadioButton(modeLabels[i]);
        grpPokeTypeMode->addButton(radPokeTypeMode[i], i);
        modeRow->addWidget(radPokeTypeMode[i]);
    }
    modeRow->addStretch();
    lay->addLayout(modeRow);
    grpPokeTypeMode->button(pokeTypeBindings.mode)->setChecked(true);

    for (int i = 0; i < 3; i++)
        connect(radPokeTypeMode[i], &QRadioButton::toggled,
                this, &InputConfigDialog::pokeTypeEdited);

    QHBoxLayout* regionRow = new QHBoxLayout();
    regionRow->addWidget(new QLabel("Region"));
    cbxPokeTypeRegion = new QComboBox();
    for (int r = 0; r < PokeTypeBindings::NumRegions; r++)
        cbxPokeTypeRegion->addItem(regionDisplayName((melonDS::PokeTypeKeyboard::Region)r));
    regionRow->addWidget(cbxPokeTypeRegion);
    regionRow->addStretch();
    lay->addLayout(regionRow);

    // never hidden, only emptied, so the rows don't shift when a warning appears
    lblPokeTypeDuplicate = new QLabel();
    lay->addWidget(lblPokeTypeDuplicate);

    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    pokeTypeRowHost = new QWidget();
    QGridLayout* rowGrid = new QGridLayout();
    rowGrid->setVerticalSpacing(2);
    pokeTypeRowHost->setLayout(rowGrid);
    scroll->setWidget(pokeTypeRowHost);
    lay->addWidget(scroll, 1);

    lay->addWidget(new QLabel(
        "Press a key to bind it. Delete clears a binding; Escape cancels."));

    btnPokeTypeReset = new QPushButton();
    lay->addWidget(btnPokeTypeReset);
    connect(btnPokeTypeReset, &QPushButton::clicked,
            this, &InputConfigDialog::pokeTypeResetClicked);

    auto inserted = emuInstance->pokeTypeRegion();
    pokeTypeRegion = (inserted != melonDS::PokeTypeKeyboard::Region::MAX)
                   ? inserted
                   : melonDS::PokeTypeKeyboard::Region::Europe;

    // set before connecting, so setup doesn't flush a key map not yet filled
    cbxPokeTypeRegion->setCurrentIndex((int)pokeTypeRegion);
    connect(cbxPokeTypeRegion, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InputConfigDialog::pokeTypeRegionChanged);

    loadPokeTypeRegion(pokeTypeRegion);
}

void InputConfigDialog::loadPokeTypeRegion(melonDS::PokeTypeKeyboard::Region r)
{
    pokeTypeRegion = r;

    for (melonDS::u16 keyid = 0; keyid < PokeTypeBindings::MaxKeys; keyid++)
    {
        int val = pokeTypeBindings.binding(r, keyid);

        // a key with no default shows as "None" rather than blank
        pokeTypeKeyMap[keyid] = (val == 0) ? -1 : val;
    }

    btnPokeTypeReset->setText(QString("Reset %1 to defaults").arg(regionDisplayName(r)));

    rebuildPokeTypeRows();
    refreshPokeTypeWarnings();
}

void InputConfigDialog::rebuildPokeTypeRows()
{
    QGridLayout* grid = (QGridLayout*)pokeTypeRowHost->layout();

    while (QLayoutItem* item = grid->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    std::vector<std::pair<melonDS::u16, QString>> specialKeys;

    // special keys exist in every region, independent of the layout table
    for (melonDS::u16 keyid = 0; keyid < PokeTypeBindings::MaxKeys; keyid++)
    {
        if (!melonDS::PokeTypeKeyboard::SpecialCharForKeyID(keyid)) continue;
        specialKeys.emplace_back(keyid,
            QString::fromStdString(PokeTypeBindings::label(pokeTypeRegion, keyid)));
    }

    std::vector<std::pair<melonDS::u16, QString>> charKeys;

    melonDS::u32 count = 0;
    const melonDS::PokeTypeKeyboard::KeyDesc* table =
        melonDS::PokeTypeKeyboard::GetKeyTable(pokeTypeRegion, count);

    if (table)
    {
        for (melonDS::u32 i = 0; i < count; i++)
        {
            melonDS::u16 keyid = table[i].KeyID;
            if (melonDS::PokeTypeKeyboard::SpecialCharForKeyID(keyid)) continue;

            charKeys.emplace_back(keyid, QString::fromStdString(
                PokeTypeBindings::label(pokeTypeRegion, keyid)));
        }
    }

    // both groups share the grid's columns, so they use one count to stay aligned
    static constexpr int kCols = 3;

    int row = 0;

    auto addGroup = [&](const std::vector<std::pair<melonDS::u16, QString>>& keys, int cols)
    {
        int n = (int)keys.size();
        if (n == 0) return;

        int rowsPerCol = (n + cols - 1) / cols;

        for (int i = 0; i < n; i++)
        {
            int col = i / rowsPerCol;
            int r = row + (i % rowsPerCol);
            melonDS::u16 keyid = keys[i].first;

            grid->addWidget(new QLabel(keys[i].second), r, col * 2);

            KeyMapButton* btn = new KeyMapButton(&pokeTypeKeyMap[keyid], false, true);
            btn->setMaximumWidth(140);
            connect(btn, &KeyMapButton::clicked,
                    this, &InputConfigDialog::pokeTypeKeyCaptured);
            grid->addWidget(btn, r, col * 2 + 1);
        }

        row += rowsPerCol;
    };

    QLabel* specialHdr = new QLabel("<b>Special keys</b>");
    grid->addWidget(specialHdr, row++, 0, 1, kCols * 2);
    addGroup(specialKeys, kCols);

    if (!charKeys.empty())
    {
        QLabel* charHdr = new QLabel("<b>Character keys</b>");
        grid->addWidget(charHdr, row++, 0, 1, kCols * 2);
        addGroup(charKeys, kCols);
    }

    // QGridLayout never drops rows, so clear any stretch left by a longer region
    for (int i = 0; i < grid->rowCount(); i++)
        grid->setRowStretch(i, 0);
    grid->setRowStretch(row, 1);
}

// uses pokeTypeKeyMap: pokeTypeBindings only sees edits on region switch or accept
void InputConfigDialog::refreshPokeTypeWarnings()
{
    melonDS::u16 keyids[PokeTypeBindings::MaxKeys];
    int nkeys = 0;

    for (melonDS::u16 keyid = 0; keyid < PokeTypeBindings::MaxKeys; keyid++)
        if (melonDS::PokeTypeKeyboard::SpecialCharForKeyID(keyid))
            keyids[nkeys++] = keyid;

    // the layout table lists the special keys too; don't count them twice
    melonDS::u32 count = 0;
    const melonDS::PokeTypeKeyboard::KeyDesc* table =
        melonDS::PokeTypeKeyboard::GetKeyTable(pokeTypeRegion, count);
    if (table)
        for (melonDS::u32 i = 0; i < count; i++)
        {
            melonDS::u16 keyid = table[i].KeyID;
            if (melonDS::PokeTypeKeyboard::SpecialCharForKeyID(keyid)) continue;
            keyids[nkeys++] = keyid;
        }

    bool dup = false;
    for (int a = 0; a < nkeys && !dup; a++)
    {
        int ka = pokeTypeKeyMap[keyids[a]];
        if (ka == 0 || ka == -1) continue;   // sentinels only; bit 31 is a real key
        ka = PokeTypeBindings::normaliseHostKey(ka);

        for (int b = a + 1; b < nkeys; b++)
        {
            int kb = pokeTypeKeyMap[keyids[b]];
            if (kb == 0 || kb == -1) continue;
            if (PokeTypeBindings::normaliseHostKey(kb) == ka) { dup = true; break; }
        }
    }

    lblPokeTypeDuplicate->setText(dup
        ? "<b>Two keys are bound to the same host key.</b> The lower key ID wins."
        : "");

    bool releaseBound = (pokeTypeReleaseKey != 0 && pokeTypeReleaseKey != -1);
    lblPokeTypeNoRelease->setVisible(!releaseBound);
}

// keys left on their default are stored as 0 (never set)
void InputConfigDialog::flushPokeTypeRegion()
{
    for (melonDS::u16 keyid = 0; keyid < PokeTypeBindings::MaxKeys; keyid++)
    {
        int val = pokeTypeKeyMap[keyid];
        int def = PokeTypeBindings::defaultBinding(pokeTypeRegion, keyid);

        // a key that has no default and was left alone is unset, not unbound
        if (val == -1 && def == 0) val = 0;

        pokeTypeBindings.setBinding(pokeTypeRegion, keyid, (val == def) ? 0 : val);
    }
}

// A dirty tab is saved on accept even with no supported cart inserted, so keys
// can be set up before the game is loaded. Switching region is not an edit.
void InputConfigDialog::pokeTypeEdited()
{
    pokeTypeDirty = true;
    refreshPokeTypeWarnings();
}

// clicked() fires both when a capture is armed and when it ends, so arming alone
// marks the tab dirty; at worst that saves the defaults.
void InputConfigDialog::pokeTypeKeyCaptured()
{
    pokeTypeDirty = true;

    KeyMapButton* btn = qobject_cast<KeyMapButton*>(sender());
    if (!btn) { refreshPokeTypeWarnings(); return; }

    if (btn->isChecked())
    {
        pokeTypeCaptureBefore = *btn->mappingPtr();
        return;
    }

    bool rejected = rejectPokeTypeConflict(btn);
    QString conflictText = lblPokeTypeDuplicate->text();

    refreshPokeTypeWarnings();

    // a rejection leaves no duplicate, so the refresh cleared the rejection message
    if (rejected) lblPokeTypeDuplicate->setText(conflictText);
}

// Undoes a capture whose host key is already bound, release key included: a
// game key sharing it would toggle the grab instead of reaching the game.
bool InputConfigDialog::rejectPokeTypeConflict(KeyMapButton* btn)
{
    int* value = btn->mappingPtr();
    int newKey = *value;

    if (newKey == pokeTypeCaptureBefore) return false;   // unchanged, e.g. Escape
    if (newKey == 0 || newKey == -1) return false;       // cleared

    int normNew = PokeTypeBindings::normaliseHostKey(newKey);
    QString otherLabel;

    auto matches = [&](int* other, const QString& label)
    {
        if (!otherLabel.isEmpty()) return;
        if (other == value) return;          // this row itself
        if (*other == 0 || *other == -1) return;
        if (PokeTypeBindings::normaliseHostKey(*other) != normNew) return;
        otherLabel = label;
    };

    matches(&pokeTypeReleaseKey, "the release key");

    for (melonDS::u16 keyid = 0; keyid < PokeTypeBindings::MaxKeys; keyid++)
        if (melonDS::PokeTypeKeyboard::SpecialCharForKeyID(keyid))
            matches(&pokeTypeKeyMap[keyid], QString("the %1 key").arg(
                QString::fromStdString(PokeTypeBindings::label(pokeTypeRegion, keyid))));

    melonDS::u32 count = 0;
    const melonDS::PokeTypeKeyboard::KeyDesc* table =
        melonDS::PokeTypeKeyboard::GetKeyTable(pokeTypeRegion, count);
    if (table)
        for (melonDS::u32 i = 0; i < count; i++)
        {
            melonDS::u16 keyid = table[i].KeyID;
            if (melonDS::PokeTypeKeyboard::SpecialCharForKeyID(keyid)) continue;
            matches(&pokeTypeKeyMap[keyid], QString("the %1 key").arg(
                QString::fromStdString(PokeTypeBindings::label(pokeTypeRegion, keyid))));
        }

    if (otherLabel.isEmpty()) return false;

    *value = pokeTypeCaptureBefore;
    btn->refresh();

    lblPokeTypeDuplicate->setText(QString("<b>%1 is already bound to %2.</b>")
        .arg(KeyMapButton::keyName(newKey), otherLabel));

    return true;
}

void InputConfigDialog::pokeTypeRegionChanged(int id)
{
    flushPokeTypeRegion();
    loadPokeTypeRegion((melonDS::PokeTypeKeyboard::Region)id);
}

void InputConfigDialog::pokeTypeResetClicked()
{
    pokeTypeBindings.resetRegion(pokeTypeRegion);
    loadPokeTypeRegion(pokeTypeRegion);
    pokeTypeDirty = true;
}

void InputConfigDialog::populatePage(QWidget* page,
    const std::initializer_list<const char*>& labels,
    int* keymap, int* joymap)
{
    // kind of a hack
    bool ishotkey = (page != ui->tabInput);

    QHBoxLayout* main_layout = new QHBoxLayout();

    QGroupBox* group;
    QGridLayout* group_layout;

    group = new QGroupBox("Keyboard mappings:");
    main_layout->addWidget(group);
    group_layout = new QGridLayout();
    group_layout->setSpacing(1);
    int i = 0;
    for (const char* labelStr : labels)
    {
        QLabel* label = new QLabel(QString(labelStr)+":");
        KeyMapButton* btn = new KeyMapButton(&keymap[i], ishotkey);

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);
        i++;
    }
    group_layout->setRowStretch(labels.size(), 1);
    group->setLayout(group_layout);
    group->setMinimumWidth(275);

    group = new QGroupBox("Joystick mappings:");
    main_layout->addWidget(group);
    group_layout = new QGridLayout();
    group_layout->setSpacing(1);
    i = 0;
    for (const char* labelStr : labels)
    {
        QLabel* label = new QLabel(QString(labelStr)+":");
        JoyMapButton* btn = new JoyMapButton(&joymap[i], ishotkey);

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);
        i++;
    }
    group_layout->setRowStretch(labels.size(), 1);
    group->setLayout(group_layout);
    group->setMinimumWidth(275);

    page->setLayout(main_layout);
}

void InputConfigDialog::on_InputConfigDialog_accepted()
{
    Config::Table& instcfg = emuInstance->getLocalConfig();
    Config::Table keycfg = instcfg.GetTable("Keyboard");
    Config::Table joycfg = instcfg.GetTable("Joystick");

    for (int i = 0; i < keypad_num; i++)
    {
        const char* btn = EmuInstance::buttonNames[dskeyorder[i]];
        keycfg.SetInt(btn, keypadKeyMap[i]);
        joycfg.SetInt(btn, keypadJoyMap[i]);
    }

    int i = 0;
    for (int hotkey : hk_addons)
    {
        const char* btn = EmuInstance::hotkeyNames[hotkey];
        keycfg.SetInt(btn, addonsKeyMap[i]);
        joycfg.SetInt(btn, addonsJoyMap[i]);
        i++;
    }

    i = 0;
    for (int hotkey : hk_general)
    {
        const char* btn = EmuInstance::hotkeyNames[hotkey];
        keycfg.SetInt(btn, hkGeneralKeyMap[i]);
        joycfg.SetInt(btn, hkGeneralJoyMap[i]);
        i++;
    }

    instcfg.SetInt("JoystickID", joystickID);
    Config::Save();

    emuInstance->inputLoadConfig();

    flushPokeTypeRegion();

    pokeTypeBindings.mode = grpPokeTypeMode->checkedId();
    pokeTypeBindings.releaseKey = pokeTypeReleaseKey;

    emuInstance->pokeTypeBindings = pokeTypeBindings;
    emuInstance->getLocalConfig().SetBool("PokeType.Enabled",
                                          chkPokeTypeEnable->isChecked());
    emuInstance->getLocalConfig().SetBool("PokeType.AutoSendFn",
                                          chkPokeTypeAutoSendFn->isChecked());
    emuInstance->pokeTypeSaveBindings(pokeTypeDirty);
    emuInstance->pokeTypeAutoPairChanged();

    ((MainWindow*)parentWidget())->syncPokeTypeMenuItem();

    closeDlg();
}

void InputConfigDialog::on_InputConfigDialog_rejected()
{
    Config::Table& instcfg = emuInstance->getLocalConfig();
    emuInstance->setJoystick(instcfg.GetInt("JoystickID"));

    closeDlg();
}

void InputConfigDialog::on_btnKeyMapSwitch_clicked()
{
    ui->stackMapping->setCurrentIndex(0);
}

void InputConfigDialog::on_btnJoyMapSwitch_clicked()
{
    ui->stackMapping->setCurrentIndex(1);
}

void InputConfigDialog::on_cbxJoystick_currentIndexChanged(int id)
{
    // prevent a spurious change
    if (ui->cbxJoystick->count() < 2) return;

    joystickID = id;
    emuInstance->setJoystick(id);
}

SDL_Joystick* InputConfigDialog::getJoystick()
{
    return emuInstance->getJoystick();
}

std::shared_ptr<SDL_mutex> InputConfigDialog::getJoyMutex()
{
    return emuInstance->getJoyMutex();
}
