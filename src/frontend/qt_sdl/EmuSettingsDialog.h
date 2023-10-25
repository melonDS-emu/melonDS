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

#ifndef EMUSETTINGSDIALOG_H
#define EMUSETTINGSDIALOG_H

#include <QDialog>

namespace Ui { class EmuSettingsDialog; }
class EmuSettingsDialog;

class EmuSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EmuSettingsDialog(QWidget* parent);
    ~EmuSettingsDialog();

    static EmuSettingsDialog* currentDlg;
    static EmuSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new EmuSettingsDialog(parent);
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

    void on_btnBIOS9Browse_clicked();
    void on_btnBIOS7Browse_clicked();
    void on_btnFirmwareBrowse_clicked();

    void on_cbDLDIEnable_toggled();
    void on_btnDLDISDBrowse_clicked();
    void on_cbDLDIFolder_toggled();
    void on_btnDLDIFolderBrowse_clicked();

    void on_btnDSiBIOS9Browse_clicked();
    void on_btnDSiBIOS7Browse_clicked();
    void on_btnDSiFirmwareBrowse_clicked();
    void on_btnDSiNANDBrowse_clicked();

    void on_cbDSiSDEnable_toggled();
    void on_btnDSiSDBrowse_clicked();
    void on_cbDSiSDFolder_toggled();
    void on_btnDSiSDFolderBrowse_clicked();

    void on_chkEnableJIT_toggled();
    void on_chkExternalBIOS_toggled();

    void on_cbGdbEnabled_toggled();

private:
    void verifyFirmware();

    Ui::EmuSettingsDialog* ui;
};

#endif // EMUSETTINGSDIALOG_H
