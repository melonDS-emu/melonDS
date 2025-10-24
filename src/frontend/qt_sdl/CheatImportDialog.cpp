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
#include <QtGlobal>
#include <QFileDialog>
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "EmuInstance.h"
#include "ARDatabaseDAT.h"

#include "CheatImportDialog.h"
#include "ui_CheatImportDialog.h"

using namespace melonDS;
using Platform::Log;
using Platform::LogLevel;


CheatImportDialog::CheatImportDialog(QWidget *parent)
: QDialog(parent), ui(new Ui::CheatImportDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent->parentWidget())->getEmuInstance();

    auto rom = emuInstance->getNDS()->NDSCartSlot.GetCart();
    gameCode = rom->GetHeader().GameCodeAsU32();
    gameChecksum = ~CRC32(rom->GetROM(), 0x200, 0);

    // TODO this could be a base class thing and support multiple types of database
    // also maybe don't hardcode the filename
    database = new ARDatabaseDAT("usrcheat.dat");
    if (database->Error)
    {
        printf("database shat itself\n");
        return;
    }

    // test
    //database->Test();

    auto listmodel = new CheatGameList(ui->lvEntryList, database);
    listmodel->setGameInfo(gameCode, gameChecksum);
    ui->lvEntryList->setModel(listmodel);
}

CheatImportDialog::~CheatImportDialog()
{
    delete database;
    delete ui;
}

void CheatImportDialog::accept()
{
    //
}

void CheatImportDialog::on_chkShowAllMatches_clicked(bool checked)
{
    ((CheatGameList*)ui->lvEntryList->model())->setListByChecksum(!checked);
}


CheatGameList::CheatGameList(QObject* parent, melonDS::ARDatabaseDAT* db)
: QAbstractListModel(parent), database(db)
{
    gameCode = 0;
    gameChecksum = 0;
    dbEntriesByGameCode = {};
    dbEntriesByChecksum = {};
    listByChecksum = false;

    QImage blank(64, 64, QImage::Format_Alpha8);
    blank.fill(0);
    blankIcon = QIcon(QPixmap::fromImage(blank));

    matchIcon = ((QWidget*)parent)->style()->standardIcon(QStyle::SP_DialogApplyButton);
}

CheatGameList::~CheatGameList()
{
}

void CheatGameList::setGameInfo(melonDS::u32 code, melonDS::u32 checksum)
{
    gameCode = code;
    gameChecksum = checksum;

    dbEntriesByGameCode = database->GetEntriesByGameCode(gameCode);

    dbEntriesByChecksum = {};
    for (auto& entry : dbEntriesByGameCode)
    {
        if (entry.Checksum == gameChecksum)
            dbEntriesByChecksum.push_back(entry);
    }
}

int CheatGameList::rowCount(const QModelIndex& parent) const
{
    auto& list = listByChecksum ? dbEntriesByChecksum : dbEntriesByGameCode;
    return list.size();
}

QVariant CheatGameList::data(const QModelIndex& index, int role) const
{
    auto& list = listByChecksum ? dbEntriesByChecksum : dbEntriesByGameCode;
    int row = index.row();
    if (row >= list.size())
        return {};

    if (role == Qt::DisplayRole)
    {
        auto entry = list.at(row);
        return QString::fromStdString(entry.Name);
    }
    else if (role == Qt::DecorationRole)
    {
        auto entry = list.at(row);
        if (entry.Checksum == gameChecksum)
            return matchIcon;
        else
            return blankIcon;
    }

    return {};
}

Qt::ItemFlags CheatGameList::flags(const QModelIndex& index) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant CheatGameList::headerData(int section, Qt::Orientation orientation, int role) const
{
    return {};
}

void CheatGameList::setListByChecksum(bool val)
{
    beginResetModel();
    listByChecksum = val;
    endResetModel();
}
