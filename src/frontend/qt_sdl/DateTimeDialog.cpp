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

#include "types.h"
#include "Config.h"

#include "DateTimeDialog.h"
#include "ui_DateTimeDialog.h"

DateTimeDialog* DateTimeDialog::currentDlg = nullptr;


DateTimeDialog::DateTimeDialog(QWidget* parent) : QDialog(parent), ui(new Ui::DateTimeDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    grpTimeMode = new QButtonGroup(this);
    grpTimeMode->addButton(ui->rbSystemTime, 0);
    grpTimeMode->addButton(ui->rbCustomTime, 1);
    connect(grpTimeMode, SIGNAL(buttonClicked(int)), this, SLOT(onChangeTimeMode(int)));
    grpTimeMode->button(Config::RTCMode)->setChecked(true);

    QDateTime now = QDateTime::currentDateTime();
    customTime = now;

    ui->chkChangeTime->setChecked(false);

    if (Config::RTCNewTime != "")
    {
        QDateTime newtime = QDateTime::fromString(QString::fromStdString(Config::RTCNewTime), Qt::ISODate);

        if (newtime.isValid())
        {
            ui->chkChangeTime->setChecked(true);
            ui->txtNewCustomTime->setDateTime(newtime);
        }
    }

    if (Config::RTCLastTime != "" && Config::RTCLastHostTime != "")
    {
        QDateTime lasttime = QDateTime::fromString(QString::fromStdString(Config::RTCLastTime), Qt::ISODate);
        QDateTime lasthost = QDateTime::fromString(QString::fromStdString(Config::RTCLastHostTime), Qt::ISODate);

        if (lasttime.isValid() && lasthost.isValid())
        {
            qint64 offset = lasthost.secsTo(now);
            customTime = lasttime.addSecs(offset);
        }
    }

    ui->lblCustomTime->setText(customTime.toString(ui->txtNewCustomTime->displayFormat()));
    startTimer(1000);

    bool iscustom = (Config::RTCMode == 1);
    ui->chkChangeTime->setEnabled(iscustom);
    ui->txtNewCustomTime->setEnabled(iscustom && ui->chkChangeTime->isChecked());
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
        Config::RTCMode = grpTimeMode->checkedId();

        if (ui->chkChangeTime->isChecked())
            Config::RTCNewTime = ui->txtNewCustomTime->dateTime().toString(Qt::ISODate).toStdString();
        else
            Config::RTCNewTime = "";

        Config::Save();
    }

    QDialog::done(r);

    closeDlg();
}

void DateTimeDialog::on_chkChangeTime_clicked(bool checked)
{
    bool iscustom = (grpTimeMode->checkedId() == 1);

    ui->txtNewCustomTime->setEnabled(iscustom && checked);
}

void DateTimeDialog::onChangeTimeMode(int mode)
{
    bool iscustom = (mode == 1);

    ui->chkChangeTime->setEnabled(iscustom);
    ui->txtNewCustomTime->setEnabled(iscustom && ui->chkChangeTime->isChecked());
}

void setCustomTimeLabel()
{
    //
}

