/*
    Copyright 2016-2022 melonDS team

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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include <QWidget>

namespace Ui { class Settings; }
class Settings;

class Settings : public QDialog
{
    Q_OBJECT

public:
    const QColor colors[16] =
    {
        QColor(97, 130, 154),  // Greyish blue
        QColor(186, 73, 0),    // Brown
        QColor(251, 0, 24),    // Red
        QColor(251, 138, 251), // Light pink,
        QColor(251, 146, 0),   // Orange
        QColor(243, 227, 0),   // Yellow
        QColor(170, 251, 0),   // Lime
        QColor(0, 251, 0),     // Light green
        QColor(0, 162, 56),    // Dark green
        QColor(73, 219, 138),  // Turquoise
        QColor(48, 186, 243),  // Light blue
        QColor(0, 89, 243),    // Blue
        QColor(0, 0, 146),     // Dark blue
        QColor(138, 0, 211),   // Dark purple
        QColor(211, 0, 235),   // Light purple
        QColor(251, 0, 246)    // Dark pink
    };

    explicit Settings(QWidget* parent);
    ~Settings();

    static Settings* currentDlg;

    static Settings* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new Settings(parent);
        currentDlg->open();
        return currentDlg;
    }

    static void closeDlg()
    {
        currentDlg = nullptr;
    }

    static bool needsReset;

private slots:
    void done(int r);

    void onDSBIOS9Browse();
    void onDSBIOS7Browse();
    void onDSFirmwareBrowse();

    void onDSiBIOS9Browse();
    void onDSiBIOS7Browse();
    void onDSiFirmwareBrowse();
    void onDSiNANDBrowse();

    void birthdayMonthDayCorrection(int index);

private:
    bool verifyMAC();

    Ui::Settings* ui;
};

#endif // Settings_H
