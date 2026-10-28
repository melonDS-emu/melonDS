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

#ifndef LANDIALOG_H
#define LANDIALOG_H

#include <QDialog>
#include <QMutex>
#include <QItemSelection>
#include <QGroupBox>

class QLineEdit;
class QLabel;

#include "types.h"

namespace Ui
{
    class LANStartHostDialog;
    class LANStartClientDialog;
    class LANDialog;
}

// settings for playing LAN games over the internet through a relay server (see net/VNet.h)
class RelaySettingsBox : public QGroupBox
{
Q_OBJECT

public:
    explicit RelaySettingsBox(QWidget* parent);

    // connects to (or disconnects from) the relay according to the current settings
    // returns false and shows a message box if connecting failed
    bool apply(bool showerrors);
    void saveConfig();
    void updateStatus();

signals:
    void applied();

private slots:
    void onConnectClicked();

private:
    QLineEdit* txtServer;
    QLineEdit* txtRoom;
    QLabel* lblStatus;
    QString lastError;
};

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
    void done(int r) override;

private:
    Ui::LANStartHostDialog* ui;
    RelaySettingsBox* relayBox;
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

protected:
    void timerEvent(QTimerEvent* event) override;

private slots:
    void onGameSelectionChanged(const QItemSelection& cur, const QItemSelection& prev);
    void on_tvAvailableGames_doubleClicked(QModelIndex index);
    void onDirectConnect();
    void done(int r) override;

    void doUpdateDiscoveryList();
    void onRelayApplied();

private:
    Ui::LANStartClientDialog* ui;
    RelaySettingsBox* relayBox;
    int timerID;
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

protected:
    void timerEvent(QTimerEvent* event) override;

private slots:
    void on_btnLeaveGame_clicked();
    void done(int r) override;

    void doUpdatePlayerList();

private:
    Ui::LANDialog* ui;
    int timerID;
};

#endif // LANDIALOG_H
