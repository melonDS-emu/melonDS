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

    QDate birthDate = QDate(QDate::currentDate().year(), Config::FirmwareBirthdayMonth, Config::FirmwareBirthdayDay);
    ui->birthdayEdit->setDate(birthDate);

    ui->colorsEdit->addItems(colours);
    ui->colorsEdit->setCurrentIndex(Config::FirmwareFavouriteColour);

    ui->messageEdit->setText(Config::FirmwareMessage);

    ui->overrideFirmwareBox->setChecked(Config::FirmwareOverrideSettings);
}

FirmwareSettingsDialog::~FirmwareSettingsDialog()
{
    delete ui;
}

void FirmwareSettingsDialog::on_dialogButtons_accepted()
{
    std::string newName = ui->usernameEdit->text().toStdString();
    strncpy(Config::FirmwareUsername, newName.c_str(), 63); Config::FirmwareUsername[63] = '\0';

    Config::FirmwareLanguage = ui->languageBox->currentIndex();
    Config::FirmwareFavouriteColour = ui->colorsEdit->currentIndex();
    Config::FirmwareBirthdayDay = ui->birthdayEdit->date().day();
    Config::FirmwareBirthdayMonth = ui->birthdayEdit->date().month();
    Config::FirmwareOverrideSettings = ui->overrideFirmwareBox->isChecked();

    std::string newMessage = ui->messageEdit->text().toStdString();
    strncpy(Config::FirmwareMessage, newMessage.c_str(), 1023); Config::FirmwareMessage[1023] = '\0';
    Config::Save();

    closeDlg();
}

void FirmwareSettingsDialog::on_dialogButtons_rejected()
{
    closeDlg();
}
