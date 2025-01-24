/*
    Copyright 2016-2024 melonDS team

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

    @BarretKlics
*/

#include <stdio.h>
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "main.h"

#include "IRSettingsDialog.h"
#include "ui_IRSettingsDialog.h"


IRSettingsDialog* IRSettingsDialog::currentDlg = nullptr;

bool IRSettingsDialog::needsReset = false;

void NetInit();

IRSettingsDialog::IRSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::IRSettingsDialog)
{


    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();
    auto& cfg = emuInstance->getGlobalConfig();




    //ui->groupBoxSerial->setEnabled(false);
    ui->groupBoxUDP->setEnabled(false);

   // connect(ui->rbSerialMode, &QRadioButton::toggled, this, &IRSettingsDialog::toggleSerialSettings);
    connect(ui->rbUDPMode, &QRadioButton::toggled, this, &IRSettingsDialog::toggleUDPSettings);

    

}


void IRSettingsDialog::toggleSerialSettings(bool checked)
{
  //  ui->groupBoxSerial->setEnabled(checked);

}
void IRSettingsDialog::toggleUDPSettings(bool checked){

    ui->groupBoxUDP->setEnabled(checked);
}




IRSettingsDialog::~IRSettingsDialog()
{
    delete ui;
}

void IRSettingsDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    needsReset = false;

    if (r == QDialog::Accepted)
    {
    /*
        auto& cfg = emuInstance->getGlobalConfig();

        cfg.SetBool("LAN.IPMode", ui->rbIPMode->isChecked());

        int sel = ui->cbxIPAdapter->currentIndex();
        if (sel < 0 || sel >= adapters.size()) sel = 0;
        if (adapters.empty())
        {
            //cfg.SetString("LAN.Device", "");
        }
        else
        {
            //cfg.SetString("LAN.Device", adapters[sel].DeviceName);
        }
        */
        Config::Save();
    }

    //Config::Table cfg = Config::GetGlobalTable();
    //std::string devicename = cfg.GetString("LAN.Device");

    //NetInit();

    QDialog::done(r);

    closeDlg();
}



