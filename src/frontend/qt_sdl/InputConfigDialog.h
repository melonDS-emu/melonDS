/*
    Copyright 2016-2020 Arisotura

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

    //

private:
    void populatePage(QWidget* page, int num, const char** labels, int* keymap, int* joymap);

    QString joyMappingName(int id);

    Ui::InputConfigDialog* ui;

    int keypadKeyMap[12],   keypadJoyMap[12];
    int addonsKeyMap[2],    addonsJoyMap[2];
    int hkGeneralKeyMap[6], hkGeneralJoyMap[6];
};


class KeyMapButton : public QPushButton
{
    Q_OBJECT

public:
    explicit KeyMapButton(QWidget* parent, int* mapping, bool hotkey);
    ~KeyMapButton();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private slots:
    void onClick();

private:
    QString mappingText();

    int* mapping;
    bool isHotkey;
};

#endif // INPUTCONFIGDIALOG_H
