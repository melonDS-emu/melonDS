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

#include <stdio.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QTemporaryFile>

#include "types.h"
#include "Config.h"
#include "Platform.h"

#include "PathSettingsDialog.h"
#include "ui_PathSettingsDialog.h"

using namespace melonDS::Platform;
namespace Platform = melonDS::Platform;

PathSettingsDialog* PathSettingsDialog::currentDlg = nullptr;

extern std::string EmuDirectory;
extern bool RunningSomething;

bool PathSettingsDialog::needsReset = false;

constexpr char errordialog[] = "melonDS cannot write to that directory.";

PathSettingsDialog::PathSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::PathSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->txtSaveFilePath->setText(QString::fromStdString(Config::SaveFilePath));
    ui->txtSavestatePath->setText(QString::fromStdString(Config::SavestatePath));
    ui->txtCheatFilePath->setText(QString::fromStdString(Config::CheatFilePath));

    int inst = Platform::InstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(QString("Configuring paths for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();
}

PathSettingsDialog::~PathSettingsDialog()
{
    delete ui;
}

void PathSettingsDialog::done(int r)
{
    needsReset = false;

    if (r == QDialog::Accepted)
    {
        std::string saveFilePath = ui->txtSaveFilePath->text().toStdString();
        std::string savestatePath = ui->txtSavestatePath->text().toStdString();
        std::string cheatFilePath = ui->txtCheatFilePath->text().toStdString();

        if (   saveFilePath != Config::SaveFilePath
            || savestatePath != Config::SavestatePath
            || cheatFilePath != Config::CheatFilePath)
        {
            if (RunningSomething
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            Config::SaveFilePath = saveFilePath;
            Config::SavestatePath = savestatePath;
            Config::CheatFilePath = cheatFilePath;

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
                                                     QString::fromStdString(EmuDirectory));

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
                                                     QString::fromStdString(EmuDirectory));

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
                                                     QString::fromStdString(EmuDirectory));

    if (dir.isEmpty()) return;
    
    if (!QTemporaryFile(dir).open())
    {
        QMessageBox::critical(this, "melonDS", errordialog);
        return;
    }

    ui->txtCheatFilePath->setText(dir);
}
