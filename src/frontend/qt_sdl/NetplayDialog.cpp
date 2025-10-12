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
#include <stdlib.h>
#include <string.h>
#include <queue>

#include <enet/enet.h>

#include <QStandardItemModel>
#include <QProcess>

#include "NDS.h"
#include "NDSCart.h"
#include "main.h"
//#include "IPC.h"
#include "NetplayDialog.h"
//#include "Input.h"
//#include "ROMManager.h"
#include "Config.h"
#include "Savestate.h"
#include "Platform.h"

#include "ui_NetplayStartHostDialog.h"
#include "ui_NetplayStartClientDialog.h"
#include "ui_NetplayDialog.h"

using namespace melonDS;


extern EmuThread* emuThread;
NetplayDialog* netplayDlg;


NetplayStartHostDialog::NetplayStartHostDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayStartHostDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->txtPort->setText("8064");
}

NetplayStartHostDialog::~NetplayStartHostDialog()
{
    delete ui;
}

void NetplayStartHostDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        return;
    }

    if (r == QDialog::Accepted)
    {
        std::string player = ui->txtPlayerName->text().toStdString();
        int port = ui->txtPort->text().toInt();

        // TODO validate input!!

        netplayDlg = NetplayDialog::openDlg(parentWidget());

        Netplay::StartHost(player.c_str(), port);
    }

    QDialog::done(r);
}


NetplayStartClientDialog::NetplayStartClientDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayStartClientDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->txtPort->setText("8064");
}

NetplayStartClientDialog::~NetplayStartClientDialog()
{
    delete ui;
}

void NetplayStartClientDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        return;
    }

    if (r == QDialog::Accepted)
    {
        std::string player = ui->txtPlayerName->text().toStdString();
        std::string host = ui->txtIPAddress->text().toStdString();
        int port = ui->txtPort->text().toInt();

        // TODO validate input!!

        netplayDlg = NetplayDialog::openDlg(parentWidget());

        Netplay::StartClient(player.c_str(), host.c_str(), port);
    }

    QDialog::done(r);
}


NetplayDialog::NetplayDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);

    connect(this, &NetplayDialog::sgUpdatePlayerList, this, &NetplayDialog::doUpdatePlayerList);
}

NetplayDialog::~NetplayDialog()
{
    delete ui;
}

void NetplayDialog::done(int r)
{
    // ???

    QDialog::done(r);
}

void NetplayDialog::updatePlayerList(Netplay::Player* players, int num)
{
    emit sgUpdatePlayerList(players, num);
}

void NetplayDialog::doUpdatePlayerList(Netplay::Player* players, int num)
{
    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();

    model->clear();
    model->setRowCount(num);

    // TODO: remove IP column in final product

    const QStringList header = {"#", "Player", "Status", "Ping", "IP"};
    model->setHorizontalHeaderLabels(header);

    for (int i = 0; i < num; i++)
    {
        Netplay::Player* player = &players[i];

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
        snprintf(ip, sizeof(ip), "%d.%d.%d.%d", addr&0xFF, (addr>>8)&0xFF, (addr>>16)&0xFF, addr>>24);
        model->setItem(i, 4, new QStandardItem(ip));
    }
}
