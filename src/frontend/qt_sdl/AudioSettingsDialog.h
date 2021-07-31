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

#ifndef AUDIOSETTINGSDIALOG_H
#define AUDIOSETTINGSDIALOG_H

#include <QDialog>
#include <QButtonGroup>

namespace Ui { class AudioSettingsDialog; }
class AudioSettingsDialog;

class AudioSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioSettingsDialog(QWidget* parent);
    ~AudioSettingsDialog();

    static AudioSettingsDialog* currentDlg;
    static AudioSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new AudioSettingsDialog(parent);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void on_AudioSettingsDialog_accepted();
    void on_AudioSettingsDialog_rejected();

    void on_slVolume_valueChanged(int val);
    void onChangeMicMode(int mode);
    void on_btnMicWavBrowse_clicked();

private:
    Ui::AudioSettingsDialog* ui;

    int oldVolume;
    QButtonGroup* grpMicMode;
};

#endif // AUDIOSETTINGSDIALOG_H
