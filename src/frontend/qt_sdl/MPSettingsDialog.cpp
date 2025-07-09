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

#include <stdio.h>
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "main.h"

#include "Net_Slirp.h"
#include "Net_PCap.h"
#include "Wifi.h"

#include "MPSettingsDialog.h"
#include "ui_MPSettingsDialog.h"


MPSettingsDialog* MPSettingsDialog::currentDlg = nullptr;


MPSettingsDialog::MPSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::MPSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto& cfg = emuInstance->getGlobalConfig();
    grpAudioMode = new QButtonGroup(this);
    grpAudioMode->addButton(ui->rbAudioAll,        0);
    grpAudioMode->addButton(ui->rbAudioOneOnly,    1);
    grpAudioMode->addButton(ui->rbAudioActiveOnly, 2);
    grpAudioMode->button(cfg.GetInt("MP.AudioMode"))->setChecked(true);

    ui->sbReceiveTimeout->setValue(cfg.GetInt("MP.RecvTimeout"));
}

MPSettingsDialog::~MPSettingsDialog()
{
    delete ui;
}

void MPSettingsDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    if (r == QDialog::Accepted)
    {
        auto& cfg = emuInstance->getGlobalConfig();
        cfg.SetInt("MP.AudioMode", grpAudioMode->checkedId());
        cfg.SetInt("MP.RecvTimeout", ui->sbReceiveTimeout->value());

        Config::Save();
    }

    QDialog::done(r);

    closeDlg();
}
