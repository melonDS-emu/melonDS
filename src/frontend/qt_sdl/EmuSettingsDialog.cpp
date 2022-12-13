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
#include <QMessageBox>
#include <QList>
#include <QDateEdit>

#include "types.h"
#include "Platform.h"
#include "Config.h"

#include "EmuSettingsDialog.h"
#include "ui_EmuSettingsDialog.h"

#ifdef DEBUG_FEATURES_ENABLED
#include "debug/st.h"
#endif


EmuSettingsDialog* EmuSettingsDialog::currentDlg = nullptr;

extern std::string EmuDirectory;
extern bool RunningSomething;

bool EmuSettingsDialog::needsReset = false;

EmuSettingsDialog::EmuSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::EmuSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    cbArray[0] = ui->cb_A7stat;
    cbArray[1] = ui->cb_A9stat;
    cbArray[2] = ui->cb_mem;
    cbArray[3] = ui->cb_disp;
    cbArray[4] = ui->cb_disp3d;
    cbArray[5] = ui->cb_dma;
    cbArray[6] = ui->cb_ipcfifo;
    cbArray[7] = ui->cb_audio;
    cbArray[8] = ui->cb_timers;
    cbArray[9] = ui->cb_int;
    cbArray[10] = ui->cb_divsqrt;
    cbArray[11] = ui->cb_pwman;
    cbArray[12] = ui->cb_scfg;
    cbArray[13] = ui->cb_dsp;
    cbArray[14] = ui->cb_sdmmc_host;
    cbArray[15] = ui->cb_sdmmc_card;
    cbArray[16] = ui->cb_ntrwifi;
    cbArray[17] = ui->cb_dsiwifi_host;
    cbArray[18] = ui->cb_dsiwifi_board;
    cbArray[19] = ui->cb_dsiwifi_xtensa;
    cbArray[20] = ui->cb_gpio;
    cbArray[21] = ui->cb_keypad;
    cbArray[22] = ui->cb_ntrmic;
    cbArray[23] = ui->cb_dsimic;
    cbArray[24] = ui->cb_rtc;
    cbArray[25] = ui->cb_spi;
    cbArray[26] = ui->cb_auxspi;
    cbArray[27] = ui->cb_tsc;
    cbArray[28] = ui->cb_i2c;
    cbArray[29] = ui->cb_dsiwifi_firm;
    cbArray[30] = ui->cb_cam;
    cbArray[63] = ui->cb_custom;

    ui->chkExternalBIOS->setChecked(Config::ExternalBIOSEnable);
    ui->txtBIOS9Path->setText(QString::fromStdString(Config::BIOS9Path));
    ui->txtBIOS7Path->setText(QString::fromStdString(Config::BIOS7Path));
    ui->txtFirmwarePath->setText(QString::fromStdString(Config::FirmwarePath));

    ui->txtDSiBIOS9Path->setText(QString::fromStdString(Config::DSiBIOS9Path));
    ui->txtDSiBIOS7Path->setText(QString::fromStdString(Config::DSiBIOS7Path));
    ui->txtDSiFirmwarePath->setText(QString::fromStdString(Config::DSiFirmwarePath));
    ui->txtDSiNANDPath->setText(QString::fromStdString(Config::DSiNANDPath));

    ui->cbxConsoleType->addItem("DS");
    ui->cbxConsoleType->addItem("DSi (experimental)");
    ui->cbxConsoleType->setCurrentIndex(Config::ConsoleType);

    ui->chkDirectBoot->setChecked(Config::DirectBoot);

#ifdef JIT_ENABLED
    ui->chkEnableJIT->setChecked(Config::JIT_Enable);
    ui->chkJITBranchOptimisations->setChecked(Config::JIT_BranchOptimisations);
    ui->chkJITLiteralOptimisations->setChecked(Config::JIT_LiteralOptimisations);
    ui->chkJITFastMemory->setChecked(Config::JIT_FastMemory);
    #ifdef __APPLE__
        ui->chkJITFastMemory->setDisabled(true);
    #endif
    ui->spnJITMaximumBlockSize->setValue(Config::JIT_MaxBlockSize);
#else
    ui->chkEnableJIT->setDisabled(true);
    ui->chkJITBranchOptimisations->setDisabled(true);
    ui->chkJITLiteralOptimisations->setDisabled(true);
    ui->chkJITFastMemory->setDisabled(true);
    ui->spnJITMaximumBlockSize->setDisabled(true);
#endif

#ifdef DEBUG_FEATURES_ENABLED
    ui->gbTracing->setChecked(Config::DBG_EnableTracing);
    ui->gbTracing->setCheckable(true);

    ui->txtLXTPath->setText(QString::fromStdString(Config::DBG_LXTPath));

    // FIXME this is thowing segfautls fsr
    /*for (int i = 0; i < 64; ++i)
        if (cbArray[i] != nullptr)
            cbArray[i]->setChecked((Config::DBG_EnabledSignals & (1uLL<<i)) != 0);*/

    ui->gbHypercall->setChecked(Config::DBG_EnableHypercalls);
    ui->gbHypercall->setCheckable(true);

    ui->cb_hv_misc->setChecked(Config::DBG_HVMisc);
    ui->cb_hv_sig->setChecked(Config::DBG_HVSignalTracing);
#else
    ui->gbTracing->setChecked(false);
    ui->gbTracing->setCheckable(false);

    ui->gbHypercall->setChecked(false);
    ui->gbHypercall->setCheckable(false);
#endif

    on_chkEnableJIT_toggled();
    on_chkExternalBIOS_toggled();

    const int imgsizes[] = {256, 512, 1024, 2048, 4096, 0};

    ui->cbxDLDISize->addItem("Auto");
    ui->cbxDSiSDSize->addItem("Auto");

    for (int i = 0; imgsizes[i] != 0; i++)
    {
        int sizedisp = imgsizes[i];
        QString sizelbl;
        if (sizedisp >= 1024)
        {
            sizedisp >>= 10;
            sizelbl = QString("%1 GB").arg(sizedisp);
        }
        else
        {
            sizelbl = QString("%1 MB").arg(sizedisp);
        }

        ui->cbxDLDISize->addItem(sizelbl);
        ui->cbxDSiSDSize->addItem(sizelbl);
    }

    ui->cbDLDIEnable->setChecked(Config::DLDIEnable);
    ui->txtDLDISDPath->setText(QString::fromStdString(Config::DLDISDPath));
    ui->cbxDLDISize->setCurrentIndex(Config::DLDISize);
    ui->cbDLDIReadOnly->setChecked(Config::DLDIReadOnly);
    ui->cbDLDIFolder->setChecked(Config::DLDIFolderSync);
    ui->txtDLDIFolder->setText(QString::fromStdString(Config::DLDIFolderPath));
    on_cbDLDIEnable_toggled();

    ui->cbDSiSDEnable->setChecked(Config::DSiSDEnable);
    ui->txtDSiSDPath->setText(QString::fromStdString(Config::DSiSDPath));
    ui->cbxDSiSDSize->setCurrentIndex(Config::DSiSDSize);
    ui->cbDSiSDReadOnly->setChecked(Config::DSiSDReadOnly);
    ui->cbDSiSDFolder->setChecked(Config::DSiSDFolderSync);
    ui->txtDSiSDFolder->setText(QString::fromStdString(Config::DSiSDFolderPath));
    on_cbDSiSDEnable_toggled();
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

    std::string filename = ui->txtFirmwarePath->text().toStdString();
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

void EmuSettingsDialog::done(int r)
{
    needsReset = false;

    if (r == QDialog::Accepted)
    {
        verifyFirmware();

        int consoleType = ui->cbxConsoleType->currentIndex();
        bool directBoot = ui->chkDirectBoot->isChecked();

        bool jitEnable = ui->chkEnableJIT->isChecked();
        int jitMaxBlockSize = ui->spnJITMaximumBlockSize->value();
        bool jitBranchOptimisations = ui->chkJITBranchOptimisations->isChecked();
        bool jitLiteralOptimisations = ui->chkJITLiteralOptimisations->isChecked();
        bool jitFastMemory = ui->chkJITFastMemory->isChecked();

        bool externalBiosEnable = ui->chkExternalBIOS->isChecked();
        std::string bios9Path = ui->txtBIOS9Path->text().toStdString();
        std::string bios7Path = ui->txtBIOS7Path->text().toStdString();
        std::string firmwarePath = ui->txtFirmwarePath->text().toStdString();

        bool dldiEnable = ui->cbDLDIEnable->isChecked();
        std::string dldiSDPath = ui->txtDLDISDPath->text().toStdString();
        int dldiSize = ui->cbxDLDISize->currentIndex();
        bool dldiReadOnly = ui->cbDLDIReadOnly->isChecked();
        bool dldiFolderSync = ui->cbDLDIFolder->isChecked();
        std::string dldiFolderPath = ui->txtDLDIFolder->text().toStdString();

        std::string dsiBios9Path = ui->txtDSiBIOS9Path->text().toStdString();
        std::string dsiBios7Path = ui->txtDSiBIOS7Path->text().toStdString();
        std::string dsiFirmwarePath = ui->txtDSiFirmwarePath->text().toStdString();
        std::string dsiNANDPath = ui->txtDSiNANDPath->text().toStdString();

        bool dsiSDEnable = ui->cbDSiSDEnable->isChecked();
        std::string dsiSDPath = ui->txtDSiSDPath->text().toStdString();
        int dsiSDSize = ui->cbxDSiSDSize->currentIndex();
        bool dsiSDReadOnly = ui->cbDSiSDReadOnly->isChecked();
        bool dsiSDFolderSync = ui->cbDSiSDFolder->isChecked();
        std::string dsiSDFolderPath = ui->txtDSiSDFolder->text().toStdString();

        int enableTracing = ui->gbTracing->isChecked()?1:0;
        std::string lxtPath = ui->txtLXTPath->text().toStdString();
        unsigned long long enabledSignals = 1|2|4|32|64|256|512|(1<<11)|(1<<12)|(1<<20)|(1<<25)|(1<<28);
        // FIXME: this is also segfaulting!
        /*for (int i = 0; i < 64; ++i)
            if (cbArray[i] != nullptr && cbArray[i]->isChecked())
                enabledSignals |= 1uLL<<i;*/

        int enableHypercalls = ui->gbHypercall->isChecked()?1:0;
        int hv_misc = ui->cb_hv_misc->isChecked()?1:0;
        int hv_sig = ui->cb_hv_sig->isChecked()?1:0;

        if (consoleType != Config::ConsoleType
            || directBoot != Config::DirectBoot
#ifdef JIT_ENABLED
            || jitEnable != Config::JIT_Enable
            || jitMaxBlockSize != Config::JIT_MaxBlockSize
            || jitBranchOptimisations != Config::JIT_BranchOptimisations
            || jitLiteralOptimisations != Config::JIT_LiteralOptimisations
            || jitFastMemory != Config::JIT_FastMemory
#endif
#ifdef DEBUG_FEATURES_ENABLED
            || enableTracing != Config::DBG_EnableTracing
            || lxtPath != Config::DBG_LXTPath
            || enabledSignals != Config::DBG_EnabledSignals

            || enableHypercalls != Config::DBG_EnableHypercalls
            || hv_misc != Config::DBG_HVMisc
            || hv_sig != Config::DBG_HVSignalTracing
#endif
            || externalBiosEnable != Config::ExternalBIOSEnable
            || bios9Path != Config::BIOS9Path
            || bios7Path != Config::BIOS7Path
            || firmwarePath != Config::FirmwarePath
            || dldiEnable != Config::DLDIEnable
            || dldiSDPath != Config::DLDISDPath
            || dldiSize != Config::DLDISize
            || dldiReadOnly != Config::DLDIReadOnly
            || dldiFolderSync != Config::DLDIFolderSync
            || dldiFolderPath != Config::DLDIFolderPath
            || dsiBios9Path != Config::DSiBIOS9Path
            || dsiBios7Path != Config::DSiBIOS7Path
            || dsiFirmwarePath != Config::DSiFirmwarePath
            || dsiNANDPath != Config::DSiNANDPath
            || dsiSDEnable != Config::DSiSDEnable
            || dsiSDPath != Config::DSiSDPath
            || dsiSDSize != Config::DSiSDSize
            || dsiSDReadOnly != Config::DSiSDReadOnly
            || dsiSDFolderSync != Config::DSiSDFolderSync
            || dsiSDFolderPath != Config::DSiSDFolderPath)
        {
            if (RunningSomething
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            Config::ExternalBIOSEnable = externalBiosEnable;
            Config::BIOS9Path = bios9Path;
            Config::BIOS7Path = bios7Path;
            Config::FirmwarePath = firmwarePath;

            Config::DLDIEnable = dldiEnable;
            Config::DLDISDPath = dldiSDPath;
            Config::DLDISize = dldiSize;
            Config::DLDIReadOnly = dldiReadOnly;
            Config::DLDIFolderSync = dldiFolderSync;
            Config::DLDIFolderPath = dldiFolderPath;

            Config::DSiBIOS9Path = dsiBios9Path;
            Config::DSiBIOS7Path = dsiBios7Path;
            Config::DSiFirmwarePath = dsiFirmwarePath;
            Config::DSiNANDPath = dsiNANDPath;

            Config::DSiSDEnable = dsiSDEnable;
            Config::DSiSDPath = dsiSDPath;
            Config::DSiSDSize = dsiSDSize;
            Config::DSiSDReadOnly = dsiSDReadOnly;
            Config::DSiSDFolderSync = dsiSDFolderSync;
            Config::DSiSDFolderPath = dsiSDFolderPath;

#ifdef JIT_ENABLED
            Config::JIT_Enable = jitEnable;
            Config::JIT_MaxBlockSize = jitMaxBlockSize;
            Config::JIT_BranchOptimisations = jitBranchOptimisations;
            Config::JIT_LiteralOptimisations = jitLiteralOptimisations;
            Config::JIT_FastMemory = jitFastMemory;
#endif
#ifdef DEBUG_FEATURES_ENABLED
            Config::DBG_EnableTracing = enableTracing;
            Config::DBG_LXTPath = lxtPath;
            Config::DBG_EnabledSignals = enabledSignals;

            Config::DBG_EnableHypercalls = enableHypercalls;
            Config::DBG_HVMisc = hv_misc;
            Config::DBG_HVSignalTracing = hv_sig;
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
                                                QString::fromStdString(EmuDirectory),
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtBIOS9Path->setText(file);
}

void EmuSettingsDialog::on_btnBIOS7Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DS-mode ARM7 BIOS...",
                                                QString::fromStdString(EmuDirectory),
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtBIOS7Path->setText(file);
}

void EmuSettingsDialog::on_btnFirmwareBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DS-mode firmware...",
                                                QString::fromStdString(EmuDirectory),
                                                "Firmware files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtFirmwarePath->setText(file);
}

void EmuSettingsDialog::on_btnDSiBIOS9Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi-mode ARM9 BIOS...",
                                                QString::fromStdString(EmuDirectory),
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiBIOS9Path->setText(file);
}

void EmuSettingsDialog::on_btnDSiBIOS7Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi-mode ARM7 BIOS...",
                                                QString::fromStdString(EmuDirectory),
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiBIOS7Path->setText(file);
}

void EmuSettingsDialog::on_cbDLDIEnable_toggled()
{
    bool disabled = !ui->cbDLDIEnable->isChecked();
    ui->txtDLDISDPath->setDisabled(disabled);
    ui->btnDLDISDBrowse->setDisabled(disabled);
    ui->cbxDLDISize->setDisabled(disabled);
    ui->cbDLDIReadOnly->setDisabled(disabled);
    ui->cbDLDIFolder->setDisabled(disabled);

    if (!disabled) disabled = !ui->cbDLDIFolder->isChecked();
    ui->txtDLDIFolder->setDisabled(disabled);
    ui->btnDLDIFolderBrowse->setDisabled(disabled);
}

void EmuSettingsDialog::on_btnDLDISDBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DLDI SD image...",
                                                QString::fromStdString(EmuDirectory),
                                                "Image files (*.bin *.rom *.img *.dmg);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDLDISDPath->setText(file);
}

void EmuSettingsDialog::on_cbDLDIFolder_toggled()
{
    bool disabled = !ui->cbDLDIFolder->isChecked();
    ui->txtDLDIFolder->setDisabled(disabled);
    ui->btnDLDIFolderBrowse->setDisabled(disabled);
}

void EmuSettingsDialog::on_btnDLDIFolderBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                     "Select DLDI SD folder...",
                                                     QString::fromStdString(EmuDirectory));

    if (dir.isEmpty()) return;

    ui->txtDLDIFolder->setText(dir);
}

void EmuSettingsDialog::on_btnDSiFirmwareBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi DS-mode firmware...",
                                                QString::fromStdString(EmuDirectory),
                                                "Firmware files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiFirmwarePath->setText(file);
}

void EmuSettingsDialog::on_btnDSiNANDBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi NAND...",
                                                QString::fromStdString(EmuDirectory),
                                                "NAND files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiNANDPath->setText(file);
}

void EmuSettingsDialog::on_cbDSiSDEnable_toggled()
{
    bool disabled = !ui->cbDSiSDEnable->isChecked();
    ui->txtDSiSDPath->setDisabled(disabled);
    ui->btnDSiSDBrowse->setDisabled(disabled);
    ui->cbxDSiSDSize->setDisabled(disabled);
    ui->cbDSiSDReadOnly->setDisabled(disabled);
    ui->cbDSiSDFolder->setDisabled(disabled);

    if (!disabled) disabled = !ui->cbDSiSDFolder->isChecked();
    ui->txtDSiSDFolder->setDisabled(disabled);
    ui->btnDSiSDFolderBrowse->setDisabled(disabled);
}

void EmuSettingsDialog::on_btnDSiSDBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi SD image...",
                                                QString::fromStdString(EmuDirectory),
                                                "Image files (*.bin *.rom *.img *.dmg);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtDSiSDPath->setText(file);
}

void EmuSettingsDialog::on_cbDSiSDFolder_toggled()
{
    bool disabled = !ui->cbDSiSDFolder->isChecked();
    ui->txtDSiSDFolder->setDisabled(disabled);
    ui->btnDSiSDFolderBrowse->setDisabled(disabled);
}

void EmuSettingsDialog::on_btnDSiSDFolderBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                     "Select DSi SD folder...",
                                                     QString::fromStdString(EmuDirectory));

    if (dir.isEmpty()) return;

    ui->txtDSiSDFolder->setText(dir);
}

void EmuSettingsDialog::on_btnLXTBrowse_clicked()
{
    QString file = QFileDialog::getSaveFileName(this,
            "Select LXT output file...",
            get_current_dir_name(),
            "LXT files (*.lxt);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtLXTPath->setText(file);
}

void EmuSettingsDialog::on_chkEnableJIT_toggled()
{
    bool disabled = !ui->chkEnableJIT->isChecked();
    ui->chkJITBranchOptimisations->setDisabled(disabled);
    ui->chkJITLiteralOptimisations->setDisabled(disabled);
    #ifndef __APPLE__
        ui->chkJITFastMemory->setDisabled(disabled);
    #endif
    ui->spnJITMaximumBlockSize->setDisabled(disabled);
}

void EmuSettingsDialog::on_chkExternalBIOS_toggled()
{
    bool disabled = !ui->chkExternalBIOS->isChecked();
    ui->txtBIOS9Path->setDisabled(disabled);
    ui->btnBIOS9Browse->setDisabled(disabled);
    ui->txtBIOS7Path->setDisabled(disabled);
    ui->btnBIOS7Browse->setDisabled(disabled);
    ui->txtFirmwarePath->setDisabled(disabled);
    ui->btnFirmwareBrowse->setDisabled(disabled);
}
