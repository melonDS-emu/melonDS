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

#ifndef FRAMERATESETTINGSDIALOG_H
#define FRAMERATESETTINGSDIALOG_H

#include <QDialog>

namespace Ui { class FramerateSettingsDialog; }
class FramerateSettingsDialog;

class FramerateSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FramerateSettingsDialog(QWidget* parent);
    ~FramerateSettingsDialog();

    static FramerateSettingsDialog* currentDlg;
    static FramerateSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new FramerateSettingsDialog(parent);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void on_cbLimitFPS_clicked();
    void on_cbAudioSync_clicked();
    void on_spinFPSRate_valueChanged(int arg);
    void on_FramerateSettingsDialog_accepted();
    void on_FramerateSettingsDialog_rejected();

private:
    Ui::FramerateSettingsDialog* ui;
};

#endif // FRAMERATESETTINGSDIALOG_H
