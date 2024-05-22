/*
    Copyright 2016-2023 melonDS team

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

#include <stdio.h>
#include <SDL2/SDL.h>
#include <QFileDialog>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "NDS.h"
#include "DSi.h"
#include "DSi_I2C.h"

#include "AudioSettingsDialog.h"
#include "ui_AudioSettingsDialog.h"
#include "main.h"

using namespace melonDS;
AudioSettingsDialog* AudioSettingsDialog::currentDlg = nullptr;

extern std::string EmuDirectory;


AudioSettingsDialog::AudioSettingsDialog(QWidget* parent, bool emuActive, EmuThread* emuThread) : QDialog(parent), ui(new Ui::AudioSettingsDialog), emuThread(emuThread)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    oldInterp = Config::AudioInterp;
    oldBitDepth = Config::AudioBitDepth;
    oldVolume = Config::AudioVolume;
    oldDSiSync = Config::DSiVolumeSync;

    ui->cbInterpolation->addItem("None");
    ui->cbInterpolation->addItem("Linear");
    ui->cbInterpolation->addItem("Cosine");
    ui->cbInterpolation->addItem("Cubic");
    ui->cbInterpolation->addItem("Gaussian (SNES)");
    ui->cbInterpolation->setCurrentIndex(Config::AudioInterp);

    ui->cbBitDepth->addItem("Automatic");
    ui->cbBitDepth->addItem("10-bit");
    ui->cbBitDepth->addItem("16-bit");
    ui->cbBitDepth->setCurrentIndex(Config::AudioBitDepth);

    bool state = ui->slVolume->blockSignals(true);
    ui->slVolume->setValue(Config::AudioVolume);
    ui->slVolume->blockSignals(state);

    ui->chkSyncDSiVolume->setChecked(Config::DSiVolumeSync);

    // Setup volume slider accordingly
    if (emuActive && emuThread->NDS->ConsoleType == 1)
    {
        on_chkSyncDSiVolume_clicked(Config::DSiVolumeSync);
    }
    else
    {
        ui->chkSyncDSiVolume->setEnabled(false);
    }
    bool isext = (Config::MicInputType == 1);
    ui->cbMic->setEnabled(isext);

    const int count = SDL_GetNumAudioDevices(true);
    for (int i = 0; i < count; i++)
    {
        ui->cbMic->addItem(SDL_GetAudioDeviceName(i, true));
    }
    if (Config::MicDevice == "" && count > 0)
    {
        Config::MicDevice = SDL_GetAudioDeviceName(0, true);
    }

    ui->cbMic->setCurrentText(QString::fromStdString(Config::MicDevice));

    grpMicMode = new QButtonGroup(this);
    grpMicMode->addButton(ui->rbMicNone,     micInputType_Silence);
    grpMicMode->addButton(ui->rbMicExternal, micInputType_External);
    grpMicMode->addButton(ui->rbMicNoise,    micInputType_Noise);
    grpMicMode->addButton(ui->rbMicWav,      micInputType_Wav);
    connect(grpMicMode, SIGNAL(buttonClicked(int)), this, SLOT(onChangeMicMode(int)));
    grpMicMode->button(Config::MicInputType)->setChecked(true);

    ui->txtMicWavPath->setText(QString::fromStdString(Config::MicWavPath));

    bool iswav = (Config::MicInputType == micInputType_Wav);
    ui->txtMicWavPath->setEnabled(iswav);
    ui->btnMicWavBrowse->setEnabled(iswav);

    int inst = Platform::InstanceID();
    if (inst > 0)
    {
        ui->lblInstanceNum->setText(QString("Configuring settings for instance %1").arg(inst+1));
        ui->cbInterpolation->setEnabled(false);
        ui->cbBitDepth->setEnabled(false);
        for (QAbstractButton* btn : grpMicMode->buttons())
            btn->setEnabled(false);
        ui->txtMicWavPath->setEnabled(false);
        ui->btnMicWavBrowse->setEnabled(false);
        ui->cbMic->setEnabled(false);
    }
    else
        ui->lblInstanceNum->hide();
}

AudioSettingsDialog::~AudioSettingsDialog()
{
    delete ui;
}

void AudioSettingsDialog::onSyncVolumeLevel()
{
    if (Config::DSiVolumeSync && emuThread->NDS->ConsoleType == 1)
    {
        auto& dsi = static_cast<DSi&>(*emuThread->NDS);
        bool state = ui->slVolume->blockSignals(true);
        ui->slVolume->setValue(dsi.I2C.GetBPTWL()->GetVolumeLevel());
        ui->slVolume->blockSignals(state);
    }
}

void AudioSettingsDialog::onConsoleReset()
{
    on_chkSyncDSiVolume_clicked(Config::DSiVolumeSync);
    ui->chkSyncDSiVolume->setEnabled(emuThread->NDS->ConsoleType == 1);
}

void AudioSettingsDialog::on_AudioSettingsDialog_accepted()
{
    Config::MicDevice = ui->cbMic->currentText().toStdString();
    Config::MicInputType = grpMicMode->checkedId();
    Config::MicWavPath = ui->txtMicWavPath->text().toStdString();
    Config::Save();

    closeDlg();
}

void AudioSettingsDialog::on_AudioSettingsDialog_rejected()
{
    Config::AudioInterp = oldInterp;
    Config::AudioBitDepth = oldBitDepth;
    Config::AudioVolume = oldVolume;
    Config::DSiVolumeSync = oldDSiSync;

    closeDlg();
}

void AudioSettingsDialog::on_cbBitDepth_currentIndexChanged(int idx)
{
    // prevent a spurious change
    if (ui->cbBitDepth->count() < 3) return;

    Config::AudioBitDepth = ui->cbBitDepth->currentIndex();

    emit updateAudioSettings();
}

void AudioSettingsDialog::on_cbInterpolation_currentIndexChanged(int idx)
{
    // prevent a spurious change
    if (ui->cbInterpolation->count() < 5) return;

    Config::AudioInterp = ui->cbInterpolation->currentIndex();

    emit updateAudioSettings();
}

void AudioSettingsDialog::on_slVolume_valueChanged(int val)
{
    if (Config::DSiVolumeSync && emuThread->NDS->ConsoleType == 1)
    {
        auto& dsi = static_cast<DSi&>(*emuThread->NDS);
        dsi.I2C.GetBPTWL()->SetVolumeLevel(val);
        return;
    }

    Config::AudioVolume = val;
}

void AudioSettingsDialog::on_chkSyncDSiVolume_clicked(bool checked)
{
    Config::DSiVolumeSync = checked;

    bool state = ui->slVolume->blockSignals(true);
    if (Config::DSiVolumeSync && emuThread->NDS->ConsoleType == 1)
    {
        auto& dsi = static_cast<DSi&>(*emuThread->NDS);
        ui->slVolume->setMaximum(31);
        ui->slVolume->setValue(dsi.I2C.GetBPTWL()->GetVolumeLevel());
        ui->slVolume->setPageStep(4);
        ui->slVolume->setTickPosition(QSlider::TicksBelow);
    }
    else
    {
        Config::AudioVolume = oldVolume;
        ui->slVolume->setMaximum(256);
        ui->slVolume->setValue(Config::AudioVolume);
        ui->slVolume->setPageStep(16);
        ui->slVolume->setTickPosition(QSlider::NoTicks);
    }
    ui->slVolume->blockSignals(state);
}

void AudioSettingsDialog::onChangeMicMode(int mode)
{
    bool iswav = (mode == 3);
    bool isext = (mode == 1);
    ui->txtMicWavPath->setEnabled(iswav);
    ui->btnMicWavBrowse->setEnabled(iswav);
    ui->cbMic->setEnabled(isext);
}

void AudioSettingsDialog::on_btnMicWavBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select WAV file...",
                                                QString::fromStdString(EmuDirectory),
                                                "WAV files (*.wav);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtMicWavPath->setText(file);
}
