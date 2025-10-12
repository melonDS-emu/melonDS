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

#include "types.h"
#include "Config.h"
#include "main.h"

#include "DateTimeDialog.h"
#include "ui_DateTimeDialog.h"

DateTimeDialog* DateTimeDialog::currentDlg = nullptr;


DateTimeDialog::DateTimeDialog(QWidget* parent) : QDialog(parent), ui(new Ui::DateTimeDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto& cfg = emuInstance->getLocalConfig();
    QDateTime now = QDateTime::currentDateTime();
    customTime = now.addSecs(cfg.GetInt64("RTC.Offset"));

    ui->chkChangeTime->setChecked(false);
    ui->chkResetTime->setChecked(false);

    ui->lblCustomTime->setText(customTime.toString(ui->txtNewCustomTime->displayFormat()));
    startTimer(1000);

    ui->txtNewCustomTime->setEnabled(ui->chkChangeTime->isChecked());
}

DateTimeDialog::~DateTimeDialog()
{
    delete ui;
}

void DateTimeDialog::timerEvent(QTimerEvent* event)
{
    customTime = customTime.addSecs(1);
    ui->lblCustomTime->setText(customTime.toString(ui->txtNewCustomTime->displayFormat()));
}

void DateTimeDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        auto& cfg = emuInstance->getLocalConfig();

        if (ui->chkChangeTime->isChecked())
        {
            QDateTime now = QDateTime::currentDateTime();
            cfg.SetInt64("RTC.Offset", now.secsTo(ui->txtNewCustomTime->dateTime()));
        }
        else if (ui->chkResetTime->isChecked())
            cfg.SetInt64("RTC.Offset", 0);

        Config::Save();
    }

    QDialog::done(r);

    closeDlg();
}

void DateTimeDialog::on_chkChangeTime_clicked(bool checked)
{
    if (checked) ui->chkResetTime->setChecked(false);
    ui->txtNewCustomTime->setEnabled(checked);
}

void DateTimeDialog::on_chkResetTime_clicked(bool checked)
{
    if (checked)
    {
        ui->chkChangeTime->setChecked(false);
        ui->txtNewCustomTime->setEnabled(false);
    }
}
