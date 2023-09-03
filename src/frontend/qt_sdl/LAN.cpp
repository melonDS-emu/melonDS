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
#include <stdlib.h>
#include <string.h>

#include <enet/enet.h>

#include <QStandardItemModel>

#include "LAN.h"
#include "Config.h"
#include "main.h"

#include "ui_LANStartHostDialog.h"
#include "ui_LANStartClientDialog.h"
#include "ui_LANDialog.h"


extern EmuThread* emuThread;
LANDialog* lanDlg;


LANStartHostDialog::LANStartHostDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartHostDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    //ui->txtPort->setText("8064");
}

LANStartHostDialog::~LANStartHostDialog()
{
    delete ui;
}

void LANStartHostDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        std::string player = ui->txtPlayerName->text().toStdString();
        //int port = ui->txtPort->text().toInt();

        // TODO validate input!!

        lanDlg = LANDialog::openDlg(parentWidget());

        //Netplay::StartHost(player.c_str(), port);
    }

    QDialog::done(r);
}


LANStartClientDialog::LANStartClientDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartClientDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    //ui->txtPort->setText("8064");
}

LANStartClientDialog::~LANStartClientDialog()
{
    delete ui;
}

void LANStartClientDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        std::string player = ui->txtPlayerName->text().toStdString();
        std::string host = ui->txtIPAddress->text().toStdString();
        //int port = ui->txtPort->text().toInt();

        // TODO validate input!!

        lanDlg = LANDialog::openDlg(parentWidget());

        //Netplay::StartClient(player.c_str(), host.c_str(), port);
    }

    QDialog::done(r);
}


LANDialog::LANDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);

    connect(this, &LANDialog::sgUpdatePlayerList, this, &LANDialog::doUpdatePlayerList);
}

LANDialog::~LANDialog()
{
    delete ui;
}

void LANDialog::done(int r)
{
    // ???

    QDialog::done(r);
}

void LANDialog::updatePlayerList(LAN::Player* players, int num)
{
    emit sgUpdatePlayerList(players, num);
}

void LANDialog::doUpdatePlayerList(LAN::Player* players, int num)
{
    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();

    model->clear();
    model->setRowCount(num);

    // TODO: remove IP column in final product

    const QStringList header = {"#", "Player", "Status", "Ping", "IP"};
    model->setHorizontalHeaderLabels(header);

    for (int i = 0; i < num; i++)
    {
        LAN::Player* player = &players[i];

        QString id = QString("%0").arg(player->ID+1);
        model->setItem(i, 0, new QStandardItem(id));

        QString name = player->Name;
        model->setItem(i, 1, new QStandardItem(name));

        QString status;
        switch (player->Status)
        {
        case 1: status = ""; break;
        case 2: status = "Host"; break;
        default: status = "ded"; break;
        }
        model->setItem(i, 2, new QStandardItem(status));

        // TODO: ping
        model->setItem(i, 3, new QStandardItem("x"));

        char ip[32];
        u32 addr = player->Address;
        sprintf(ip, "%d.%d.%d.%d", addr&0xFF, (addr>>8)&0xFF, (addr>>16)&0xFF, addr>>24);
        model->setItem(i, 4, new QStandardItem(ip));
    }
}


namespace LAN
{

//


bool Init()
{
    //

    return true;
}

void DeInit()
{
    //
}


void SetMPRecvTimeout(int timeout)
{
    //MPRecvTimeout = timeout;
}

void MPBegin()
{
    //
}

void MPEnd()
{
    //
}

void SetActive(bool active)
{
    //
}

u16 GetInstanceBitmask()
{
    //
    return 0;
}


int SendMPPacket(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int RecvMPPacket(u8* packet, u64* timestamp)
{
    return 0;
}


int SendMPCmd(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int SendMPReply(u8* packet, int len, u64 timestamp, u16 aid)
{
    return 0;
}

int SendMPAck(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int RecvMPHostPacket(u8* packet, u64* timestamp)
{
    return 0;
}

u16 RecvMPReplies(u8* packets, u64 timestamp, u16 aidmask)
{
    return 0;
}

}
