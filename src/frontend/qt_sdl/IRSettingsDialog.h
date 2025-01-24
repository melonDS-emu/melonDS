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


    @BarretKlics
*/

#ifndef IRSETTINGSDIALOG_H
#define IRSETTINGSDIALOG_H

#include <QDialog>
#include <vector>

namespace Ui { class IRSettingsDialog; }
class IRSettingsDialog;

class EmuInstance;

class IRSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IRSettingsDialog(QWidget* parent);
    ~IRSettingsDialog();

    static IRSettingsDialog* currentDlg;
    static IRSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new IRSettingsDialog(parent);
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
    void toggleSerialSettings(bool checked);
    void toggleUDPSettings(bool checked);

private:
    Ui::IRSettingsDialog* ui;
    EmuInstance* emuInstance;

   // void updateAdapterControls();
   // std::vector<melonDS::AdapterData> adapters;
};

#endif // IRSETTINGSDIALOG_H
