/*
    Copyright 2016-2026 melonDS team

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
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QUrl>

#include "types.h"
#include "Config.h"
#include "Platform.h"
#include "main.h"

#include "PathSettingsDialog.h"
#include "SaveConversionDialog.h"
#include "ui_PathSettingsDialog.h"

using namespace melonDS::Platform;
namespace Platform = melonDS::Platform;

PathSettingsDialog* PathSettingsDialog::currentDlg = nullptr;

bool PathSettingsDialog::needsReset = false;

constexpr char errordialog[] = "melonDS cannot write to that directory.";

PathSettingsDialog::PathSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::PathSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto& cfg = emuInstance->getLocalConfig();
    ui->txtSaveFilePath->setText(cfg.GetQString("SaveFilePath"));
    ui->txtSavestatePath->setText(cfg.GetQString("SavestatePath"));
    ui->txtCheatFilePath->setText(cfg.GetQString("CheatFilePath"));

    ui->cbxSaveFileExtension->addItem(".sav");
    ui->cbxSaveFileExtension->addItem(".srm");
    int extidx = ui->cbxSaveFileExtension->findText(cfg.GetQString("SaveFileExtension"));
    ui->cbxSaveFileExtension->setCurrentIndex(extidx < 0 ? 0 : extidx);

    int inst = emuInstance->getInstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(QString("Configuring paths for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();

#define SET_ORIGVAL(type, val) \
    for (type* w : findChildren<type*>(nullptr)) \
        w->setProperty("user_originalValue", w->val());

    SET_ORIGVAL(QLineEdit, text);
    SET_ORIGVAL(QComboBox, currentIndex);

#undef SET_ORIGVAL
}

PathSettingsDialog::~PathSettingsDialog()
{
    delete ui;
}

void PathSettingsDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }
    
    // a conversion moved files, so re-resolve even if this is a cancel
    needsReset = savesConverted;

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
        CHECK_ORIGVAL(QComboBox, currentIndex);

#undef CHECK_ORIGVAL

        if (modified)
        {
            if (emuInstance->emuIsActive()
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            if (!confirmSaveOverwrite(ui->txtSaveFilePath->text(),
                                      ui->cbxSaveFileExtension->currentText()))
                return;

            auto& cfg = emuInstance->getLocalConfig();
            cfg.SetQString("SaveFilePath", ui->txtSaveFilePath->text());
            cfg.SetQString("SavestatePath", ui->txtSavestatePath->text());
            cfg.SetQString("CheatFilePath", ui->txtCheatFilePath->text());
            cfg.SetQString("SaveFileExtension", ui->cbxSaveFileExtension->currentText());

            Config::Save();

            needsReset = true;
        }
    }

    QDialog::done(r);

    closeDlg();
}

void PathSettingsDialog::on_btnSaveFileBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                     "Select save files path...",
                                                     emuDirectory);

    if (dir.isEmpty()) return;
    
    if (!QTemporaryFile(dir).open())
    {
        QMessageBox::critical(this, "melonDS", errordialog);
        return;
    }

    ui->txtSaveFilePath->setText(dir);
}

void PathSettingsDialog::on_btnSavestateBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                     "Select savestates path...",
                                                     emuDirectory);

    if (dir.isEmpty()) return;
    
    if (!QTemporaryFile(dir).open())
    {
        QMessageBox::critical(this, "melonDS", errordialog);
        return;
    }

    ui->txtSavestatePath->setText(dir);
}

void PathSettingsDialog::on_btnCheatFileBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                     "Select cheat files path...",
                                                     emuDirectory);

    if (dir.isEmpty()) return;
    
    if (!QTemporaryFile(dir).open())
    {
        QMessageBox::critical(this, "melonDS", errordialog);
        return;
    }

    ui->txtCheatFilePath->setText(dir);
}

// Retargeting a live SaveManager flushes the in-memory save to the new path, which
// truncates whatever is there. Name the file rather than let it vanish silently.
bool PathSettingsDialog::confirmSaveOverwrite(const QString& dir, const QString& ext)
{
    if (!emuInstance->emuIsActive()) return true;

    std::string victim = emuInstance->saveOverwrittenBySettings(dir.toStdString(),
                                                                ext.toStdString());
    if (victim.empty()) return true;

    QString msg = QString("%1 already exists and will be overwritten with the save "
                          "data currently in memory. Its contents will be lost.\n\n"
                          "Continue?")
                  .arg(QFileInfo(QString::fromStdString(victim)).fileName());

    return QMessageBox::warning(this, "melonDS", msg,
                                QMessageBox::Yes|QMessageBox::No,
                                QMessageBox::No) == QMessageBox::Yes;
}

void PathSettingsDialog::on_btnOpenSaveDir_clicked()
{
    QString dir = ui->txtSaveFilePath->text();
    if (dir.isEmpty())
    {
        QMessageBox::information(this, "melonDS",
            "There is no save files path set.\n\nWith a blank path, saves are kept next "
            "to each ROM rather than in one directory.");
        return;
    }

    if (!QFileInfo(dir).isDir())
    {
        QMessageBox::warning(this, "melonDS", "That save files path does not exist.");
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void PathSettingsDialog::on_btnConvertSaves_clicked()
{
    QString dir = ui->txtSaveFilePath->text();
    if (dir.isEmpty())
    {
        QMessageBox::information(this, "melonDS",
            "Set a save files path first.\n\nWith a blank path, saves are kept next to "
            "each ROM rather than in one directory, so there is no single place to convert.");
        return;
    }

    if (!QFileInfo(dir).isDir())
    {
        QMessageBox::warning(this, "melonDS", "That save files path does not exist.");
        return;
    }

    // both from the combobox, so an unrecognised stored value can't build a target
    int idx = ui->cbxSaveFileExtension->currentIndex();
    QString toext = ui->cbxSaveFileExtension->itemText(idx);
    QString fromext = ui->cbxSaveFileExtension->itemText(idx == 0 ? 1 : 0);

    // asked before anything is renamed, since renaming cannot be undone
    if (!confirmSaveOverwrite(dir, toext))
        return;

    SaveConversionDialog dlg(this, dir, fromext, toext, emuInstance->emuIsActive());
    dlg.exec();

    if (dlg.renamedCount() > 0)
    {
        // the rename cannot be undone, so the setting has to be saved with it or
        // Cancel would leave the config and the files on different extensions
        auto& cfg = emuInstance->getLocalConfig();
        cfg.SetQString("SaveFilePath", dir);
        cfg.SetQString("SaveFileExtension", toext);
        Config::Save();

        ui->txtSaveFilePath->setProperty("user_originalValue", dir);
        ui->cbxSaveFileExtension->setProperty("user_originalValue", idx);

        savesConverted = true;
    }
}
