/*
    Copyright 2016-2020 Arisotura

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
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "EmuSettingsDialog.h"
#include "ui_EmuSettingsDialog.h"


EmuSettingsDialog* EmuSettingsDialog::currentDlg = nullptr;

extern char* EmuDirectory;


EmuSettingsDialog::EmuSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::EmuSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->txtBIOS9Path->setText(Config::BIOS9Path);
    ui->txtBIOS7Path->setText(Config::BIOS7Path);
    ui->txtFirmwarePath->setText(Config::FirmwarePath);
    ui->chkDirectBoot->setChecked(Config::DirectBoot != 0);
}

EmuSettingsDialog::~EmuSettingsDialog()
{
    delete ui;
}

void EmuSettingsDialog::verifyFirmware()
{
    // verify the firmware
    //
    // there are dumps of an old hacked firmware floating around on the internet
    // and those are problematic
    // the hack predates WFC, and, due to this, any game that alters the WFC
    // access point data will brick that firmware due to it having critical
    // data in the same area. it has the same problem on hardware.
    //
    // but this should help stop users from reporting that issue over and over
    // again, when the issue is not from melonDS but from their firmware dump.
    //
    // I don't know about all the firmware hacks in existence, but the one I
    // looked at has 0x180 bytes from the header repeated at 0x3FC80, but
    // bytes 0x0C-0x14 are different.

    char filename[1024];
    strncpy(filename, ui->txtFirmwarePath->text().toStdString().c_str(), 1023); filename[1023] = '\0';
    FILE* f = Platform::OpenLocalFile(filename, "rb");
    u8 chk1[0x180], chk2[0x180];

    fseek(f, 0, SEEK_SET);
    fread(chk1, 1, 0x180, f);
    fseek(f, -0x380, SEEK_END);
    fread(chk2, 1, 0x180, f);

    memset(&chk1[0x0C], 0, 8);
    memset(&chk2[0x0C], 0, 8);

    fclose(f);

    if (!memcmp(chk1, chk2, 0x180))
    {
        QMessageBox::warning(this,
                      "Problematic firmware dump",
                      "You are using an old hacked firmware dump.\n"
                      "Firmware boot will stop working if you run any game that alters WFC settings.\n\n"
                      "Note that the issue is not from melonDS, it would also happen on an actual DS.");
    }
}

void EmuSettingsDialog::on_EmuSettingsDialog_accepted()
{
    verifyFirmware();

    strncpy(Config::BIOS9Path, ui->txtBIOS9Path->text().toStdString().c_str(), 1023); Config::BIOS9Path[1023] = '\0';
    strncpy(Config::BIOS7Path, ui->txtBIOS7Path->text().toStdString().c_str(), 1023); Config::BIOS7Path[1023] = '\0';
    strncpy(Config::FirmwarePath, ui->txtFirmwarePath->text().toStdString().c_str(), 1023); Config::FirmwarePath[1023] = '\0';
    Config::DirectBoot = ui->chkDirectBoot->isChecked() ? 1:0;

    closeDlg();
}

void EmuSettingsDialog::on_EmuSettingsDialog_rejected()
{
    closeDlg();
}

void EmuSettingsDialog::on_btnBIOS9Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DS-mode ARM9 BIOS...",
                                                EmuDirectory,
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtBIOS9Path->setText(file);
}

void EmuSettingsDialog::on_btnBIOS7Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DS-mode ARM7 BIOS...",
                                                EmuDirectory,
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtBIOS7Path->setText(file);
}

void EmuSettingsDialog::on_btnFirmwareBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DS-mode firmware...",
                                                EmuDirectory,
                                                "Firmware files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtFirmwarePath->setText(file);
}
