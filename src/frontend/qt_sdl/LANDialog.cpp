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
#include <string.h>
#include <queue>
#include <vector>

#include <QStandardItemModel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>

#include "LANDialog.h"
#include "Config.h"
#include "main.h"

#include "ui_LANStartHostDialog.h"
#include "ui_LANStartClientDialog.h"
#include "ui_LANDialog.h"

using namespace melonDS;


extern EmuThread* emuThread;
LANStartClientDialog* lanClientDlg = nullptr;
LANDialog* lanDlg = nullptr;


LANStartHostDialog::LANStartHostDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartHostDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    // TODO: remember the last setting? so this doesn't suck massively
    // we could also remember the player name (and auto-init it from the firmware name or whatever)
    ui->sbNumPlayers->setRange(2, 16);
    ui->sbNumPlayers->setValue(16);
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
        int numplayers = ui->sbNumPlayers->value();

        // TODO validate input!!

        lanDlg = LANDialog::openDlg(parentWidget());

        LAN::StartHost(player.c_str(), numplayers);
    }

    QDialog::done(r);
}


LANStartClientDialog::LANStartClientDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartClientDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvAvailableGames->setModel(model);
    const QStringList listheader = {"Name", "Players", "Status", "Host IP"};
    model->setHorizontalHeaderLabels(listheader);

    connect(ui->tvAvailableGames->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(onGameSelectionChanged(const QItemSelection&, const QItemSelection&)));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Connect");
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    QPushButton* btn = ui->buttonBox->addButton("Direct connect...", QDialogButtonBox::ActionRole);
    connect(btn, SIGNAL(clicked()), this, SLOT(onDirectConnect()));

    connect(this, &LANStartClientDialog::sgUpdateDiscoveryList, this, &LANStartClientDialog::doUpdateDiscoveryList);

    lanClientDlg = this;
    LAN::StartDiscovery();
}

LANStartClientDialog::~LANStartClientDialog()
{
    lanClientDlg = nullptr;
    delete ui;
}

void LANStartClientDialog::onGameSelectionChanged(const QItemSelection& cur, const QItemSelection& prev)
{
    QModelIndexList indlist = cur.indexes();
    if (indlist.count() == 0)
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}

void LANStartClientDialog::on_tvAvailableGames_doubleClicked(QModelIndex index)
{
    done(QDialog::Accepted);
}

void LANStartClientDialog::onDirectConnect()
{
    if (ui->txtPlayerName->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, "melonDS", "Please enter a player name before connecting.");
        return;
    }

    QString host = QInputDialog::getText(this, "Direct connect", "Host address:");
    if (host.isEmpty()) return;

    std::string hostname = host.toStdString();
    std::string player = ui->txtPlayerName->text().toStdString();

    setEnabled(false);
    LAN::EndDiscovery();
    if (!LAN::StartClient(player.c_str(), hostname.c_str()))
    {
        QString msg = QString("Failed to connect to the host %0.").arg(QString::fromStdString(hostname));
        QMessageBox::warning(this, "melonDS", msg);
        setEnabled(true);
        LAN::StartDiscovery();
        return;
    }

    setEnabled(true);
    lanDlg = LANDialog::openDlg(parentWidget());
    QDialog::done(QDialog::Accepted);
}

void LANStartClientDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        if (ui->txtPlayerName->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, "melonDS", "Please enter a player name before connecting.");
            return;
        }

        QModelIndexList indlist = ui->tvAvailableGames->selectionModel()->selectedRows();
        if (indlist.count() == 0) return;

        QStandardItemModel* model = (QStandardItemModel*)ui->tvAvailableGames->model();
        QStandardItem* item = model->item(indlist[0].row());
        u32 addr = item->data().toUInt();
        char hostname[16];
        snprintf(hostname, 16, "%d.%d.%d.%d", (addr>>24), ((addr>>16)&0xFF), ((addr>>8)&0xFF), (addr&0xFF));

        std::string player = ui->txtPlayerName->text().toStdString();

        setEnabled(false);
        LAN::EndDiscovery();
        if (!LAN::StartClient(player.c_str(), hostname))
        {
            QString msg = QString("Failed to connect to the host %0.").arg(QString(hostname));
            QMessageBox::warning(this, "melonDS", msg);
            setEnabled(true);
            LAN::StartDiscovery();
            return;
        }

        setEnabled(true);
        lanDlg = LANDialog::openDlg(parentWidget());
    }
    else
    {
        LAN::EndDiscovery();
    }

    QDialog::done(r);
}

void LANStartClientDialog::updateDiscoveryList()
{
    emit sgUpdateDiscoveryList();
}

void LANStartClientDialog::doUpdateDiscoveryList()
{
    if (LAN::DiscoveryMutex)
        Platform::Mutex_Lock(LAN::DiscoveryMutex);

    QStandardItemModel* model = (QStandardItemModel*)ui->tvAvailableGames->model();
    int curcount = model->rowCount();
    int newcount = LAN::DiscoveryList.size();
    if (curcount > newcount)
    {
        model->removeRows(newcount, curcount-newcount);
    }
    else if (curcount < newcount)
    {
        for (int i = curcount; i < newcount; i++)
        {
            QList<QStandardItem*> row;
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            model->appendRow(row);
        }
    }

    int i = 0;
    for (const auto& [key, data] : LAN::DiscoveryList)
    {
        model->item(i, 0)->setText(data.SessionName);
        model->item(i, 0)->setData(QVariant(key));

        QString plcount = QString("%0/%1").arg(data.NumPlayers).arg(data.MaxPlayers);
        model->item(i, 1)->setText(plcount);

        QString status;
        switch (data.Status)
        {
            case 0: status = "Idle"; break;
            case 1: status = "Playing"; break;
        }
        model->item(i, 2)->setText(status);

        QString ip = QString("%0.%1.%2.%3").arg(key>>24).arg((key>>16)&0xFF).arg((key>>8)&0xFF).arg(key&0xFF);
        model->item(i, 3)->setText(ip);

        i++;
    }

    if (LAN::DiscoveryMutex)
        Platform::Mutex_Unlock(LAN::DiscoveryMutex);
}


LANDialog::LANDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);
    const QStringList header = {"#", "Player", "Status", "Ping", "IP"};
    model->setHorizontalHeaderLabels(header);

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

void LANDialog::updatePlayerList()
{
    playerListMutex.lock();
    memcpy(playerList, LAN::Players, sizeof(playerList));
    memcpy(playerPing, LAN::PlayerPing, sizeof(playerPing));
    numPlayers = LAN::NumPlayers;
    maxPlayers = LAN::MaxPlayers;
    myPlayerID = LAN::MyPlayer.ID;
    hostAddress = LAN::HostAddress;
    playerListMutex.unlock();

    emit sgUpdatePlayerList();
}

void LANDialog::doUpdatePlayerList()
{
    playerListMutex.lock();

    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();
    int curcount = model->rowCount();
    int newcount = numPlayers;
    if (curcount > newcount)
    {
        model->removeRows(newcount, curcount-newcount);
    }
    else if (curcount < newcount)
    {
        for (int i = curcount; i < newcount; i++)
        {
            QList<QStandardItem*> row;
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            model->appendRow(row);
        }
    }

    for (int i = 0; i < 16; i++)
    {
        LAN::Player* player = &playerList[i];
        if (player->Status == 0) break;

        QString id = QString("%0/%1").arg(player->ID+1).arg(maxPlayers);
        model->item(i, 0)->setText(id);

        QString name = player->Name;
        model->item(i, 1)->setText(name);

        QString status;
        switch (player->Status)
        {
            case 1: status = "Connected"; break;
            case 2: status = "Game host"; break;
            case 3: status = "Connecting"; break;
            case 4: status = "Connection lost"; break;
        }
        model->item(i, 2)->setText(status);

        if (i == myPlayerID)
        {
            model->item(i, 3)->setText("-");
            model->item(i, 4)->setText("(local)");
        }
        else
        {
            if (player->Status == 1 || player->Status == 2)
            {
                QString ping = QString("%0 ms").arg(playerPing[i]);
                model->item(i, 3)->setText(ping);
            }
            else
            {
                model->item(i, 3)->setText("-");
            }

            // note on the player IP display
            // * we make an exception for the host -- the player list is issued by the host, so the host IP would be 127.0.0.1
            // * for the same reason, the host can't know its own IP, so for the current player we force it to 127.0.0.1
            u32 ip;
            if (player->Status == 2)
                ip = hostAddress;
            else
                ip = player->Address;

            QString ips = QString("%0.%1.%2.%3").arg(ip&0xFF).arg((ip>>8)&0xFF).arg((ip>>16)&0xFF).arg(ip>>24);
            model->item(i, 4)->setText(ips);
        }
    }

    playerListMutex.unlock();
}
