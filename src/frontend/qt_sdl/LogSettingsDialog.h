/*
    Copyright 2016-2020 Arisotura, WaluigiWare64

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

#ifndef LOGSETTINGSDIALOG_H
#define LOGSETTINGSDIALOG_H

#include <QDialog>

namespace Ui { class LogSettingsDialog; }
class LogSettingsDialog;

class LogSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogSettingsDialog(QWidget* parent);
    ~LogSettingsDialog();

    static LogSettingsDialog* currentDlg;
    static LogSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new LogSettingsDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void done(int r);

    void on_rbLogToFile_clicked();
    void on_rbLogToConsole_clicked();
    void on_btnLogFolderBrowse_clicked();

private:
    Ui::LogSettingsDialog* ui;
    
    void updateControls();

};

#endif // LOGSETTINGSDIALOG_H
