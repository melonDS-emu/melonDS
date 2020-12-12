/*
    Copyright 2016-2020 Arisotura, WaluigiWare64

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

#include <QMessageBox>
#include <QFileDialog>

#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "LogSettingsDialog.h"
#include "ui_LogSettingsDialog.h"

LogSettingsDialog* LogSettingsDialog::currentDlg = nullptr;

extern char* EmuDirectory;

LogSettingsDialog::LogSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LogSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->rbLogToFile->setChecked(Config::LogToFile != 0);
    ui->rbLogToConsole->setChecked(Config::LogToFile == 0);
    ui->txtLogFolderPath->setText(Config::LogFileLocation);
    
    updateControls();
}

LogSettingsDialog::~LogSettingsDialog()
{
    delete ui;
}

void LogSettingsDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        Config::LogToFile = ui->rbLogToFile->isChecked() ? 1:0;
        strncpy(Config::LogFileLocation, ui->txtLogFolderPath->text().toUtf8().constData(), 1023); Config::LogFileLocation[1023] = '\0';

        Config::Save();
    }

    QDialog::done(r);

    closeDlg();
}

void LogSettingsDialog::on_rbLogToFile_clicked()
{
    updateControls();
}

void LogSettingsDialog::on_rbLogToConsole_clicked()
{
    updateControls();
}

void LogSettingsDialog::on_btnLogFolderBrowse_clicked()
{
    QString file = QFileDialog::getSaveFileName(this,
                                                "Select Log File...",
                                                EmuDirectory,
                                                "Log files (*.log *.txt);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtLogFolderPath->setText(file);
}

void LogSettingsDialog::updateControls()
{
    bool logFileEnabled = ui->rbLogToFile->isChecked();
    ui->groupLogFile->setEnabled(logFileEnabled);
}
