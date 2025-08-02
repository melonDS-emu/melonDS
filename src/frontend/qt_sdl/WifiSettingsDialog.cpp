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

extern std::optional<melonDS::LibPCap> pcap;
extern melonDS::Net net;

WifiSettingsDialog* WifiSettingsDialog::currentDlg = nullptr;

bool WifiSettingsDialog::needsReset = false;

void NetInit();

WifiSettingsDialog::WifiSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::WifiSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();
    auto& cfg = emuInstance->getGlobalConfig();

    if (!pcap)
        pcap = melonDS::LibPCap::New();

    haspcap = pcap.has_value();
    if (pcap)
        adapters = pcap->GetAdapters();

    ui->rbDirectMode->setText("Direct mode (requires " PCAP_NAME " and ethernet connection)");

    ui->lblAdapterMAC->setText("(none)");
    ui->lblAdapterIP->setText("(none)");

    int sel = 0;
    for (int i = 0; i < adapters.size(); i++)
    {
        melonDS::AdapterData& adapter = adapters[i];

        ui->cbxDirectAdapter->addItem(QString(adapter.FriendlyName));

        if (!strncmp(adapter.DeviceName, cfg.GetString("LAN.Device").c_str(), 128))
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
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    needsReset = false;

    if (r == QDialog::Accepted)
    {
        auto& cfg = emuInstance->getGlobalConfig();

        cfg.SetBool("LAN.DirectMode", ui->rbDirectMode->isChecked());

        int sel = ui->cbxDirectAdapter->currentIndex();
        if (sel < 0 || sel >= adapters.size()) sel = 0;
        if (adapters.empty())
        {
            cfg.SetString("LAN.Device", "");
        }
        else
        {
            cfg.SetString("LAN.Device", adapters[sel].DeviceName);
        }

        Config::Save();
    }

    Config::Table cfg = Config::GetGlobalTable();
    std::string devicename = cfg.GetString("LAN.Device");

    NetInit();

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

    if (sel < 0 || sel >= adapters.size() || adapters.empty()) return;

    melonDS::AdapterData* adapter = &adapters[sel];
    char tmp[64];

    snprintf(tmp, sizeof(tmp), "%02X:%02X:%02X:%02X:%02X:%02X",
             adapter->MAC[0], adapter->MAC[1], adapter->MAC[2],
             adapter->MAC[3], adapter->MAC[4], adapter->MAC[5]);
    ui->lblAdapterMAC->setText(QString(tmp));

    snprintf(tmp, sizeof(tmp), "%d.%d.%d.%d",
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
