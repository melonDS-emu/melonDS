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

#include <QMessageBox>

#include "Platform.h"
#include "Config.h"

#include "FirmwareSettingsDialog.h"
#include "ui_FirmwareSettingsDialog.h"

using namespace melonDS::Platform;
namespace Platform = melonDS::Platform;

FirmwareSettingsDialog* FirmwareSettingsDialog::currentDlg = nullptr;

extern bool RunningSomething;

bool FirmwareSettingsDialog::needsReset = false;

FirmwareSettingsDialog::FirmwareSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::FirmwareSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->usernameEdit->setText(QString::fromStdString(Config::FirmwareUsername));

    ui->languageBox->addItems(languages);
    ui->languageBox->setCurrentIndex(Config::FirmwareLanguage);

    for (int i = 1; i <= 31; i++)
    {
        ui->cbxBirthdayDay->addItem(QString("%1").arg(i));
    }

    ui->cbxBirthdayMonth->addItems(months);
    ui->cbxBirthdayMonth->setCurrentIndex(Config::FirmwareBirthdayMonth - 1);

    ui->cbxBirthdayDay->setCurrentIndex(Config::FirmwareBirthdayDay - 1);

    for (int i = 0; i < 16; i++)
    {
        QImage image(16, 16, QImage::Format_ARGB32);
        image.fill(colors[i]);
        QIcon icon(QPixmap::fromImage(image.copy()));
        ui->colorsEdit->addItem(icon, colornames[i]);
    }
    ui->colorsEdit->setCurrentIndex(Config::FirmwareFavouriteColour);

    ui->messageEdit->setText(QString::fromStdString(Config::FirmwareMessage));

    ui->overrideFirmwareBox->setChecked(Config::FirmwareOverrideSettings);

    ui->txtMAC->setText(QString::fromStdString(Config::FirmwareMAC));

    on_overrideFirmwareBox_toggled();

    int inst = Platform::InstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(QString("Configuring settings for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();
}

FirmwareSettingsDialog::~FirmwareSettingsDialog()
{
    delete ui;
}

bool FirmwareSettingsDialog::verifyMAC()
{
    QString mac = ui->txtMAC->text();
    int maclen = mac.length();

    // blank MAC = no MAC override
    if (maclen == 0)
        return true;

    // length should be 12 or 17 if separators are used
    if (maclen != 12 && maclen != 17)
        return false;

    bool hassep = maclen==17;
    int pos = 0;
    for (int i = 0; i < maclen;)
    {
        QChar c = mac[i];
        bool good = false;
        if      (c >= '0' && c <= '9') good = true;
        else if (c >= 'a' && c <= 'f') good = true;
        else if (c >= 'A' && c <= 'F') good = true;
        if (!good) return false;

        i++;
        pos++;
        if (pos >= 2)
        {
            pos = 0;
            if (hassep) i++;
        }
    }

    return true;
}

void FirmwareSettingsDialog::done(int r)
{
    needsReset = false;

    if (r == QDialog::Accepted)
    {
        if (!verifyMAC())
        {
            QMessageBox::critical(this, "Invalid MAC address",
                                  "The MAC address you entered isn't valid. It should contain 6 pairs of hexadecimal digits, optionally separated.");

            return;
        }

        bool newOverride = ui->overrideFirmwareBox->isChecked();

        std::string newName = ui->usernameEdit->text().toStdString();
        int newLanguage = ui->languageBox->currentIndex();
        int newFavColor = ui->colorsEdit->currentIndex();
        int newBirthdayDay = ui->cbxBirthdayDay->currentIndex() + 1;
        int newBirthdayMonth = ui->cbxBirthdayMonth->currentIndex() + 1;
        std::string newMessage = ui->messageEdit->text().toStdString();

        std::string newMAC = ui->txtMAC->text().toStdString();

        if (   newOverride != Config::FirmwareOverrideSettings
            || newName != Config::FirmwareUsername
            || newLanguage != Config::FirmwareLanguage
            || newFavColor != Config::FirmwareFavouriteColour
            || newBirthdayDay != Config::FirmwareBirthdayDay
            || newBirthdayMonth != Config::FirmwareBirthdayMonth
            || newMessage != Config::FirmwareMessage
            || newMAC != Config::FirmwareMAC)
        {
            if (RunningSomething
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            Config::FirmwareOverrideSettings = newOverride;

            Config::FirmwareUsername = newName;
            Config::FirmwareLanguage = newLanguage;
            Config::FirmwareFavouriteColour = newFavColor;
            Config::FirmwareBirthdayDay = newBirthdayDay;
            Config::FirmwareBirthdayMonth = newBirthdayMonth;
            Config::FirmwareMessage = newMessage;

            Config::FirmwareMAC = newMAC;

            Config::Save();

            needsReset = true;
        }
    }

    QDialog::done(r);

    closeDlg();
}

void FirmwareSettingsDialog::on_cbxBirthdayMonth_currentIndexChanged(int idx)
{
    // prevent spurious changes
    if (ui->cbxBirthdayMonth->count() < 12) return;

    // the DS firmware caps the birthday day depending on the birthday month
    // for February, the limit is 29
    const int ndays[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    int nday_old = ui->cbxBirthdayDay->count();
    int nday_cur = ndays[idx];

    if (nday_cur > nday_old)
    {
        for (int i = nday_old+1; i <= nday_cur; i++)
        {
            ui->cbxBirthdayDay->insertItem(i-1, QString("%1").arg(i));
        }
    }
    else if (nday_cur < nday_old)
    {
        for (int i = nday_old; i >= nday_cur+1; i--)
        {
            ui->cbxBirthdayDay->removeItem(i-1);
        }
    }
}

void FirmwareSettingsDialog::on_overrideFirmwareBox_toggled()
{
    bool disable = !ui->overrideFirmwareBox->isChecked();
    ui->grpUserSettings->setDisabled(disable);
    ui->grpWifiSettings->setDisabled(disable);
}
