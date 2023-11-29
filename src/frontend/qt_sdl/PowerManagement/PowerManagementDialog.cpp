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
#include "main.h"

using namespace melonDS;

PowerManagementDialog* PowerManagementDialog::currentDlg = nullptr;

PowerManagementDialog::PowerManagementDialog(QWidget* parent, EmuThread* emuThread) : QDialog(parent), emuThread(emuThread), ui(new Ui::PowerManagementDialog)
{
    inited = false;

    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    if (emuThread->NDS->ConsoleType == 1)
    {
        ui->grpDSBattery->setEnabled(false);

        auto& dsi = static_cast<DSi&>(*emuThread->NDS);
        oldDSiBatteryLevel = dsi.I2C.GetBPTWL()->GetBatteryLevel();
        oldDSiBatteryCharging = dsi.I2C.GetBPTWL()->GetBatteryCharging();
    }
    else
    {
        ui->grpDSiBattery->setEnabled(false);

        oldDSBatteryLevel = emuThread->NDS->SPI.GetPowerMan()->GetBatteryLevelOkay();
    }

    updateDSBatteryLevelControls();

    bool defaultDSiBatteryCharging = (emuThread->NDS->ConsoleType == 1) ? Config::DSiBatteryCharging : false;

    if (emuThread->NDS->ConsoleType == 1)
    {
        auto& dsi = static_cast<DSi&>(*emuThread->NDS);
        ui->cbDSiBatteryCharging->setChecked(dsi.I2C.GetBPTWL()->GetBatteryCharging());
        int dsiBatterySliderPos = 4;
        switch (dsi.I2C.GetBPTWL()->GetBatteryLevel())
        {
        case DSi_BPTWL::batteryLevel_AlmostEmpty:   dsiBatterySliderPos = 0; break;
        case DSi_BPTWL::batteryLevel_Low:           dsiBatterySliderPos = 1; break;
        case DSi_BPTWL::batteryLevel_Half:          dsiBatterySliderPos = 2; break;
        case DSi_BPTWL::batteryLevel_ThreeQuarters: dsiBatterySliderPos = 3; break;
        case DSi_BPTWL::batteryLevel_Full:          dsiBatterySliderPos = 4; break;
        }
        ui->sliderDSiBatteryLevel->setValue(dsiBatterySliderPos);
    }
    else
    {
        ui->cbDSiBatteryCharging->setChecked(Config::DSiBatteryCharging);
        ui->sliderDSiBatteryLevel->setValue(Config::DSiBatteryLevel);
    }


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
        if (emuThread->NDS->ConsoleType == 1)
        {
            auto& dsi = static_cast<DSi&>(*emuThread->NDS);
            Config::DSiBatteryLevel = dsi.I2C.GetBPTWL()->GetBatteryLevel();
            Config::DSiBatteryCharging = dsi.I2C.GetBPTWL()->GetBatteryCharging();
        }
        else
        {
            Config::DSBatteryLevelOkay = emuThread->NDS->SPI.GetPowerMan()->GetBatteryLevelOkay();
        }
    }
    else
    {
        if (emuThread->NDS->ConsoleType == 1)
        {
            auto& dsi = static_cast<DSi&>(*emuThread->NDS);
            dsi.I2C.GetBPTWL()->SetBatteryLevel(oldDSiBatteryLevel);
            dsi.I2C.GetBPTWL()->SetBatteryCharging(oldDSiBatteryCharging);
        }
        else
        {
            emuThread->NDS->SPI.GetPowerMan()->SetBatteryLevelOkay(oldDSBatteryLevel);
        }
    }

    QDialog::done(r);

    closeDlg();
}

void PowerManagementDialog::on_rbDSBatteryLow_clicked()
{
    emuThread->NDS->SPI.GetPowerMan()->SetBatteryLevelOkay(false);
}

void PowerManagementDialog::on_rbDSBatteryOkay_clicked()
{
    emuThread->NDS->SPI.GetPowerMan()->SetBatteryLevelOkay(true);
}

void PowerManagementDialog::updateDSBatteryLevelControls()
{
    if (emuThread->NDS->SPI.GetPowerMan()->GetBatteryLevelOkay())
        ui->rbDSBatteryOkay->setChecked(true);
    else
        ui->rbDSBatteryLow->setChecked(true);
}

void PowerManagementDialog::on_cbDSiBatteryCharging_toggled()
{
    if (emuThread->NDS->ConsoleType == 1)
    {
        auto& dsi = static_cast<DSi&>(*emuThread->NDS);
        dsi.I2C.GetBPTWL()->SetBatteryCharging(ui->cbDSiBatteryCharging->isChecked());
    }
}

void PowerManagementDialog::on_sliderDSiBatteryLevel_valueChanged(int value)
{
    if (!inited) return;

    if (emuThread->NDS->ConsoleType == 1)
    {
        auto& dsi = static_cast<DSi&>(*emuThread->NDS);
        u8 newBatteryLevel = DSi_BPTWL::batteryLevel_Full;
        switch (value)
        {
        case 0: newBatteryLevel = DSi_BPTWL::batteryLevel_AlmostEmpty; break;
        case 1: newBatteryLevel = DSi_BPTWL::batteryLevel_Low; break;
        case 2: newBatteryLevel = DSi_BPTWL::batteryLevel_Half; break;
        case 3: newBatteryLevel = DSi_BPTWL::batteryLevel_ThreeQuarters; break;
        case 4: newBatteryLevel = DSi_BPTWL::batteryLevel_Full; break;
        }
        dsi.I2C.GetBPTWL()->SetBatteryLevel(newBatteryLevel);
    }

    updateDSBatteryLevelControls();
}

