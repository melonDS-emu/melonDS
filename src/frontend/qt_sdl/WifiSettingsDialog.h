/*
    Copyright 2016-2025 melonDS team

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

#ifndef WIFISETTINGSDIALOG_H
#define WIFISETTINGSDIALOG_H

#include <QDialog>
#include <vector>
#include "Net_PCap.h"

namespace Ui { class WifiSettingsDialog; }
class WifiSettingsDialog;

class EmuInstance;

class WifiSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WifiSettingsDialog(QWidget* parent);
    ~WifiSettingsDialog();

    static WifiSettingsDialog* currentDlg;
    static WifiSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new WifiSettingsDialog(parent);
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

    void on_rbDirectMode_clicked();
    void on_rbIndirectMode_clicked();
    void on_cbxDirectAdapter_currentIndexChanged(int sel);

private:
    Ui::WifiSettingsDialog* ui;
    EmuInstance* emuInstance;

    bool haspcap;

    void updateAdapterControls();
    std::vector<melonDS::AdapterData> adapters;
};

#endif // WIFISETTINGSDIALOG_H
