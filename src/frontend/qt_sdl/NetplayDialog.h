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

#ifndef NETPLAYDIALOG_H
#define NETPLAYDIALOG_H

#include <QDialog>

#include "types.h"
#include "Netplay.h"

namespace Ui
{
    class NetplayStartHostDialog;
    class NetplayStartClientDialog;
    class NetplayDialog;
}

class NetplayStartHostDialog;
class NetplayStartClientDialog;
class NetplayDialog;

class NetplayStartHostDialog : public QDialog
{
Q_OBJECT

public:
    explicit NetplayStartHostDialog(QWidget* parent);
    ~NetplayStartHostDialog();

    static NetplayStartHostDialog* openDlg(QWidget* parent)
    {
        NetplayStartHostDialog* dlg = new NetplayStartHostDialog(parent);
        dlg->open();
        return dlg;
    }

private slots:
    void done(int r);

private:
    Ui::NetplayStartHostDialog* ui;
};

class NetplayStartClientDialog : public QDialog
{
Q_OBJECT

public:
    explicit NetplayStartClientDialog(QWidget* parent);
    ~NetplayStartClientDialog();

    static NetplayStartClientDialog* openDlg(QWidget* parent)
    {
        NetplayStartClientDialog* dlg = new NetplayStartClientDialog(parent);
        dlg->open();
        return dlg;
    }

private slots:
    void done(int r);

private:
    Ui::NetplayStartClientDialog* ui;
};

class NetplayDialog : public QDialog
{
Q_OBJECT

public:
    explicit NetplayDialog(QWidget* parent);
    ~NetplayDialog();

    static NetplayDialog* openDlg(QWidget* parent)
    {
        NetplayDialog* dlg = new NetplayDialog(parent);
        dlg->show();
        return dlg;
    }

    void updatePlayerList(Netplay::Player* players, int num);

signals:
    void sgUpdatePlayerList(Netplay::Player* players, int num);

private slots:
    void done(int r);

    void doUpdatePlayerList(Netplay::Player* players, int num);

private:
    Ui::NetplayDialog* ui;
};

#endif // NETPLAYDIALOG_H
