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

#ifndef CHEATIMPORTDIALOG_H
#define CHEATIMPORTDIALOG_H

#include <QDialog>
#include <QAbstractListModel>

#include "ARCodeFile.h"
#include "ARDatabaseDAT.h"

namespace Ui { class CheatImportDialog; }
class CheatImportDialog;

class EmuInstance;

class CheatImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheatImportDialog(QWidget* parent);
    ~CheatImportDialog();

private slots:
    void accept() override;

    void on_chkShowAllMatches_clicked(bool checked);

private:
    Ui::CheatImportDialog* ui;

    EmuInstance* emuInstance;

    melonDS::ARDatabaseDAT* database;
    melonDS::u32 gameCode;
    melonDS::u32 gameChecksum;
};

class CheatGameList : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit CheatGameList(QObject* parent, melonDS::ARDatabaseDAT* db);
    ~CheatGameList();

    void setGameInfo(melonDS::u32 code, melonDS::u32 checksum);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setListByChecksum(bool val);

private:
    melonDS::ARDatabaseDAT* database;
    melonDS::u32 gameCode;
    melonDS::u32 gameChecksum;

    melonDS::ARDatabaseEntryList dbEntriesByGameCode;
    melonDS::ARDatabaseEntryList dbEntriesByChecksum;
    bool listByChecksum;

    QIcon blankIcon, matchIcon;
};

class CheatCodeImportList : public QAbstractItemModel
{
    Q_OBJECT

public:
    //
};

#endif // CHEATIMPORTDIALOG_H
