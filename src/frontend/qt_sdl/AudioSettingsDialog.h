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
    explicit AudioSettingsDialog(QWidget* parent, bool emuActive);
    ~AudioSettingsDialog();

    static AudioSettingsDialog* currentDlg;
    static AudioSettingsDialog* openDlg(QWidget* parent, bool emuActive)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new AudioSettingsDialog(parent, emuActive);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

    void onSyncVolumeLevel();
    void onConsoleReset();

signals:
    void updateAudioSettings();

private slots:
    void on_AudioSettingsDialog_accepted();
    void on_AudioSettingsDialog_rejected();

    void on_cbInterpolation_currentIndexChanged(int idx);
    void on_cbBitDepth_currentIndexChanged(int idx);
    void on_slVolume_valueChanged(int val);
    void on_chkSyncDSiVolume_clicked(bool checked);
    void onChangeMicMode(int mode);
    void on_btnMicWavBrowse_clicked();

private:
    Ui::AudioSettingsDialog* ui;

    int oldInterp;
    int oldBitDepth;
    int oldVolume;
    bool oldDSiSync;
    QButtonGroup* grpMicMode;
};

#endif // AUDIOSETTINGSDIALOG_H
