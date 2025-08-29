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

#include <QFileDialog>
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"

#include "EmuSettingsDialog.h"
#include "ui_EmuSettingsDialog.h"
#include "main.h"

using namespace melonDS::Platform;
using namespace melonDS;

EmuSettingsDialog* EmuSettingsDialog::currentDlg = nullptr;

bool EmuSettingsDialog::needsReset = false;

inline void EmuSettingsDialog::updateLastBIOSFolder(QString& filename)
{
    int pos = filename.lastIndexOf("/");
    if (pos == -1)
    {
        pos = filename.lastIndexOf("\\");
    }

    QString path_dir = filename.left(pos);
    //QString path_file = filename.mid(pos+1);

    Config::Table cfg = Config::GetGlobalTable();
    cfg.SetQString("LastBIOSFolder", path_dir);
    lastBIOSFolder = path_dir;
}

EmuSettingsDialog::EmuSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::EmuSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();
    auto& cfg = emuInstance->getGlobalConfig();
    auto& instcfg = emuInstance->getLocalConfig();

    lastBIOSFolder = cfg.GetQString("LastBIOSFolder");

    ui->chkExternalBIOS->setChecked(cfg.GetBool("Emu.ExternalBIOSEnable"));
    ui->txtBIOS9Path->setText(cfg.GetQString("DS.BIOS9Path"));
    ui->txtBIOS7Path->setText(cfg.GetQString("DS.BIOS7Path"));
    ui->txtFirmwarePath->setText(cfg.GetQString("DS.FirmwarePath"));

    ui->txtDSiBIOS9Path->setText(cfg.GetQString("DSi.BIOS9Path"));
    ui->txtDSiBIOS7Path->setText(cfg.GetQString("DSi.BIOS7Path"));
    ui->txtDSiFirmwarePath->setText(cfg.GetQString("DSi.FirmwarePath"));
    ui->txtDSiNANDPath->setText(cfg.GetQString("DSi.NANDPath"));

    ui->cbxConsoleType->addItem("DS");
    ui->cbxConsoleType->addItem("DSi (experimental)");
    ui->cbxConsoleType->setCurrentIndex(cfg.GetInt("Emu.ConsoleType"));

    ui->chkDirectBoot->setChecked(cfg.GetBool("Emu.DirectBoot"));

#ifdef JIT_ENABLED
    ui->chkEnableJIT->setChecked(cfg.GetBool("JIT.Enable"));
    ui->chkJITBranchOptimisations->setChecked(cfg.GetBool("JIT.BranchOptimisations"));
    ui->chkJITLiteralOptimisations->setChecked(cfg.GetBool("JIT.LiteralOptimisations"));
    ui->chkJITFastMemory->setChecked(cfg.GetBool("JIT.FastMemory"));
    ui->spnJITMaximumBlockSize->setValue(cfg.GetInt("JIT.MaxBlockSize"));
#else
    ui->chkEnableJIT->setDisabled(true);
    ui->chkJITBranchOptimisations->setDisabled(true);
    ui->chkJITLiteralOptimisations->setDisabled(true);
    ui->chkJITFastMemory->setDisabled(true);
    ui->spnJITMaximumBlockSize->setDisabled(true);
#endif

#ifdef GDBSTUB_ENABLED
    ui->cbGdbEnabled->setChecked(instcfg.GetBool("Gdb.Enabled"));
    ui->intGdbPortA7->setValue(instcfg.GetInt("Gdb.ARM7.Port"));
    ui->intGdbPortA9->setValue(instcfg.GetInt("Gdb.ARM9.Port"));
    ui->cbGdbBOSA7->setChecked(instcfg.GetBool("Gdb.ARM7.BreakOnStartup"));
    ui->cbGdbBOSA9->setChecked(instcfg.GetBool("Gdb.ARM9.BreakOnStartup"));
#else
    ui->cbGdbEnabled->setDisabled(true);
    ui->intGdbPortA7->setDisabled(true);
    ui->intGdbPortA9->setDisabled(true);
    ui->cbGdbBOSA7->setDisabled(true);
    ui->cbGdbBOSA9->setDisabled(true);
#endif

    on_chkEnableJIT_toggled();
    on_cbGdbEnabled_toggled();
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

    ui->cbDLDIEnable->setChecked(cfg.GetBool("DLDI.Enable"));
    ui->txtDLDISDPath->setText(cfg.GetQString("DLDI.ImagePath"));
    ui->cbxDLDISize->setCurrentIndex(cfg.GetInt("DLDI.ImageSize"));
    ui->cbDLDIReadOnly->setChecked(cfg.GetBool("DLDI.ReadOnly"));
    ui->cbDLDIFolder->setChecked(cfg.GetBool("DLDI.FolderSync"));
    ui->txtDLDIFolder->setText(cfg.GetQString("DLDI.FolderPath"));
    on_cbDLDIEnable_toggled();

    ui->cbDSiFullBIOSBoot->setChecked(cfg.GetBool("DSi.FullBIOSBoot"));
    ui->cbDSPHLE->setChecked(cfg.GetBool("DSi.DSP.HLE"));

    ui->cbDSiSDEnable->setChecked(cfg.GetBool("DSi.SD.Enable"));
    ui->txtDSiSDPath->setText(cfg.GetQString("DSi.SD.ImagePath"));
    ui->cbxDSiSDSize->setCurrentIndex(cfg.GetInt("DSi.SD.ImageSize"));
    ui->cbDSiSDReadOnly->setChecked(cfg.GetBool("DSi.SD.ReadOnly"));
    ui->cbDSiSDFolder->setChecked(cfg.GetBool("DSi.SD.FolderSync"));
    ui->txtDSiSDFolder->setText(cfg.GetQString("DSi.SD.FolderPath"));
    on_cbDSiSDEnable_toggled();

#define SET_ORIGVAL(type, val) \
    for (type* w : findChildren<type*>(nullptr)) \
        w->setProperty("user_originalValue", w->val());

    SET_ORIGVAL(QLineEdit, text);
    SET_ORIGVAL(QSpinBox, value);
    SET_ORIGVAL(QComboBox, currentIndex);
    SET_ORIGVAL(QCheckBox, isChecked);

#undef SET_ORIGVAL
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
    FileHandle* f = Platform::OpenLocalFile(filename, FileMode::Read);
    if (!f) return;
    u8 chk1[0x180], chk2[0x180];

    FileRewind(f);
    FileRead(chk1, 1, 0x180, f);
    FileSeek(f, -0x380, FileSeekOrigin::End);
    FileRead(chk2, 1, 0x180, f);

    memset(&chk1[0x0C], 0, 8);
    memset(&chk2[0x0C], 0, 8);

    CloseFile(f);

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
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    needsReset = false;

    if (r == QDialog::Accepted)
    {
        bool modified = false;

#define CHECK_ORIGVAL(type, val) \
        if (!modified) for (type* w : findChildren<type*>(nullptr)) \
        {                        \
            QVariant v = w->val();                   \
            if (v != w->property("user_originalValue")) \
            {                    \
                modified = true; \
                break;                   \
            }\
        }

        CHECK_ORIGVAL(QLineEdit, text);
        CHECK_ORIGVAL(QSpinBox, value);
        CHECK_ORIGVAL(QComboBox, currentIndex);
        CHECK_ORIGVAL(QCheckBox, isChecked);

#undef CHECK_ORIGVAL

        if (QVariant(ui->txtFirmwarePath->text()) != ui->txtFirmwarePath->property("user_originalValue"))
            verifyFirmware();

        if (modified)
        {
            if (emuInstance->emuIsActive()
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            auto& cfg = emuInstance->getGlobalConfig();
            auto& instcfg = emuInstance->getLocalConfig();

            cfg.SetBool("Emu.ExternalBIOSEnable", ui->chkExternalBIOS->isChecked());
            cfg.SetQString("DS.BIOS9Path", ui->txtBIOS9Path->text());
            cfg.SetQString("DS.BIOS7Path", ui->txtBIOS7Path->text());
            cfg.SetQString("DS.FirmwarePath", ui->txtFirmwarePath->text());

            cfg.SetBool("DLDI.Enable", ui->cbDLDIEnable->isChecked());
            cfg.SetQString("DLDI.ImagePath", ui->txtDLDISDPath->text());
            cfg.SetInt("DLDI.ImageSize", ui->cbxDLDISize->currentIndex());
            cfg.SetBool("DLDI.ReadOnly", ui->cbDLDIReadOnly->isChecked());
            cfg.SetBool("DLDI.FolderSync", ui->cbDLDIFolder->isChecked());
            cfg.SetQString("DLDI.FolderPath", ui->txtDLDIFolder->text());

            cfg.SetQString("DSi.BIOS9Path", ui->txtDSiBIOS9Path->text());
            cfg.SetQString("DSi.BIOS7Path", ui->txtDSiBIOS7Path->text());
            cfg.SetQString("DSi.FirmwarePath", ui->txtDSiFirmwarePath->text());
            cfg.SetQString("DSi.NANDPath", ui->txtDSiNANDPath->text());

            cfg.SetBool("DSi.FullBIOSBoot", ui->cbDSiFullBIOSBoot->isChecked());
            cfg.SetBool("DSi.DSP.HLE", ui->cbDSPHLE->isChecked());

            cfg.SetBool("DSi.SD.Enable", ui->cbDSiSDEnable->isChecked());
            cfg.SetQString("DSi.SD.ImagePath", ui->txtDSiSDPath->text());
            cfg.SetInt("DSi.SD.ImageSize", ui->cbxDSiSDSize->currentIndex());
            cfg.SetBool("DSi.SD.ReadOnly", ui->cbDSiSDReadOnly->isChecked());
            cfg.SetBool("DSi.SD.FolderSync", ui->cbDSiSDFolder->isChecked());
            cfg.SetQString("DSi.SD.FolderPath", ui->txtDSiSDFolder->text());

#ifdef JIT_ENABLED
            cfg.SetBool("JIT.Enable", ui->chkEnableJIT->isChecked());
            cfg.SetInt("JIT.MaxBlockSize", ui->spnJITMaximumBlockSize->value());
            cfg.SetBool("JIT.BranchOptimisations", ui->chkJITBranchOptimisations->isChecked());
            cfg.SetBool("JIT.LiteralOptimisations", ui->chkJITLiteralOptimisations->isChecked());
            cfg.SetBool("JIT.FastMemory", ui->chkJITFastMemory->isChecked());
#endif
#ifdef GDBSTUB_ENABLED
            instcfg.SetBool("Gdb.Enabled", ui->cbGdbEnabled->isChecked());
            instcfg.SetInt("Gdb.ARM7.Port", ui->intGdbPortA7->value());
            instcfg.SetInt("Gdb.ARM9.Port", ui->intGdbPortA9->value());
            instcfg.SetBool("Gdb.ARM7.BreakOnStartup", ui->cbGdbBOSA7->isChecked());
            instcfg.SetBool("Gdb.ARM9.BreakOnStartup", ui->cbGdbBOSA9->isChecked());
#endif

            cfg.SetInt("Emu.ConsoleType", ui->cbxConsoleType->currentIndex());
            cfg.SetBool("Emu.DirectBoot", ui->chkDirectBoot->isChecked());

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
                                                lastBIOSFolder,
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    updateLastBIOSFolder(file);

    ui->txtBIOS9Path->setText(file);
}

void EmuSettingsDialog::on_btnBIOS7Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DS-mode ARM7 BIOS...",
                                                lastBIOSFolder,
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    updateLastBIOSFolder(file);

    ui->txtBIOS7Path->setText(file);
}

void EmuSettingsDialog::on_btnFirmwareBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DS-mode firmware...",
                                                lastBIOSFolder,
                                                "Firmware files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    if (!Platform::CheckFileWritable(file.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", "Unable to write to firmware file.\nPlease check file/folder write permissions.");
        return;
    }

    updateLastBIOSFolder(file);

    ui->txtFirmwarePath->setText(file);
}

void EmuSettingsDialog::on_btnDSiBIOS9Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi-mode ARM9 BIOS...",
                                                lastBIOSFolder,
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    updateLastBIOSFolder(file);

    ui->txtDSiBIOS9Path->setText(file);
}

void EmuSettingsDialog::on_btnDSiBIOS7Browse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi-mode ARM7 BIOS...",
                                                lastBIOSFolder,
                                                "BIOS files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    updateLastBIOSFolder(file);

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
                                                lastBIOSFolder,
                                                "Image files (*.bin *.rom *.img *.dmg);;Any file (*.*)");

    if (file.isEmpty()) return;

    if (!Platform::CheckFileWritable(file.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", "Unable to write to DLDI SD image.\nPlease check file/folder write permissions.");
        return;
    }

    updateLastBIOSFolder(file);

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
                                                     lastBIOSFolder);

    if (dir.isEmpty()) return;

    ui->txtDLDIFolder->setText(dir);
}

void EmuSettingsDialog::on_btnDSiFirmwareBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi DS-mode firmware...",
                                                lastBIOSFolder,
                                                "Firmware files (*.bin *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    if (!Platform::CheckFileWritable(file.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", "Unable to write to DSi firmware file.\nPlease check file/folder write permissions.");
        return;
    }


    updateLastBIOSFolder(file);

    ui->txtDSiFirmwarePath->setText(file);
}

void EmuSettingsDialog::on_btnDSiNANDBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select DSi NAND...",
                                                lastBIOSFolder,
                                                "NAND files (*.bin *.mmc *.rom);;Any file (*.*)");

    if (file.isEmpty()) return;

    if (!Platform::CheckFileWritable(file.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", "Unable to write to DSi NAND image.\nPlease check file/folder write permissions.");
        return;
    }


    updateLastBIOSFolder(file);

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
                                                lastBIOSFolder,
                                                "Image files (*.bin *.rom *.img *.sd *.dmg);;Any file (*.*)");

    if (file.isEmpty()) return;

    if (!Platform::CheckFileWritable(file.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", "Unable to write to DSi SD image.\nPlease check file/folder write permissions.");
        return;
    }

    updateLastBIOSFolder(file);

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
                                                     lastBIOSFolder);

    if (dir.isEmpty()) return;

    ui->txtDSiSDFolder->setText(dir);
}

void EmuSettingsDialog::on_chkEnableJIT_toggled()
{
    bool disabled = !ui->chkEnableJIT->isChecked();
#ifdef JIT_ENABLED
    bool fastmemSupported = ARMJIT_Memory::IsFastMemSupported();
#else
    bool fastmemSupported = false;
#endif
    ui->chkJITBranchOptimisations->setDisabled(disabled);
    ui->chkJITLiteralOptimisations->setDisabled(disabled);
    ui->chkJITFastMemory->setDisabled(disabled || !fastmemSupported);
    ui->spnJITMaximumBlockSize->setDisabled(disabled);

    on_cbGdbEnabled_toggled();
}

void EmuSettingsDialog::on_cbGdbEnabled_toggled()
{
#ifdef GDBSTUB_ENABLED
    bool disabled = !ui->cbGdbEnabled->isChecked();
    bool jitenable = ui->chkEnableJIT->isChecked();

    if (jitenable && !disabled) {
        ui->cbGdbEnabled->setChecked(false);
        disabled = true;
    }
#else
    bool disabled = true;
    bool jitenable = true;
    ui->cbGdbEnabled->setChecked(false);
#endif

    ui->cbGdbEnabled->setDisabled(jitenable);
    ui->intGdbPortA7->setDisabled(disabled);
    ui->intGdbPortA9->setDisabled(disabled);
    ui->cbGdbBOSA7->setDisabled(disabled);
    ui->cbGdbBOSA9->setDisabled(disabled);
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
