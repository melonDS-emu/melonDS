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

#include <stdio.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QTemporaryFile>

#include "types.h"
#include "Config.h"
#include "Platform.h"
#include "main.h"

#include "PathSettingsDialog.h"
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

    int inst = emuInstance->getInstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(QString("Configuring paths for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();

#define SET_ORIGVAL(type, val) \
    for (type* w : findChildren<type*>(nullptr)) \
        w->setProperty("user_originalValue", w->val());

    SET_ORIGVAL(QLineEdit, text);

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

#undef CHECK_ORIGVAL

        if (modified)
        {
            if (emuInstance->emuIsActive()
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            auto& cfg = emuInstance->getLocalConfig();
            cfg.SetQString("SaveFilePath", ui->txtSaveFilePath->text());
            cfg.SetQString("SavestatePath", ui->txtSavestatePath->text());
            cfg.SetQString("CheatFilePath", ui->txtCheatFilePath->text());

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
