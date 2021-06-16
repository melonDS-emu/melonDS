/*
    Copyright 2016-2021 Arisotura

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
#include "PlatformConfig.h"

#include "LAN_Socket.h"
#include "LAN_PCap.h"
#include "Wifi.h"

#include "WifiSettingsDialog.h"
#include "ui_WifiSettingsDialog.h"


#ifdef __WIN32__
#define PCAP_NAME "winpcap/npcap"
#else
#define PCAP_NAME "libpcap"
#endif


WifiSettingsDialog* WifiSettingsDialog::currentDlg = nullptr;

bool WifiSettingsDialog::needsReset = false;

extern bool RunningSomething;


WifiSettingsDialog::WifiSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::WifiSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

#ifdef HAVE_LIBSLIRP
    LAN_Socket::Init();
#endif
    haspcap = LAN_PCap::Init(false);

    ui->rbDirectMode->setText("Direct mode (requires " PCAP_NAME " and ethernet connection)");

    ui->cbBindAnyAddr->setChecked(Config::SocketBindAnyAddr != 0);
    ui->cbRandomizeMAC->setChecked(Config::RandomizeMAC != 0);

    int sel = 0;
    for (int i = 0; i < LAN_PCap::NumAdapters; i++)
    {
        LAN_PCap::AdapterData* adapter = &LAN_PCap::Adapters[i];

        ui->cbxDirectAdapter->addItem(QString(adapter->FriendlyName));

        if (!strncmp(adapter->DeviceName, Config::LANDevice, 128))
            sel = i;
    }
    ui->cbxDirectAdapter->setCurrentIndex(sel);

#ifdef HAVE_LIBSLIRP
    ui->rbDirectMode->setChecked(Config::DirectLAN != 0);
    ui->rbIndirectMode->setChecked(Config::DirectLAN == 0);
#else
    ui->rbDirectMode->setChecked(true);
    ui->rbIndirectMode->setEnabled(false);
    ui->rbIndirectMode->setText("Indirect mode (unavailable, compiled without libslirp)");
#endif
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
        int randommac = ui->cbRandomizeMAC->isChecked() ? 1:0;

        if (randommac != Config::RandomizeMAC)
        {
            if (RunningSomething
                && QMessageBox::warning(this, "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
                return;
        }

        Config::SocketBindAnyAddr = ui->cbBindAnyAddr->isChecked() ? 1:0;
        Config::RandomizeMAC = randommac;
        Config::DirectLAN = ui->rbDirectMode->isChecked() ? 1:0;

        int sel = ui->cbxDirectAdapter->currentIndex();
        if (sel < 0 || sel >= LAN_PCap::NumAdapters) sel = 0;
        if (LAN_PCap::NumAdapters < 1)
        {
            Config::LANDevice[0] = '\0';
        }
        else
        {
            strncpy(Config::LANDevice, LAN_PCap::Adapters[sel].DeviceName, 127);
            Config::LANDevice[127] = '\0';
        }

        Config::Save();

        needsReset = true;
    }

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

    if (sel < 0 || sel >= LAN_PCap::NumAdapters) return;
    if (LAN_PCap::NumAdapters < 1) return;

    LAN_PCap::AdapterData* adapter = &LAN_PCap::Adapters[sel];
    char tmp[64];

    sprintf(tmp, "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
            adapter->MAC[0], adapter->MAC[1], adapter->MAC[2],
            adapter->MAC[3], adapter->MAC[4], adapter->MAC[5]);
    ui->lblAdapterMAC->setText(QString(tmp));

    sprintf(tmp, "IP: %d.%d.%d.%d",
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
