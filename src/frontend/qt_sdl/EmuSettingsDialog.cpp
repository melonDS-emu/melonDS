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

#include <QFileDialog>

#include "types.h"
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

void EmuSettingsDialog::on_EmuSettingsDialog_accepted()
{
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

    // TODO: check for shitty hacked firmware here?

    ui->txtFirmwarePath->setText(file);
}
