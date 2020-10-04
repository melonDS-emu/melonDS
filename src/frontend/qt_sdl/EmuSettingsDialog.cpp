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
#include "SPI.h"

#include "EmuSettingsDialog.h"
#include "ui_EmuSettingsDialog.h"


EmuSettingsDialog* EmuSettingsDialog::currentDlg = nullptr;

extern char* EmuDirectory;
extern bool RunningSomething;

bool EmuSettingsDialog::needsReset = false;

EmuSettingsDialog::EmuSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::EmuSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->txtBIOS9Path->setText(Config::BIOS9Path);
    ui->txtBIOS7Path->setText(Config::BIOS7Path);
    ui->txtFirmwarePath->setText(Config::FirmwarePath);
    ui->cbDLDIEnable->setChecked(Config::DLDIEnable != 0);
    ui->txtDLDISDPath->setText(Config::DLDISDPath);

    ui->txtDSiBIOS9Path->setText(Config::DSiBIOS9Path);
    ui->txtDSiBIOS7Path->setText(Config::DSiBIOS7Path);
    ui->txtDSiFirmwarePath->setText(Config::DSiFirmwarePath);
    ui->txtDSiNANDPath->setText(Config::DSiNANDPath);
    ui->cbDSiSDEnable->setChecked(Config::DSiSDEnable != 0);
    ui->txtDSiSDPath->setText(Config::DSiSDPath);

    ui->cbxConsoleType->addItem("DS");
    ui->cbxConsoleType->addItem("DSi (experimental)");
    ui->cbxConsoleType->setCurrentIndex(Config::ConsoleType);

    ui->chkDirectBoot->setChecked(Config::DirectBoot != 0);

#ifdef JIT_ENABLED
    ui->chkEnableJIT->setChecked(Config::JIT_Enable != 0);
    ui->chkJITBranchOptimisations->setChecked(Config::JIT_BranchOptimisations != 0);
    ui->chkJITLiteralOptimisations->setChecked(Config::JIT_LiteralOptimisations != 0);
    ui->chkJITFastMemory->setChecked(Config::JIT_FastMemory != 0);
    ui->spnJITMaximumBlockSize->setValue(Config::JIT_MaxBlockSize);
#else
    ui->chkEnableJIT->setDisabled(true);
    ui->chkJITBranchOptimisations->setDisabled(true);
    ui->chkJITLiteralOptimisations->setDisabled(true);
    ui->chkJITFastMemory->setDisabled(true);
    ui->spnJITMaximumBlockSize->setDisabled(true);
#endif

    on_chkEnableJIT_toggled();
    readFirmware();
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
    if (!f) return;
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
        QMessageBox::warning((QWidget*)this->parent(),
                      "Problematic firmware dump",
                      "You are using an old hacked firmware dump.\n"
                      "Firmware boot will stop working if you run any game that alters WFC settings.\n\n"
                      "Note that the issue is not from melonDS, it would also happen on an actual DS.");
    }
}

void EmuSettingsDialog::readFirmware()
{
    u8 Color;
    u8 Birthday[2];
    ushort UserName[20];
    ushort Message[52];
    u8 Language;

    char filename[1024];
    strncpy(filename, ui->txtFirmwarePath->text().toStdString().c_str(), 1023); filename[1023] = '\0';
    FILE* f = Platform::OpenLocalFile(filename, "rb");
    if (!f) return;
    
    fseek(f, 0x3FF02, SEEK_SET);
    fread(&Color, 1, 1, f);
    fseek(f, 0x3FF03, SEEK_SET);
    fread(Birthday, 2, 1, f);
    fseek(f, 0x3FF06, SEEK_SET);
    fread(UserName, 20, 1, f);
    fseek(f, 0x3FF1C, SEEK_SET);
    fread(Message, 52, 1, f);
    fseek(f, 0x3FF64, SEEK_SET);
    fread(&Language, 1, 1, f);
    
    fclose(f);
    
    ui->cbxColor->setCurrentIndex(Color);
    ui->deBirthday->setDate(QDate(2004, Birthday[0], Birthday[1]));
    ui->txtUserName->setText(QString::fromUtf16(UserName));
    ui->txtMessage->setText(QString::fromUtf16(Message));
    ui->cbxLanguage->setCurrentIndex(Language);
}

void EmuSettingsDialog::writeFirmware()
{    
    char filename[1024];
    strncpy(filename, ui->txtFirmwarePath->text().toStdString().c_str(), 1023); filename[1023] = '\0';
    FILE* input = Platform::OpenLocalFile(filename, "rb");
    if (!input) return;
    
    fseek(input, 0, SEEK_END);
    u32 size = ftell(input);
    fseek(input, 0, SEEK_SET);
    u8 Firmware[size];
    fread(Firmware, size, 1, input);
    
    fclose(input);

    u8 UserSettings [256];                                                                          // based on GBATEK
    *(u16*)&UserSettings[0x00] = 5;                                                                 // 0x00 - 0x01  version
    UserSettings[0x02] = ui->cbxColor->currentIndex();                                              // 0x02         color
    UserSettings[0x03] = (u8)ui->deBirthday->date().month();                                        // 0x03         birthmonth
    UserSettings[0x04] = (u8)ui->deBirthday->date().day();                                          // 0x04         birthday
                                                                                                    // 0x05         not used
    memcpy(&UserSettings[0x06], ui->txtUserName->text().leftJustified(10, 0x00, true).utf16(), 20); // 0x06 - 0x19  username
    *(u16*)&UserSettings[0x1A] = ui->txtUserName->text().length();                                  // 0x1A - 0x1B  username length
    memcpy(&UserSettings[0x1C], ui->txtMessage->text().leftJustified(26, 0x00, true).utf16(), 52);  // 0x1C - 0x49  message
    *(u16*)&UserSettings[0x50] = ui->txtMessage->text().length();                                   // 0x50 - 0x51  message length
                                                                                                    // 0x52         alarm hour
                                                                                                    // 0x53         alarm minute
                                                                                                    // 0x54 - 0x55  not used?
                                                                                                    // 0x56         alarm enabled
                                                                                                    // 0x57         not used?
    *(u16*)&UserSettings[0x58] = 0;                                                                 // 0x58 - 0x63  touchscreen calibration
    *(u16*)&UserSettings[0x5A] = 0;                                                                 //
    UserSettings[0x5C] = 0;                                                                         //
    UserSettings[0x5D] = 0;                                                                         //
    *(u16*)&UserSettings[0x5E] = 255<<4;                                                            //
    *(u16*)&UserSettings[0x60] = 191<<4;                                                            //
    UserSettings[0x62] = 255;                                                                       //
    UserSettings[0x63] = 191;                                                                       //
    UserSettings[0x64] = ui->cbxLanguage->currentIndex();                                           // 0x64         language
    memcpy(&UserSettings[0x65], &Firmware[0x3FF65], 7);                                             /* 0x65         flags
                                                                                                       0x66         year
                                                                                                       0x67         unknown
                                                                                                       0x68 - 0x6B  RTC offset */
                                                                                                    // 0x6C - 0x6F  not used (0x00 filled)
                                                                                                    // 0x70 - 0x71  update counter
    *(u16*)&UserSettings[0x72] = SPI_Firmware::CRC16(&UserSettings[0], 0x70, 0xFFFF);               // 0x72 - 0x73  CRC32 checksum of 0x00 - 0x6F
    memset(&UserSettings[0x74], 0xFF, 140);                                                         // 0x74 - 0xFF  not used (0xFF filled)

    FILE* output = Platform::OpenLocalFile(filename, "wb");
    if (!output) return;
    
    fwrite(&Firmware[0], 0x3FE00, 1, output);
    fwrite(&UserSettings, 256, 1, output);
    fwrite(&UserSettings, 256, 1, output);
    
    fclose(output);
}

void EmuSettingsDialog::done(int r)
{
    needsReset = false;

    if (r == QDialog::Accepted)
    {
        verifyFirmware();
        writeFirmware();

        int consoleType = ui->cbxConsoleType->currentIndex();
        int directBoot = ui->chkDirectBoot->isChecked() ? 1:0;

        int jitEnable = ui->chkEnableJIT->isChecked() ? 1:0;
        int jitMaxBlockSize = ui->spnJITMaximumBlockSize->value();
        int jitBranchOptimisations = ui->chkJITBranchOptimisations->isChecked() ? 1:0;
        int jitLiteralOptimisations = ui->chkJITLiteralOptimisations->isChecked() ? 1:0;
        int jitFastMemory = ui->chkJITFastMemory->isChecked() ? 1:0;

        std::string bios9Path = ui->txtBIOS9Path->text().toStdString();
        std::string bios7Path = ui->txtBIOS7Path->text().toStdString();
        std::string firmwarePath = ui->txtFirmwarePath->text().toStdString();
        int dldiEnable = ui->cbDLDIEnable->isChecked() ? 1:0;
        std::string dldiSDPath = ui->txtDLDISDPath->text().toStdString();
        std::string dsiBios9Path = ui->txtDSiBIOS9Path->text().toStdString();
        std::string dsiBios7Path = ui->txtDSiBIOS7Path->text().toStdString();
        std::string dsiFirmwarePath = ui->txtDSiFirmwarePath->text().toStdString();
        std::string dsiNANDPath = ui->txtDSiNANDPath->text().toStdString();
        int dsiSDEnable = ui->cbDSiSDEnable->isChecked() ? 1:0;
        std::string dsiSDPath = ui->txtDSiSDPath->text().toStdString();

        if (consoleType != Config::ConsoleType
            || directBoot != Config::DirectBoot
#ifdef JIT_ENABLED
            || jitEnable != Config::JIT_Enable
            || jitMaxBlockSize != Config::JIT_MaxBlockSize
            || jitBranchOptimisations != Config::JIT_BranchOptimisations
            || jitLiteralOptimisations != Config::JIT_LiteralOptimisations
            || jitFastMemory != Config::JIT_FastMemory
#endif
            || strcmp(Config::BIOS9Path, bios9Path.c_str()) != 0
            || strcmp(Config::BIOS7Path, bios7Path.c_str()) != 0
            || strcmp(Config::FirmwarePath, firmwarePath.c_str()) != 0
            || dldiEnable != Config::DLDIEnable
            || strcmp(Config::DLDISDPath, dldiSDPath.c_str()) != 0
            || strcmp(Config::DSiBIOS9Path, dsiBios9Path.c_str()) != 0
            || strcmp(Config::DSiBIOS7Path, dsiBios7Path.c_str()) != 0
            || strcmp(Config::DSiFirmwarePath, dsiFirmwarePath.c_str()) != 0
            || strcmp(Config::DSiNANDPath, dsiNANDPath.c_str()) != 0
            || dsiSDEnable != Config::DSiSDEnable
            || strcmp(Config::DSiSDPath, dsiSDPath.c_str()) != 0)
        {
            if (RunningSomething
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            strncpy(Config::BIOS9Path, bios9Path.c_str(), 1023); Config::BIOS9Path[1023] = '\0';
            strncpy(Config::BIOS7Path, bios7Path.c_str(), 1023); Config::BIOS7Path[1023] = '\0';
            strncpy(Config::FirmwarePath, firmwarePath.c_str(), 1023); Config::FirmwarePath[1023] = '\0';
            Config::DLDIEnable = dldiEnable;
            strncpy(Config::DLDISDPath, dldiSDPath.c_str(), 1023); Config::DLDISDPath[1023] = '\0';

            strncpy(Config::DSiBIOS9Path, dsiBios9Path.c_str(), 1023); Config::DSiBIOS9Path[1023] = '\0';
            strncpy(Config::DSiBIOS7Path, dsiBios7Path.c_str(), 1023); Config::DSiBIOS7Path[1023] = '\0';
            strncpy(Config::DSiFirmwarePath, dsiFirmwarePath.c_str(), 1023); Config::DSiFirmwarePath[1023] = '\0';
            strncpy(Config::DSiNANDPath, dsiNANDPath.c_str(), 1023); Config::DSiNANDPath[1023] = '\0';
            Config::DSiSDEnable = dsiSDEnable;
            strncpy(Config::DSiSDPath, dsiSDPath.c_str(), 1023); Config::DSiSDPath[1023] = '\0';

    #ifdef JIT_ENABLED
            Config::JIT_Enable = jitEnable;
            Config::JIT_MaxBlockSize = jitMaxBlockSize;
            Config::JIT_BranchOptimisations = jitBranchOptimisations;
            Config::JIT_LiteralOptimisations = jitLiteralOptimisations;
            Config::JIT_FastMemory = jitFastMemory;
    #endif

            Config::ConsoleType = consoleType;
            Config::DirectBoot = directBoot;

            Config::Save();

            needsReset = true;
        }
    }

    QDialog::done(r);

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

void EmuSettingsDialog::on_btnDSiBIOS9Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi-mode ARM9 BIOS...",
                                                EmuDirectory,
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiBIOS9Path->setText(file);
}

void EmuSettingsDialog::on_btnDSiBIOS7Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi-mode ARM7 BIOS...",
                                                EmuDirectory,
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiBIOS7Path->setText(file);
}

void EmuSettingsDialog::on_btnDLDISDBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DLDI SD image...",
                                                EmuDirectory,
                                                "Image files (*.bin *.rom *.img);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDLDISDPath->setText(file);
}

void EmuSettingsDialog::on_btnDSiFirmwareBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi DS-mode firmware...",
                                                EmuDirectory,
                                                "Firmware files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiFirmwarePath->setText(file);
}

void EmuSettingsDialog::on_btnDSiNANDBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi NAND...",
                                                EmuDirectory,
                                                "NAND files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiNANDPath->setText(file);
}

void EmuSettingsDialog::on_btnDSiSDBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi SD image...",
                                                EmuDirectory,
                                                "Image files (*.bin *.rom *.img);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiSDPath->setText(file);
}

void EmuSettingsDialog::on_chkEnableJIT_toggled()
{
    bool disabled = !ui->chkEnableJIT->isChecked();
    ui->chkJITBranchOptimisations->setDisabled(disabled);
    ui->chkJITLiteralOptimisations->setDisabled(disabled);
    ui->chkJITFastMemory->setDisabled(disabled);
    ui->spnJITMaximumBlockSize->setDisabled(disabled);
}
