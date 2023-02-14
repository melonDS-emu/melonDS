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

#include <stdio.h>
#include <QFileDialog>

#include "types.h"
#include "Platform.h"
#include "Config.h"

#include "AudioSettingsDialog.h"
#include "ui_AudioSettingsDialog.h"


AudioSettingsDialog* AudioSettingsDialog::currentDlg = nullptr;

extern std::string EmuDirectory;


AudioSettingsDialog::AudioSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::AudioSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    oldInterp = Config::AudioInterp;
    oldBitrate = Config::AudioBitrate;
    oldVolume = Config::AudioVolume;

    ui->cbInterpolation->addItem("None");
    ui->cbInterpolation->addItem("Linear");
    ui->cbInterpolation->addItem("Cosine");
    ui->cbInterpolation->addItem("Cubic");
    ui->cbInterpolation->setCurrentIndex(Config::AudioInterp);

    ui->cbBitrate->addItem("Automatic");
    ui->cbBitrate->addItem("10-bit");
    ui->cbBitrate->addItem("16-bit");
    ui->cbBitrate->setCurrentIndex(Config::AudioBitrate);

    ui->slVolume->setValue(Config::AudioVolume);

    grpMicMode = new QButtonGroup(this);
    grpMicMode->addButton(ui->rbMicNone,     0);
    grpMicMode->addButton(ui->rbMicExternal, 1);
    grpMicMode->addButton(ui->rbMicNoise,    2);
    grpMicMode->addButton(ui->rbMicWav,      3);
    connect(grpMicMode, SIGNAL(buttonClicked(int)), this, SLOT(onChangeMicMode(int)));
    grpMicMode->button(Config::MicInputType)->setChecked(true);

    ui->txtMicWavPath->setText(QString::fromStdString(Config::MicWavPath));

    bool iswav = (Config::MicInputType == 3);
    ui->txtMicWavPath->setEnabled(iswav);
    ui->btnMicWavBrowse->setEnabled(iswav);

    int inst = Platform::InstanceID();
    if (inst > 0)
    {
        ui->lblInstanceNum->setText(QString("Configuring settings for instance %1").arg(inst+1));
        ui->cbInterpolation->setEnabled(false);
        ui->cbBitrate->setEnabled(false);
        for (QAbstractButton* btn : grpMicMode->buttons())
            btn->setEnabled(false);
        ui->txtMicWavPath->setEnabled(false);
        ui->btnMicWavBrowse->setEnabled(false);
    }
    else
        ui->lblInstanceNum->hide();
}

AudioSettingsDialog::~AudioSettingsDialog()
{
    delete ui;
}

void AudioSettingsDialog::on_AudioSettingsDialog_accepted()
{
    Config::MicInputType = grpMicMode->checkedId();
    Config::MicWavPath = ui->txtMicWavPath->text().toStdString();
    Config::Save();

    closeDlg();
}

void AudioSettingsDialog::on_AudioSettingsDialog_rejected()
{
    Config::AudioInterp = oldInterp;
    Config::AudioBitrate = oldBitrate;
    Config::AudioVolume = oldVolume;

    closeDlg();
}

void AudioSettingsDialog::on_cbBitrate_currentIndexChanged(int idx)
{
    // prevent a spurious change
    if (ui->cbBitrate->count() < 3) return;

    Config::AudioBitrate = ui->cbBitrate->currentIndex();

    emit updateAudioSettings();
}

void AudioSettingsDialog::on_cbInterpolation_currentIndexChanged(int idx)
{
    // prevent a spurious change
    if (ui->cbInterpolation->count() < 4) return;

    Config::AudioInterp = ui->cbInterpolation->currentIndex();

    emit updateAudioSettings();
}

void AudioSettingsDialog::on_slVolume_valueChanged(int val)
{
    Config::AudioVolume = val;
}

void AudioSettingsDialog::onChangeMicMode(int mode)
{
    bool iswav = (mode == 3);
    ui->txtMicWavPath->setEnabled(iswav);
    ui->btnMicWavBrowse->setEnabled(iswav);
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
