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

#ifndef FIRMWARESETTINGSDIALOG_H
#define FIRMWARESETTINGSDIALOG_H

#include <QDialog>
#include <QWidget>

namespace Ui { class FirmwareSettingsDialog; }
class FirmwareSettingsDialog;

class FirmwareSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    const QStringList colours
    {
        "Greyish Blue",
        "Brown",
        "Red",
        "Light Pink",
        "Orange",
        "Yellow",
        "Lime",
        "Light Green",
        "Dark Green",
        "Turqoise",
        "Light Blue",
        "Blue",
        "Dark Blue",
        "Dark Purple",
        "Light Purple",
        "Dark Pink"
    };

    const QStringList languages
    {
        "Japanese",
        "English",
        "French",
        "German",
        "Italian",
        "Spanish"
    };

    explicit FirmwareSettingsDialog(QWidget* parent);
    ~FirmwareSettingsDialog();

    static FirmwareSettingsDialog* currentDlg;
    static FirmwareSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new FirmwareSettingsDialog(parent);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void on_dialogButtons_accepted();
    void on_dialogButtons_rejected();

private:
    Ui::FirmwareSettingsDialog* ui;
};

#endif // FIRMWARESETTINGSDIALOG_H
