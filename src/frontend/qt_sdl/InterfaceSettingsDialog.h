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
*/

#ifndef INTERFACESETTINGSDIALOG_H
#define INTERFACESETTINGSDIALOG_H

#include <QDialog>

namespace Ui { class InterfaceSettingsDialog; }
class InterfaceSettingsDialog;

class EmuInstance;

class InterfaceSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InterfaceSettingsDialog(QWidget* parent);
    ~InterfaceSettingsDialog();

    static InterfaceSettingsDialog* currentDlg;
    static InterfaceSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new InterfaceSettingsDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

signals:
    void updateInterfaceSettings();

private slots:
    void done(int r);

    void on_cbMouseHide_clicked();

    void on_pbClean_clicked();
    void on_pbAccurate_clicked();

    void on_pb2x_clicked();
    void on_pb3x_clicked();
    void on_pbMAX_clicked();

    void on_pbHalf_clicked();
    void on_pbQuarter_clicked();

private:
    Ui::InterfaceSettingsDialog* ui;

    EmuInstance* emuInstance;
};

#endif // INTERFACESETTINGSDIALOG_H
