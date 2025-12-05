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

class CheatImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheatImportDialog(QWidget* parent, melonDS::ARDatabaseDAT* db, melonDS::u32 gamecode, melonDS::u32 checksum);
    ~CheatImportDialog();

    melonDS::ARDatabaseEntry& getImportCheats();
    melonDS::ARCodeEnableMap& getImportEnableMap();
    bool getRemoveOldCodes();

private slots:
    void on_chkShowAllMatches_clicked(bool checked);
    void on_cbEntryList_currentIndexChanged(int idx);
    void onCheatSelectionChanged(const QItemSelection& sel, const QItemSelection& desel);
    void onCheatEntryModified(QStandardItem* item);

private:
    Ui::CheatImportDialog* ui;
    QIcon blankIcon, matchIcon;

    melonDS::ARDatabaseDAT* database;
    melonDS::u32 gameCode;
    melonDS::u32 gameChecksum;

    melonDS::ARDatabaseEntryList dbEntriesByGameCode;
    bool hasChecksumMatches;

    bool updatingImportChk;

    melonDS::ARCodeEnableMap importEnableMap;

    void populateEntryList();
    void populateEntryInfo();
    void populateCheatListCat(QStandardItem* parentitem, melonDS::ARCodeCat& parentcat);
    void populateCheatList();
    void populateCheatInfo();
};

#endif // CHEATIMPORTDIALOG_H
