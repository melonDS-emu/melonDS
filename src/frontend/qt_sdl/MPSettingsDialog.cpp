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
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"

#include "LAN_Socket.h"
#include "LAN_PCap.h"
#include "Wifi.h"

#include "MPSettingsDialog.h"
#include "ui_MPSettingsDialog.h"


MPSettingsDialog* MPSettingsDialog::currentDlg = nullptr;

extern bool RunningSomething;


MPSettingsDialog::MPSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::MPSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    grpAudioMode = new QButtonGroup(this);
    grpAudioMode->addButton(ui->rbAudioAll,        0);
    grpAudioMode->addButton(ui->rbAudioOneOnly,    1);
    grpAudioMode->addButton(ui->rbAudioActiveOnly, 2);
    grpAudioMode->button(Config::MPAudioMode)->setChecked(true);

    ui->sbReceiveTimeout->setValue(Config::MPRecvTimeout);
}

MPSettingsDialog::~MPSettingsDialog()
{
    delete ui;
}

void MPSettingsDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        Config::MPAudioMode = grpAudioMode->checkedId();
        Config::MPRecvTimeout = ui->sbReceiveTimeout->value();

        Config::Save();
    }

    QDialog::done(r);

    closeDlg();
}

//
