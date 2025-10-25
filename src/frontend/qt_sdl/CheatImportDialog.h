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
#include <QStandardItem>
#include <QItemSelection>

#include "ARCodeFile.h"
#include "ARDatabaseDAT.h"

namespace Ui { class CheatImportDialog; }
class CheatImportDialog;

class EmuInstance;

// TODO move this to dedicated header
struct TreeItem
{
    TreeItem* parent;
    std::vector<TreeItem*> children;
    int num;

    int dataType;
    void* data;

    TreeItem(int type, void* data, TreeItem* parent)
    : dataType(type), data(data), parent(parent)
    {
        if (parent)
        {
            num = parent->children.size();
            parent->children.push_back(this);
        }
        else
            num = 0;
    }

    ~TreeItem()
    {
        for (auto child : children)
            delete child;
    }
};

class CheatImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheatImportDialog(QWidget* parent);
    ~CheatImportDialog();

private slots:
    void accept() override;

    void on_chkShowAllMatches_clicked(bool checked);
    void on_cbEntryList_currentIndexChanged(int idx);
    void onCheatSelectionChanged(const QItemSelection& sel, const QItemSelection& desel);
    void onCheatEntryModified(QStandardItem* item);

private:
    Ui::CheatImportDialog* ui;
    QIcon blankIcon, matchIcon;

    EmuInstance* emuInstance;

    melonDS::ARDatabaseDAT* database;
    melonDS::u32 gameCode;
    melonDS::u32 gameChecksum;

    melonDS::ARDatabaseEntryList dbEntriesByGameCode;
    bool hasChecksumMatches;

    bool updatingImportChk;

    void populateEntryList();
    void populateEntryInfo();
    void populateCheatList();
    void populateCheatInfo();
};

class CheatGameList : public QAbstractListModel
{
    Q_OBJECT

public:
    CheatGameList(QObject* parent, melonDS::ARDatabaseDAT* db);
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
    explicit CheatCodeImportList(QObject* parent);
    ~CheatCodeImportList();

    void setCurrentEntry(const melonDS::ARDatabaseEntry* entry);

    QModelIndex index(int row, int col, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    bool hasChildren(const QModelIndex& parent) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

public slots:
    //void onEntryChanged(const QItemSelection& sel, const QItemSelection& desel);

private:
    const melonDS::ARDatabaseEntry* curEntry;
    TreeItem* rootItem;
};

#endif // CHEATIMPORTDIALOG_H
