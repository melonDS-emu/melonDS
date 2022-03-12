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

#include "types.h"
#include "Platform.h"
#include "Config.h"

#include "FramerateSettingsDialog.h"
#include "ui_FramerateSettingsDialog.h"


FramerateSettingsDialog* FramerateSettingsDialog::currentDlg = nullptr;


FramerateSettingsDialog::FramerateSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::FramerateSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->spinFPSRate->setRange(0, 9999);

    ui->cbLimitFPS->setChecked(Config::LimitFPS);
    ui->spinFPSRate->setValue(Config::FPSRate);

    if (Config::LimitFPS)
    {
        ui->spinFPSRate->setEnabled(true);
    }
    else
    {
        ui->spinFPSRate->setEnabled(false);
    }
}

FramerateSettingsDialog::~FramerateSettingsDialog()
{
    delete ui;
}

void FramerateSettingsDialog::on_cbLimitFPS_clicked()
{
    if (ui->spinFPSRate->isEnabled())
    {
        ui->spinFPSRate->setEnabled(false);
    }
    else
    {
        ui->spinFPSRate->setEnabled(true);
    }

    Config::LimitFPS = ui->cbLimitFPS->isChecked() ? 1:0;
    Config::FPSRate = ui->spinFPSRate->value();
}

void FramerateSettingsDialog::on_spinFPSRate_valueChanged(int arg)
{
    Config::FPSRate = ui->spinFPSRate->value();
}

void FramerateSettingsDialog::on_FramerateSettingsDialog_accepted()
{
    closeDlg();
}

void FramerateSettingsDialog::on_FramerateSettingsDialog_rejected()
{
    closeDlg();
}
