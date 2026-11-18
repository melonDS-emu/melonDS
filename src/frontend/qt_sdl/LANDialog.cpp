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
#include <string.h>
#include <queue>
#include <vector>

#include <QStandardItemModel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>

#include "LANDialog.h"
#include "Config.h"
#include "main.h"
#include "LAN.h"
#include "VNet.h"

#include "ui_LANStartHostDialog.h"
#include "ui_LANStartClientDialog.h"
#include "ui_LANDialog.h"

using namespace melonDS;


LANStartClientDialog* lanClientDlg = nullptr;
LANDialog* lanDlg = nullptr;

#define lan() ((LAN&)MPInterface::Get())


RelaySettingsBox::RelaySettingsBox(QWidget* parent) : QGroupBox(parent)
{
    setTitle("Play over the internet (relay server)");
    setCheckable(true);

    auto cfg = Config::GetGlobalTable();
    setChecked(cfg.GetBool("LAN.Relay.Enabled"));

    txtServer = new QLineEdit(cfg.GetQString("LAN.Relay.Server"));
    txtServer->setPlaceholderText("address:port");

    txtRoom = new QLineEdit(cfg.GetQString("LAN.Relay.Room"));
    txtRoom->setPlaceholderText("same room name as your friends");
    txtRoom->setMaxLength(64);

    QPushButton* btnConnect = new QPushButton("Connect");
    connect(btnConnect, &QPushButton::clicked, this, &RelaySettingsBox::onConnectClicked);
    connect(this, &QGroupBox::toggled, this, [this](bool) { apply(false); emit applied(); });

    QHBoxLayout* roomrow = new QHBoxLayout();
    roomrow->addWidget(txtRoom);
    roomrow->addWidget(btnConnect);

    lblStatus = new QLabel();
    lblStatus->setWordWrap(true);

    QFormLayout* form = new QFormLayout(this);
    form->addRow("Server:", txtServer);
    form->addRow("Room:", roomrow);
    form->addRow(lblStatus);

    updateStatus();
}

bool RelaySettingsBox::apply(bool showerrors)
{
    lastError.clear();

    if (!isChecked())
    {
        VNet::Stop();
        updateStatus();
        return true;
    }

    std::string server = txtServer->text().trimmed().toStdString();
    std::string room = txtRoom->text().trimmed().toStdString();
    std::string error;

    setEnabled(false);
    bool ok = VNet::Start(server.c_str(), room.c_str(), error);
    setEnabled(true);

    if (!ok)
    {
        lastError = QString::fromStdString(error);
        if (showerrors)
            QMessageBox::warning(parentWidget(), "melonDS", lastError);
    }
    else
    {
        saveConfig();
    }

    updateStatus();
    return ok;
}

void RelaySettingsBox::saveConfig()
{
    auto cfg = Config::GetGlobalTable();
    cfg.SetBool("LAN.Relay.Enabled", isChecked());
    cfg.SetQString("LAN.Relay.Server", txtServer->text().trimmed());
    cfg.SetQString("LAN.Relay.Room", txtRoom->text().trimmed());
    Config::Save();
}

void RelaySettingsBox::updateStatus()
{
    if (!isChecked())
    {
        lblStatus->setText("Off: only players on your local network can be found.");
    }
    else if (VNet::IsActive())
    {
        int ping = VNet::GetRelayPing();
        QString pingstr = (ping >= 0) ? QString("%0 ms").arg(ping) : QString("?");
        lblStatus->setText(QString("<b>Online</b> in room <b>%0</b>. Your address: %1. Ping to server: %2")
            .arg(txtRoom->text().trimmed().toHtmlEscaped())
            .arg(QString::fromStdString(VNet::GetVirtualIPString()))
            .arg(pingstr));
    }
    else if (!lastError.isEmpty())
    {
        lblStatus->setText(QString("<span style='color:#c00'>%0</span>").arg(lastError.toHtmlEscaped()));
    }
    else
    {
        lblStatus->setText("Not connected. Enter the same room name as your friends and click Connect.");
    }
}

void RelaySettingsBox::onConnectClicked()
{
    apply(true);
    emit applied();
}



LANStartHostDialog::LANStartHostDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartHostDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setMPInterface(MPInterface_LAN);

    auto cfg = Config::GetGlobalTable();
    ui->txtPlayerName->setText(cfg.GetQString("LAN.PlayerName"));

    ui->sbNumPlayers->setRange(2, 16);
    ui->sbNumPlayers->setValue(cfg.GetInt("LAN.HostNumPlayers"));

    relayBox = new RelaySettingsBox(this);
    ui->verticalLayout->insertWidget(0, relayBox);
}

LANStartHostDialog::~LANStartHostDialog()
{
    delete ui;
}

void LANStartHostDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        return;
    }

    if (r == QDialog::Accepted)
    {
        if (ui->txtPlayerName->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, "melonDS", "Please enter a player name.");
            return;
        }

        std::string player = ui->txtPlayerName->text().toStdString();
        int numplayers = ui->sbNumPlayers->value();

        // (re)connect to the relay, so the room settings shown are the ones in use
        if (!relayBox->apply(true))
            return;

        if (!lan().StartHost(player.c_str(), numplayers))
        {
            QMessageBox::warning(this, "melonDS", "Failed to start LAN game.");
            return;
        }

        lanDlg = LANDialog::openDlg(parentWidget());

        auto cfg = Config::GetGlobalTable();
        cfg.SetString("LAN.PlayerName", player);
        cfg.SetInt("LAN.HostNumPlayers", numplayers);
        Config::Save();
    }
    else
    {
        setMPInterface(MPInterface_Local);
    }

    QDialog::done(r);
}


LANStartClientDialog::LANStartClientDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartClientDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    setMPInterface(MPInterface_LAN);

    auto cfg = Config::GetGlobalTable();
    ui->txtPlayerName->setText(cfg.GetQString("LAN.PlayerName"));

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

    relayBox = new RelaySettingsBox(this);
    ui->verticalLayout->insertWidget(0, relayBox);
    connect(relayBox, &RelaySettingsBox::applied, this, &LANStartClientDialog::onRelayApplied);
    if (relayBox->isChecked())
        relayBox->apply(false);

    lanClientDlg = this;
    lan().StartDiscovery();

    timerID = startTimer(1000);
}

void LANStartClientDialog::onRelayApplied()
{
    // restart discovery so it listens on the right network
    lan().EndDiscovery();
    ((QStandardItemModel*)ui->tvAvailableGames->model())->removeRows(0, ui->tvAvailableGames->model()->rowCount());
    lan().StartDiscovery();
}

LANStartClientDialog::~LANStartClientDialog()
{
    killTimer(timerID);

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
    lan().EndDiscovery();
    if (!lan().StartClient(player.c_str(), hostname.c_str()))
    {
        QString msg = QString("Failed to connect to the host %0.").arg(QString::fromStdString(hostname));
        QMessageBox::warning(this, "melonDS", msg);
        setEnabled(true);
        lan().StartDiscovery();
        return;
    }

    setEnabled(true);
    lanDlg = LANDialog::openDlg(parentWidget());
    QDialog::done(QDialog::Accepted);
}

void LANStartClientDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        return;
    }

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
        lan().EndDiscovery();
        if (!lan().StartClient(player.c_str(), hostname))
        {
            QString msg = QString("Failed to connect to the host %0.").arg(QString(hostname));
            QMessageBox::warning(this, "melonDS", msg);
            setEnabled(true);
            lan().StartDiscovery();
            return;
        }

        setEnabled(true);
        lanDlg = LANDialog::openDlg(parentWidget());

        auto cfg = Config::GetGlobalTable();
        cfg.SetString("LAN.PlayerName", player);
        Config::Save();
    }
    else
    {
        lan().EndDiscovery();
        setMPInterface(MPInterface_Local);
    }

    QDialog::done(r);
}

void LANStartClientDialog::timerEvent(QTimerEvent *event)
{
    doUpdateDiscoveryList();
    relayBox->updateStatus();
}

void LANStartClientDialog::doUpdateDiscoveryList()
{
    auto disclist = lan().GetDiscoveryList();

    QStandardItemModel* model = (QStandardItemModel*)ui->tvAvailableGames->model();
    int curcount = model->rowCount();
    int newcount = disclist.size();
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
    for (const auto& [key, data] : disclist)
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
}


LANDialog::LANDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);
    const QStringList header = {"#", "Player", "Status", "Ping", "IP"};
    model->setHorizontalHeaderLabels(header);

    timerID = startTimer(1000);
}

LANDialog::~LANDialog()
{
    killTimer(timerID);

    delete ui;
}

void LANDialog::on_btnLeaveGame_clicked()
{
    done(QDialog::Accepted);
}

void LANDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        return;
    }

    bool showwarning = true;
    if (lan().GetNumPlayers() < 2)
        showwarning = false;

    if (showwarning)
    {
        if (QMessageBox::warning(this, "melonDS", "Really leave this LAN game?",
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
            return;
    }

    lan().EndSession();
    setMPInterface(MPInterface_Local);

    QDialog::done(r);
}

void LANDialog::timerEvent(QTimerEvent *event)
{
    doUpdatePlayerList();
}

void LANDialog::doUpdatePlayerList()
{
    auto playerlist = lan().GetPlayerList();
    auto maxplayers = lan().GetMaxPlayers();

    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();
    int curcount = model->rowCount();
    int newcount = playerlist.size();
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

    int i = 0;
    for (const auto& player : playerlist)
    {
        QString id = QString("%0/%1").arg(player.ID+1).arg(maxplayers);
        model->item(i, 0)->setText(id);

        QString name = player.Name;
        model->item(i, 1)->setText(name);

        QString status = "???";
        switch (player.Status)
        {
            case LAN::Player_Client:
                status = "Connected";
                break;
            case LAN::Player_Host:
                status = "Game host";
                break;
            case LAN::Player_Connecting:
                status = "Connecting";
                break;
            case LAN::Player_Disconnected:
                status = "Connection lost";
                break;
            case LAN::Player_None:
                break;
        }
        model->item(i, 2)->setText(status);

        if (player.IsLocalPlayer)
        {
            model->item(i, 3)->setText("-");
            model->item(i, 4)->setText("(local)");
        }
        else
        {
            if (player.Status == LAN::Player_Client ||
                player.Status == LAN::Player_Host)
            {
                QString ping = QString("%0 ms").arg(player.Ping);
                model->item(i, 3)->setText(ping);
            }
            else
            {
                model->item(i, 3)->setText("-");
            }


            u32 ip = player.Address;

            QString ips = QString("%0.%1.%2.%3").arg(ip&0xFF).arg((ip>>8)&0xFF).arg((ip>>16)&0xFF).arg(ip>>24);
            model->item(i, 4)->setText(ips);
        }

        i++;
    }
}
