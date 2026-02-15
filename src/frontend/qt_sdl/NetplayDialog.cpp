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
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>

#include "NetplayDialog.h"
#include "Config.h"
#include "main.h"
#include "Netplay.h"

#include "ui_NetplayStartHostDialog.h"
#include "ui_NetplayStartClientDialog.h"
#include "ui_NetplayDialog.h"

using namespace melonDS;


NetplayDialog* netplayDlg = nullptr;

#define netplay() ((Netplay&)MPInterface::Get())


NetplayStartHostDialog::NetplayStartHostDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayStartHostDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setMPInterface(MPInterface_Netplay);

    ui->txtPort->setText("8064");

    ui->txtPlayerName->setText("eye");
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

        netplay().StartHost(player.c_str(), port);
        netplayDlg = NetplayDialog::openDlg(parentWidget());
    }
    else
    {
        setMPInterface(MPInterface_Local);
    }

    QDialog::done(r);
}


NetplayStartClientDialog::NetplayStartClientDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayStartClientDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setMPInterface(MPInterface_Netplay);

    ui->txtPort->setText("8064");
    ui->txtPlayerName->setText("guest");
    ui->txtIPAddress->setText("localhost");
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

        netplay().StartClient(player.c_str(), host.c_str(), port);
        netplayDlg = NetplayDialog::openDlg(parentWidget());
    }
    else
    {
        setMPInterface(MPInterface_Local);
    }

    QDialog::done(r);
}

void NetplayDialog::on_spnInputBufferSize_valueChanged(int value)
{
    netplay().SetInputBufferSize(value);
}

NetplayDialog::NetplayDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);

    bool isHost = netplay().GetHostStatus();
    ui->btnStartGame->setVisible(isHost);
    ui->lblInputBufferSize->setVisible(isHost);
    ui->spnInputBufferSize->setVisible(isHost);

    EmuInstance *emuInstance = ((MainWindow*)parent)->getEmuInstance();
    netplay().RegisterInstance(emuInstance->getInstanceID(), emuInstance->getNDS());

    timerID = startTimer(1000);
}

NetplayDialog::~NetplayDialog()
{
    killTimer(timerID);

    delete ui;
}

void NetplayDialog::on_btnStartGame_clicked()
{
    netplay().StartGame();
}

void NetplayDialog::on_btnLeaveGame_clicked()
{
    done(QDialog::Accepted);
}

void NetplayDialog::done(int r)
{
    /*bool showwarning = true;
    if (netplay().GetNumPlayers() < 2)
        showwarning = false;

    if (showwarning)
    {
        if (QMessageBox::warning(this, "melonDS", "Really leave this Netplay game?",
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
            return;
    }*/

    netplay().EndSession();
    setMPInterface(MPInterface_Local);

    QDialog::done(r);
}

void NetplayDialog::timerEvent(QTimerEvent *event)
{
    doUpdatePlayerList();
}

void NetplayDialog::doUpdatePlayerList()
{
    auto playerlist = netplay().GetPlayerList();
    int numplayers = playerlist.size();
    auto maxplayers = netplay().GetMaxPlayers();

    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();

    model->clear();
    model->setRowCount(numplayers);

    // TODO: remove IP column in final product

    const QStringList header = {"#", "Player", "Status", "Ping", "IP"};
    model->setHorizontalHeaderLabels(header);

    int i = 0;
    for (const auto& player : playerlist)
    {
        QString id = QString("%0").arg(player.ID+1);
        model->setItem(i, 0, new QStandardItem(id));

        QString name = player.Name;
        model->setItem(i, 1, new QStandardItem(name));

        QString status;
        switch (player.Status)
        {
            case Netplay::Player_Client:     status = "Connected"; break;
            case Netplay::Player_Host:       status = "Host"; break;
            case Netplay::Player_Connecting: status = "Connecting"; break;
            default: status = "ded"; break;
        }
        model->setItem(i, 2, new QStandardItem(status));

        if (player.IsLocalPlayer)
        {
            model->setItem(i, 3, new QStandardItem("-"));
            model->setItem(i, 4, new QStandardItem("(local)"));
        }
        else
        {
            if (player.Status == Netplay::Player_Client ||
                player.Status == Netplay::Player_Host)
            {
                QString ping = QString("%0 ms").arg(player.Ping);
                model->setItem(i, 3, new QStandardItem(ping));
            }
            else
            {
                model->setItem(i, 3, new QStandardItem("-"));
            }

            char ip[32];
            u32 addr = player.Address;
            sprintf(ip, "%d.%d.%d.%d", addr&0xFF, (addr>>8)&0xFF, (addr>>16)&0xFF, addr>>24);
            model->setItem(i, 4, new QStandardItem(ip));
        }

        i++;
    }
}
