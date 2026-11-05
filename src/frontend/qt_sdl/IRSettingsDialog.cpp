/*
    Copyright 2016-2026 melonDS team

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
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "main.h"

#include "IRSettingsDialog.h"
#include "ui_IRSettingsDialog.h"

#include <QFileDialog>
IRSettingsDialog* IRSettingsDialog::currentDlg = nullptr;

bool IRSettingsDialog::needsReset = false;

using namespace melonDS::Platform;
using namespace melonDS;

IRSettingsDialog::IRSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::IRSettingsDialog)
{


    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();
    auto& cfg = emuInstance->getLocalConfig();

    IRMode = cfg.GetInt("IR.Mode");

    connect(ui->rbSerial, &QRadioButton::toggled, this, &IRSettingsDialog::toggleSerialSettings);
    connect(ui->rbTCP, &QRadioButton::toggled, this, &IRSettingsDialog::toggleNetworkSettings);
    connect(ui->rbENet, &QRadioButton::toggled, this, &IRSettingsDialog::toggleNetworkSettings);
    connect(ui->rb_NetworkServer, &QRadioButton::toggled, this, &IRSettingsDialog::toggleNetworkServer);
    connect(ui->rb_NetworkClient, &QRadioButton::toggled, this, &IRSettingsDialog::toggleNetworkClient);

    if (IRMode == 0) ui->rbCompat->setChecked(true);
    else if (IRMode == 1) ui->rbSerial->setChecked(true);
    else if (IRMode == 2) ui->rbTCP->setChecked(true);
    else if (IRMode == 3) ui->rbENet->setChecked(true);

    ui->groupBoxSerial->setEnabled(false);
    ui->groupBoxNetwork->setEnabled(false);

    ui->txtSerialPath->setText(cfg.GetQString("IR.SerialPortPath"));
    int readTimeout = cfg.GetInt("IR.Serial.ReadTimeoutUs");
    ui->boxReadTimeoutUs->setValue(readTimeout); // 500
    int sendDelay = cfg.GetInt("IR.Serial.SendDelayUs");
    ui->boxSendDelayUs->setValue(sendDelay);

    // Load TCP settings
    ui->boxHostIP->setText(cfg.GetQString("IR.Network.HostIP"));
    int selfPort = cfg.GetInt("IR.Network.SelfPort");
    int hostPort = cfg.GetInt("IR.Network.HostPort");
    ui->boxSelfPort->setValue(selfPort == 0 ? 8081 : selfPort);
    ui->txtHostPort->setValue(hostPort == 0 ? 8081 : hostPort);

    bool isServer = cfg.GetBool("IR.Network.IsServer");
    if (isServer) ui->rb_NetworkServer->setChecked(true);
    else ui->rb_NetworkClient->setChecked(true);

    toggleSerialSettings(ui->rbSerial->isChecked());
    toggleNetworkSettings(ui->rbTCP->isChecked() || ui->rbENet->isChecked());

    ui->lblSelfIP->setText(QString("0.0.0.0"));

}

void IRSettingsDialog::toggleSerialSettings(bool checked)
{
    ui->groupBoxSerial->setEnabled(checked);

}

void IRSettingsDialog::toggleNetworkSettings(bool checked)
{
    ui->groupBoxNetwork->setEnabled(checked);
}

void IRSettingsDialog::toggleNetworkServer(bool checked)
{
    //ui->groupBoxNetwork->setEnabled(checked);
}

void IRSettingsDialog::toggleNetworkClient(bool checked)
{
    //ui->groupBoxDirect->setEnabled(checked);
}

void IRSettingsDialog::on_EepromBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Select pokewalker EEPROM file", "", "bin files (*.bin);;Any file (*.*)");

    if (file.isEmpty()) return;

    if (!Platform::CheckFileWritable(file.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", "Unable to write to EEPROM file.\nPlease check file/folder write permissions.");
        return;
    }
}

IRSettingsDialog::~IRSettingsDialog()
{
    delete ui;
}

void IRSettingsDialog::done(int r)
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

        if (ui->rbCompat->isChecked()) IRMode = 0;
        if (ui->rbSerial->isChecked()) IRMode = 1;
        if (ui->rbTCP->isChecked()) IRMode = 2;
        if (ui->rbENet->isChecked()) IRMode = 3;

        auto& cfg = emuInstance->getLocalConfig();

        cfg.SetInt("IR.Mode", IRMode);
        cfg.SetQString("IR.SerialPortPath", ui->txtSerialPath->text());
        cfg.SetInt("IR.Serial.ReadTimeoutUs", ui->boxReadTimeoutUs->value());
        cfg.SetInt("IR.Serial.SendDelayUs", ui->boxSendDelayUs->value());

        // Save TCP settings
        cfg.SetQString("IR.Network.HostIP", ui->boxHostIP->text());
        cfg.SetInt("IR.Network.SelfPort", ui->boxSelfPort->value());
        cfg.SetInt("IR.Network.HostPort", ui->txtHostPort->value());
        cfg.SetBool("IR.Network.IsServer", ui->rb_NetworkServer->isChecked());
        Config::Save();
    }

    QDialog::done(r);

    closeDlg();
}