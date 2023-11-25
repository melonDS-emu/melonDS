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

#include "PowerManagementDialog.h"
#include "ui_PowerManagementDialog.h"

#include "SPI.h"
#include "DSi_I2C.h"
#include "NDS.h"
#include "DSi.h"
#include "Config.h"
#include "Platform.h"

#include "types.h"

#include <QtDebug>

using namespace melonDS;

PowerManagementDialog* PowerManagementDialog::currentDlg = nullptr;

PowerManagementDialog::PowerManagementDialog(QWidget* parent) : QDialog(parent), ui(new Ui::PowerManagementDialog)
{
    inited = false;

    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    if (NDS::ConsoleType == 1)
    {
        ui->grpDSBattery->setEnabled(false);

        oldDSiBatteryLevel = DSi::I2C->GetBPTWL()->GetBatteryLevel();
        oldDSiBatteryCharging = DSi::I2C->GetBPTWL()->GetBatteryCharging();
    }
    else
    {
        ui->grpDSiBattery->setEnabled(false);

        oldDSBatteryLevel = NDS::SPI->GetPowerMan()->GetBatteryLevelOkay();
    }

    updateDSBatteryLevelControls();

    ui->cbDSiBatteryCharging->setChecked(DSi::I2C->GetBPTWL()->GetBatteryCharging());
    int dsiBatterySliderPos;
    switch (DSi::I2C->GetBPTWL()->GetBatteryLevel())
    {
        case DSi_BPTWL::batteryLevel_AlmostEmpty:   dsiBatterySliderPos = 0; break;
        case DSi_BPTWL::batteryLevel_Low:           dsiBatterySliderPos = 1; break;
        case DSi_BPTWL::batteryLevel_Half:          dsiBatterySliderPos = 2; break;
        case DSi_BPTWL::batteryLevel_ThreeQuarters: dsiBatterySliderPos = 3; break;
        case DSi_BPTWL::batteryLevel_Full:          dsiBatterySliderPos = 4; break;
    }
    ui->sliderDSiBatteryLevel->setValue(dsiBatterySliderPos);

    int inst = Platform::InstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(QString("Setting battery levels for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();

    inited = true;
}

PowerManagementDialog::~PowerManagementDialog()
{
    delete ui;
}

void PowerManagementDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        if (NDS::ConsoleType == 1)
        {
            Config::DSiBatteryLevel = DSi::I2C->GetBPTWL()->GetBatteryLevel();
            Config::DSiBatteryCharging = DSi::I2C->GetBPTWL()->GetBatteryCharging();
        }
        else
        {
            Config::DSBatteryLevelOkay = NDS::SPI->GetPowerMan()->GetBatteryLevelOkay();
        }
    }
    else
    {
        if (NDS::ConsoleType == 1)
        {
            DSi::I2C->GetBPTWL()->SetBatteryLevel(oldDSiBatteryLevel);
            DSi::I2C->GetBPTWL()->SetBatteryCharging(oldDSiBatteryCharging);
        }
        else
        {
            NDS::SPI->GetPowerMan()->SetBatteryLevelOkay(oldDSBatteryLevel);
        }
    }

    QDialog::done(r);

    closeDlg();
}

void PowerManagementDialog::on_rbDSBatteryLow_clicked()
{
    NDS::SPI->GetPowerMan()->SetBatteryLevelOkay(false);
}

void PowerManagementDialog::on_rbDSBatteryOkay_clicked()
{
    NDS::SPI->GetPowerMan()->SetBatteryLevelOkay(true);
}

void PowerManagementDialog::updateDSBatteryLevelControls()
{
    if (NDS::SPI->GetPowerMan()->GetBatteryLevelOkay())
        ui->rbDSBatteryOkay->setChecked(true);
    else
        ui->rbDSBatteryLow->setChecked(true);
}

void PowerManagementDialog::on_cbDSiBatteryCharging_toggled()
{
    DSi::I2C->GetBPTWL()->SetBatteryCharging(ui->cbDSiBatteryCharging->isChecked());
}

void PowerManagementDialog::on_sliderDSiBatteryLevel_valueChanged(int value)
{
    if (!inited) return;

    u8 newBatteryLevel;
    switch (value)
    {
        case 0: newBatteryLevel = DSi::I2C->GetBPTWL()->batteryLevel_AlmostEmpty; break;
        case 1: newBatteryLevel = DSi::I2C->GetBPTWL()->batteryLevel_Low; break;
        case 2: newBatteryLevel = DSi::I2C->GetBPTWL()->batteryLevel_Half; break;
        case 3: newBatteryLevel = DSi::I2C->GetBPTWL()->batteryLevel_ThreeQuarters; break;
        case 4: newBatteryLevel = DSi::I2C->GetBPTWL()->batteryLevel_Full; break;
    }
    DSi::I2C->GetBPTWL()->SetBatteryLevel(newBatteryLevel);
    updateDSBatteryLevelControls();
}

