/*
    Copyright 2016-2021 Arisotura

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

#ifndef INPUTCONFIGDIALOG_H
#define INPUTCONFIGDIALOG_H

#include <QDialog>
#include <QPushButton>

namespace Ui { class InputConfigDialog; }
class InputConfigDialog;

constexpr int hk_general_size = 10;

class InputConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InputConfigDialog(QWidget* parent);
    ~InputConfigDialog();

    static InputConfigDialog* currentDlg;
    static InputConfigDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new InputConfigDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void on_InputConfigDialog_accepted();
    void on_InputConfigDialog_rejected();

    void on_cbxJoystick_currentIndexChanged(int id);

private:
    void populatePage(QWidget* page, int num, const char** labels, int* keymap, int* joymap);

    Ui::InputConfigDialog* ui;

    int keypadKeyMap[12],   keypadJoyMap[12];
    int addonsKeyMap[2],    addonsJoyMap[2];
    int hkGeneralKeyMap[hk_general_size];
    int hkGeneralJoyMap[hk_general_size];
};


class KeyMapButton : public QPushButton
{
    Q_OBJECT

public:
    explicit KeyMapButton(int* mapping, bool hotkey);
    ~KeyMapButton();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

    bool focusNextPrevChild(bool next) override { return false; }

private slots:
    void onClick();

private:
    QString mappingText();

    int* mapping;
    bool isHotkey;
};

class JoyMapButton : public QPushButton
{
    Q_OBJECT

public:
    explicit JoyMapButton(int* mapping, bool hotkey);
    ~JoyMapButton();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

    bool focusNextPrevChild(bool next) override { return false; }

private slots:
    void onClick();

private:
    QString mappingText();

    int* mapping;
    bool isHotkey;

    int timerID;
    int axesRest[16];
};

#endif // INPUTCONFIGDIALOG_H
