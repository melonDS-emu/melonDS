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

#ifndef LAN_H
#define LAN_H

#include <string>
#include <QDialog>

#include "types.h"

namespace Ui
{
class LANStartHostDialog;
class LANStartClientDialog;
class LANDialog;
}

namespace LAN
{
struct Player
{
    int ID;
    char Name[32];
    int Status; // 0=no player 1=normal 2=host 3=connecting
    u32 Address;
};
}

class LANStartHostDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LANStartHostDialog(QWidget* parent);
    ~LANStartHostDialog();

    static LANStartHostDialog* openDlg(QWidget* parent)
    {
        LANStartHostDialog* dlg = new LANStartHostDialog(parent);
        dlg->open();
        return dlg;
    }

private slots:
    void done(int r);

private:
    Ui::LANStartHostDialog* ui;
};

class LANStartClientDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LANStartClientDialog(QWidget* parent);
    ~LANStartClientDialog();

    static LANStartClientDialog* openDlg(QWidget* parent)
    {
        LANStartClientDialog* dlg = new LANStartClientDialog(parent);
        dlg->open();
        return dlg;
    }

private slots:
    void done(int r);

private:
    Ui::LANStartClientDialog* ui;
};

class LANDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LANDialog(QWidget* parent);
    ~LANDialog();

    static LANDialog* openDlg(QWidget* parent)
    {
        LANDialog* dlg = new LANDialog(parent);
        dlg->show();
        return dlg;
    }

    void updatePlayerList(LAN::Player* players, int num);

signals:
    void sgUpdatePlayerList(LAN::Player* players, int num);

private slots:
    void done(int r);

    void doUpdatePlayerList(LAN::Player* players, int num);

private:
    Ui::LANDialog* ui;
};

namespace LAN
{

extern bool Active;

bool Init();
void DeInit();

void StartHost(const char* player, int numplayers);
void StartClient(const char* player, const char* host);

void Process(bool block = false);

void SetMPRecvTimeout(int timeout);
void MPBegin();
void MPEnd();

int SendMPPacket(u8* data, int len, u64 timestamp);
int RecvMPPacket(u8* data, u64* timestamp);
int SendMPCmd(u8* data, int len, u64 timestamp);
int SendMPReply(u8* data, int len, u64 timestamp, u16 aid);
int SendMPAck(u8* data, int len, u64 timestamp);
int RecvMPHostPacket(u8* data, u64* timestamp);
u16 RecvMPReplies(u8* data, u64 timestamp, u16 aidmask);

}

#endif // LAN_H
