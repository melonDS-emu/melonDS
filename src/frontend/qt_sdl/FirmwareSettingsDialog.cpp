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

#include <QMessageBox>

#include "Platform.h"
#include "Config.h"
#include "main.h"

#include "FirmwareSettingsDialog.h"
#include "ui_FirmwareSettingsDialog.h"

using namespace melonDS::Platform;
namespace Platform = melonDS::Platform;

FirmwareSettingsDialog* FirmwareSettingsDialog::currentDlg = nullptr;

bool FirmwareSettingsDialog::needsReset = false;

FirmwareSettingsDialog::FirmwareSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::FirmwareSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto& cfg = emuInstance->getLocalConfig();
    auto firmcfg = cfg.GetTable("Firmware");

    ui->usernameEdit->setText(firmcfg.GetQString("Username"));

    ui->languageBox->addItems(languages);
    ui->languageBox->setCurrentIndex(firmcfg.GetInt("Language"));

    for (int i = 1; i <= 31; i++)
    {
        ui->cbxBirthdayDay->addItem(QString("%1").arg(i));
    }

    ui->cbxBirthdayMonth->addItems(months);
    ui->cbxBirthdayMonth->setCurrentIndex(firmcfg.GetInt("BirthdayMonth") - 1);

    ui->cbxBirthdayDay->setCurrentIndex(firmcfg.GetInt("BirthdayDay") - 1);

    for (int i = 0; i < 16; i++)
    {
        QImage image(16, 16, QImage::Format_ARGB32);
        image.fill(colors[i]);
        QIcon icon(QPixmap::fromImage(image.copy()));
        ui->colorsEdit->addItem(icon, colornames[i]);
    }
    ui->colorsEdit->setCurrentIndex(firmcfg.GetInt("FavouriteColour"));

    ui->messageEdit->setText(firmcfg.GetQString("Message"));

    ui->overrideFirmwareBox->setChecked(firmcfg.GetBool("OverrideSettings"));

    ui->txtMAC->setText(firmcfg.GetQString("MAC"));

    int inst = emuInstance->getInstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(QString("Configuring settings for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();

#define SET_ORIGVAL(type, val) \
    for (type* w : findChildren<type*>(nullptr)) \
        w->setProperty("user_originalValue", w->val());

    SET_ORIGVAL(QLineEdit, text);
    SET_ORIGVAL(QComboBox, currentIndex);
    SET_ORIGVAL(QCheckBox, isChecked);

#undef SET_ORIGVAL
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
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    needsReset = false;

    if (r == QDialog::Accepted)
    {
        if (!verifyMAC())
        {
            QMessageBox::critical(this, "Invalid MAC address",
                                  "The MAC address you entered isn't valid. It should contain 6 pairs of hexadecimal digits, optionally separated.");

            return;
        }

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
        CHECK_ORIGVAL(QCheckBox, isChecked);

#undef CHECK_ORIGVAL

        if (modified)
        {
            if (emuInstance->emuIsActive()
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;

            auto& cfg = emuInstance->getLocalConfig();
            auto firmcfg = cfg.GetTable("Firmware");

            firmcfg.SetBool("OverrideSettings", ui->overrideFirmwareBox->isChecked());

            firmcfg.SetQString("Username", ui->usernameEdit->text());
            firmcfg.SetInt("Language", ui->languageBox->currentIndex());
            firmcfg.SetInt("FavouriteColour", ui->colorsEdit->currentIndex());
            firmcfg.SetInt("BirthdayDay", ui->cbxBirthdayDay->currentIndex() + 1);
            firmcfg.SetInt("BirthdayMonth", ui->cbxBirthdayMonth->currentIndex() + 1);
            firmcfg.SetQString("Message", ui->messageEdit->text());

            firmcfg.SetQString("MAC", ui->txtMAC->text());

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
