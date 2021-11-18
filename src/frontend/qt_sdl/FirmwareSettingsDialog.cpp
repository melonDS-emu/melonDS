/*
    Copyright 2016-2020 Arisotura

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

#include "Config.h"
#include "FirmwareSettingsDialog.h"
#include "ui_FirmwareSettingsDialog.h"


FirmwareSettingsDialog* FirmwareSettingsDialog::currentDlg = nullptr;

FirmwareSettingsDialog::FirmwareSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::FirmwareSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->usernameEdit->setText(Config::FirmwareUsername);

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

    ui->messageEdit->setText(Config::FirmwareMessage);

    ui->overrideFirmwareBox->setChecked(Config::FirmwareOverrideSettings);
}

FirmwareSettingsDialog::~FirmwareSettingsDialog()
{
    delete ui;
}

void FirmwareSettingsDialog::on_FirmwareSettingsDialog_accepted()
{
    std::string newName = ui->usernameEdit->text().toStdString();
    strncpy(Config::FirmwareUsername, newName.c_str(), 63); Config::FirmwareUsername[63] = '\0';

    Config::FirmwareLanguage = ui->languageBox->currentIndex();
    Config::FirmwareFavouriteColour = ui->colorsEdit->currentIndex();
    Config::FirmwareBirthdayDay = ui->cbxBirthdayDay->currentIndex() + 1;
    Config::FirmwareBirthdayMonth = ui->cbxBirthdayMonth->currentIndex() + 1;
    Config::FirmwareOverrideSettings = ui->overrideFirmwareBox->isChecked();

    std::string newMessage = ui->messageEdit->text().toStdString();
    strncpy(Config::FirmwareMessage, newMessage.c_str(), 1023); Config::FirmwareMessage[1023] = '\0';
    Config::Save();

    closeDlg();
}

void FirmwareSettingsDialog::on_FirmwareSettingsDialog_rejected()
{
    closeDlg();
}

void FirmwareSettingsDialog::on_cbxBirthdayMonth_currentIndexChanged(int idx)
{
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
