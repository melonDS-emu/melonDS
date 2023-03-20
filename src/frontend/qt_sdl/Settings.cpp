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

#include <QMessageBox>

#include "Platform.h"
#include "Config.h"

#include "Settings.h"
#include "ui_Settings.h"

Settings* Settings::currentDlg = nullptr;

extern bool RunningSomething;

bool Settings::needsReset = false;

Settings::Settings(QWidget* parent) : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint), ui(new Ui::Settings)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->useExternalBIOSToggle->setChecked(Config::ExternalBIOSEnable);
    ui->arm9BIOSFileName->setText(QString::fromStdString(Config::BIOS9Path));
    ui->arm7BIOSFileName->setText(QString::fromStdString(Config::BIOS7Path));
    ui->dsFirmwareFileName->setText(QString::fromStdString(Config::FirmwarePath));

    ui->dsiARM9BIOSFileName->setText(QString::fromStdString(Config::DSiBIOS9Path));
    ui->dsiARM7BIOSFileName->setText(QString::fromStdString(Config::DSiBIOS7Path));
    ui->dsiFirmwareFileName->setText(QString::fromStdString(Config::DSiFirmwarePath));
    ui->dsiNANDFileName->setText(QString::fromStdString(Config::DSiNANDPath));

    ui->consoleTypeBox->setCurrentIndex(Config::ConsoleType);

    ui->bootCartridgesToggle->setChecked(Config::DirectBoot);
}

Settings::~Settings()
{
    delete ui;
}

bool Settings::verifyMAC()
{
    QString mac = ui->macAddressData->text();
    int maclen = mac.length();

    // blank MAC -> no MAC override
    if (maclen == 0)
    {
        return true;
    }

    // length should be 12 or 17 if separators are used
    if (maclen != 12 && maclen != 17)
    {
        return false;
    }

    bool hassep = maclen == 17;
    int pos = 0;
    for (int i = 0; i < maclen; i++)
    {
        QChar c = mac[i];
        if ((c <= '0' || c >= '9') && (c <= 'a' || c >= 'f') && (c <= 'A' && c >= 'F'))
        {
            return false;
        }

        pos++;
        if (pos >= 2)
        {
            pos = 0;

            if (hassep)
            {
                i++;
            }
        }
    }

    return true;
}

void Settings::done(int r)
{
    if (!verifyMAC())
    {

    }

    QDialog::done(r);

    closeDlg();
}

void Settings::birthdayMonthDayCorrection(int index)
{
    // prevent spurious changes
    if (ui->birthdayDayBox->count() < 12)
    {
        return;
    }

    // the DS firmware caps the birthday day depending on the birthday month
    // for February, the limit is 29
    const int ndays[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    int nday_old = ui->birthdayDayBox->count();
    int nday_cur = ndays[index];

    if (nday_cur > nday_old)
    {
        for (int i = nday_old+1; i <= nday_cur; i++)
        {
            ui->birthdayDayBox->insertItem(i-1, QString("%1").arg(i));
        }

        return;
    }
    else if (nday_cur < nday_old)
    {
        for (int i = nday_old; i >= nday_cur+1; i--)
        {
            ui->birthdayDayBox->removeItem(i - 1);
        }
    }
}

void Settings::onDSBIOS9Browse()
{

}

void Settings::onDSBIOS7Browse()
{

}

void Settings::onDSFirmwareBrowse()
{

}

void Settings::onDSiBIOS9Browse()
{

}

void Settings::onDSiBIOS7Browse()
{

}

void Settings::onDSiFirmwareBrowse()
{

}

void Settings::onDSiNANDBrowse()
{

}
