/*
    Copyright 2016-2024 melonDS team

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

#include "Net.h"
#include "Net_PCap.h"

#include "WifiSettingsDialog.h"
#include "ui_WifiSettingsDialog.h"


#ifdef __WIN32__
#define PCAP_NAME "winpcap/npcap"
#else
#define PCAP_NAME "libpcap"
#endif


WifiSettingsDialog* WifiSettingsDialog::currentDlg = nullptr;

bool WifiSettingsDialog::needsReset = false;


WifiSettingsDialog::WifiSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::WifiSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();
    auto& cfg = emuInstance->getGlobalConfig();

    Net::DeInit();
    haspcap = Net_PCap::InitAdapterList();

    ui->rbDirectMode->setText("Direct mode (requires " PCAP_NAME " and ethernet connection)");

    ui->lblAdapterMAC->setText("(none)");
    ui->lblAdapterIP->setText("(none)");

    int sel = 0;
    for (int i = 0; i < Net_PCap::NumAdapters; i++)
    {
        Net_PCap::AdapterData* adapter = &Net_PCap::Adapters[i];

        ui->cbxDirectAdapter->addItem(QString(adapter->FriendlyName));

        if (!strncmp(adapter->DeviceName, cfg.GetString("LAN.Device").c_str(), 128))
            sel = i;
    }
    ui->cbxDirectAdapter->setCurrentIndex(sel);

    // errrr???
    bool direct = cfg.GetBool("LAN.DirectMode");
    ui->rbDirectMode->setChecked(direct);
    ui->rbIndirectMode->setChecked(!direct);
    if (!haspcap) ui->rbDirectMode->setEnabled(false);

    updateAdapterControls();
}

WifiSettingsDialog::~WifiSettingsDialog()
{
    delete ui;
}

void WifiSettingsDialog::done(int r)
{
    needsReset = false;

    if (r == QDialog::Accepted)
    {
        auto& cfg = emuInstance->getGlobalConfig();

        cfg.SetBool("LAN.DirectMode", ui->rbDirectMode->isChecked());

        int sel = ui->cbxDirectAdapter->currentIndex();
        if (sel < 0 || sel >= Net_PCap::NumAdapters) sel = 0;
        if (Net_PCap::NumAdapters < 1)
        {
            cfg.SetString("LAN.Device", "");
        }
        else
        {
            cfg.SetString("LAN.Device", Net_PCap::Adapters[sel].DeviceName);
        }

        Config::Save();
    }

    Net_PCap::DeInit();
    Config::Table cfg = Config::GetGlobalTable();
    bool direct = cfg.GetBool("LAN.DirectMode");
    std::string devicename = cfg.GetString("LAN.Device");
    Net::Init(direct, devicename.c_str());

    QDialog::done(r);

    closeDlg();
}

void WifiSettingsDialog::on_rbDirectMode_clicked()
{
    updateAdapterControls();
}

void WifiSettingsDialog::on_rbIndirectMode_clicked()
{
    updateAdapterControls();
}

void WifiSettingsDialog::on_cbxDirectAdapter_currentIndexChanged(int sel)
{
    if (!haspcap) return;

    if (sel < 0 || sel >= Net_PCap::NumAdapters) return;
    if (Net_PCap::NumAdapters < 1) return;

    Net_PCap::AdapterData* adapter = &Net_PCap::Adapters[sel];
    char tmp[64];

    sprintf(tmp, "%02X:%02X:%02X:%02X:%02X:%02X",
            adapter->MAC[0], adapter->MAC[1], adapter->MAC[2],
            adapter->MAC[3], adapter->MAC[4], adapter->MAC[5]);
    ui->lblAdapterMAC->setText(QString(tmp));

    sprintf(tmp, "%d.%d.%d.%d",
            adapter->IP_v4[0], adapter->IP_v4[1],
            adapter->IP_v4[2], adapter->IP_v4[3]);
    ui->lblAdapterIP->setText(QString(tmp));
}

void WifiSettingsDialog::updateAdapterControls()
{
    bool enable = haspcap && ui->rbDirectMode->isChecked();

    ui->cbxDirectAdapter->setEnabled(enable);
    ui->lblAdapterMAC->setEnabled(enable);
    ui->lblAdapterIP->setEnabled(enable);
}
