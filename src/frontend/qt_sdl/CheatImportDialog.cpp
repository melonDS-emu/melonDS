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
#include <QStandardItemModel>

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
        QMessageBox::critical(this, "melonDS",
                              "Failed to open the cheat database file.");
        close();
        return;
    }

    dbEntriesByGameCode = database->GetEntriesByGameCode(gameCode);
    if (dbEntriesByGameCode.empty())
    {
        QMessageBox::critical(this, "melonDS",
                              "No cheat codes were found in this database for this game.");
        close();
        return;
    }

    hasChecksumMatches = false;
    for (auto& entry : dbEntriesByGameCode)
    {
        if (entry.Checksum == gameChecksum)
        {
            hasChecksumMatches = true;
            break;
        }
    }

    if (hasChecksumMatches)
    {
        ui->chkShowAllMatches->setEnabled(true);
        ui->chkShowAllMatches->setCheckState(Qt::Unchecked);
    }
    else
    {
        ui->chkShowAllMatches->setEnabled(false);
        ui->chkShowAllMatches->setCheckState(Qt::Checked);
    }

    auto dbname = database->GetDBName();
    ui->lblDatabaseName->setText(QString::fromStdString(dbname));

    ui->gbCatInfo->hide();
    ui->gbCodeInfo->hide();

    QImage blank(64, 64, QImage::Format_Alpha8);
    blank.fill(0);
    blankIcon = QIcon(QPixmap::fromImage(blank.copy()));

    matchIcon = ((QWidget*)parent)->style()->standardIcon(QStyle::SP_DialogApplyButton);

    updatingImportChk = true;
    auto treemodel = new QStandardItemModel();
    ui->tvCheatList->setModel(treemodel);
    connect(treemodel, &QStandardItemModel::itemChanged, this, &CheatImportDialog::onCheatEntryModified);
    connect(ui->tvCheatList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CheatImportDialog::onCheatSelectionChanged);

    populateEntryList();
    updatingImportChk = false;
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
    populateEntryList();
}

void CheatImportDialog::on_cbEntryList_currentIndexChanged(int idx)
{
    populateEntryInfo();
}

void CheatImportDialog::onCheatSelectionChanged(const QItemSelection &sel, const QItemSelection &desel)
{
    populateCheatInfo();
}

void CheatImportDialog::onCheatEntryModified(QStandardItem* item)
{
    int itemtype = item->data(Qt::UserRole).toInt();
    if (itemtype == 2)
    {
        // sync up the enable map
        auto code = item->data(Qt::UserRole+1).value<melonDS::ARCode*>();
        importEnableMap[code] = (item->checkState() == Qt::Checked);
    }

    if (updatingImportChk) return;
    updatingImportChk = true;

    if (item->hasChildren())
    {
        // if this item is a category, we need to check/uncheck all the children

        Qt::CheckState chk = item->checkState();
        if (chk == Qt::PartiallyChecked)
        {
            updatingImportChk = false;
            return;
        }

        int rows = item->rowCount();
        for (int i = 0; i < rows; i++)
            item->child(i)->setCheckState(chk);
    }
    else
    {
        // if this item is a code, we need to update the check on the parent

        QStandardItem* parent = item->parent();
        QStandardItem* root = ((QStandardItemModel*)ui->tvCheatList->model())->invisibleRootItem();
        if ((!parent) || (parent == root))
        {
            updatingImportChk = false;
            return;
        }

        bool allyes = true;
        bool allno = true;
        int rows = parent->rowCount();
        for (int i = 0; i < rows; i++)
        {
            Qt::CheckState chk = parent->child(i)->checkState();
            if (chk != Qt::Checked) allyes = false;
            if (chk != Qt::Unchecked) allno = false;
        }

        if (allyes)     parent->setCheckState(Qt::Checked);
        else if (allno) parent->setCheckState(Qt::Unchecked);
        else            parent->setCheckState(Qt::PartiallyChecked);
    }

    updatingImportChk = false;
}

void CheatImportDialog::populateEntryList()
{
    ui->cbEntryList->clear();

    bool showall = ui->chkShowAllMatches->isChecked();
    int i = 0, sel = -1;
    for (auto& entry : dbEntriesByGameCode)
    {
        bool crcmatch = (entry.Checksum == gameChecksum);
        if ((!showall) && (!crcmatch))
            continue;

        QIcon& icon = crcmatch ? matchIcon : blankIcon;
        QString name = QString::fromStdString(entry.Name);
        ui->cbEntryList->addItem(icon, name, QVariant::fromValue(&entry));

        if (crcmatch && (sel == -1))
            sel = i;
        i++;
    }

    if (sel != -1)
        ui->cbEntryList->setCurrentIndex(sel);

    populateEntryInfo();
}

void CheatImportDialog::populateEntryInfo()
{
    int idx = ui->cbEntryList->currentIndex();
    if ((idx < 0) || (idx >= ui->cbEntryList->count()))
    {
        // nothing selected

        ui->gbEntryInfo->hide();
        return;
    }

    ui->gbEntryInfo->show();

    QVariant data = ui->cbEntryList->currentData();
    auto entry = data.value<melonDS::ARDatabaseEntry*>();

    ui->lblEntryName->setText(QString::fromStdString(entry->Name));

    char gamecode[5] = {0};
    memcpy(gamecode, &entry->GameCode, 4);
    ui->lblEntryGameCode->setText(gamecode);

    auto chksum = QString::asprintf("0x%08X (ROM: 0x%08X)", entry->Checksum, gameChecksum);
    ui->lblEntryChecksum->setText(chksum);

    populateCheatList();
}

void CheatImportDialog::populateCheatList()
{
    updatingImportChk = true;

    auto treemodel = (QStandardItemModel*)ui->tvCheatList->model();
    treemodel->clear();
    importEnableMap.clear();

    int idx = ui->cbEntryList->currentIndex();
    if ((idx < 0) || (idx >= ui->cbEntryList->count()))
    {
        updatingImportChk = false;
        populateCheatInfo();
        return;
    }

    QStandardItem* rootitem = treemodel->invisibleRootItem();

    QVariant data = ui->cbEntryList->currentData();
    auto entry = data.value<melonDS::ARDatabaseEntry*>();
    for (auto& cat : entry->Categories)
    {
        QStandardItem* catitem;
        if (cat.IsRoot)
        {
            catitem = rootitem;
        }
        else
        {
            QString catname = QString::fromStdString(cat.Name);

            catitem = new QStandardItem(catname);
            rootitem->appendRow(catitem);

            catitem->setData(1, Qt::UserRole);
            catitem->setData(QVariant::fromValue(&cat), Qt::UserRole+1);
            catitem->setCheckable(true);
            catitem->setCheckState(Qt::Checked);
        }

        for (auto& code : cat.Codes)
        {
            QString codename = QString::fromStdString(code.Name);

            auto codeitem = new QStandardItem(codename);
            catitem->appendRow(codeitem);

            codeitem->setData(2, Qt::UserRole);
            codeitem->setData(QVariant::fromValue(&code), Qt::UserRole+1);
            codeitem->setCheckable(true);
            codeitem->setCheckState(Qt::Checked);

            importEnableMap[&code] = true;
        }
    }

    updatingImportChk = false;
    populateCheatInfo();
}

void CheatImportDialog::populateCheatInfo()
{
    auto selmodel = ui->tvCheatList->selectionModel();
    if (!selmodel->hasSelection())
    {
        ui->gbCatInfo->hide();
        ui->gbCodeInfo->hide();
        return;
    }

    auto index = selmodel->selectedIndexes()[0];
    int itemtype = index.data(Qt::UserRole).toInt();
    if (itemtype == 1)
    {
        // category

        ui->gbCatInfo->show();
        ui->gbCodeInfo->hide();

        auto cat = index.data(Qt::UserRole+1).value<melonDS::ARCodeCat*>();
        QString catname = QString::fromStdString(cat->Name);
        QString catdesc = QString::fromStdString(cat->Description);
        QString codeenable = cat->OnlyOneCodeEnabled ? "Only one" : "Multiple";

        ui->lblCatName->setText(catname);
        ui->lblCatDesc->setText(catdesc);
        ui->lblCatEnableMode->setText(codeenable);
    }
    else if (itemtype == 2)
    {
        // cheat

        ui->gbCatInfo->hide();
        ui->gbCodeInfo->show();

        auto code = index.data(Qt::UserRole+1).value<melonDS::ARCode*>();
        QString codename = QString::fromStdString(code->Name);
        QString codedesc = QString::fromStdString(code->Description);
        QString codeenable = code->Enabled ? "Enabled" : "Disabled";

        ui->lblCodeName->setText(codename);
        ui->lblCodeDesc->setText(codedesc);
        ui->lblCodeStatus->setText(codeenable);

        QString codecode = "";
        for (int i = 0; i < code->Code.size(); i+=2)
        {
            if (i > 0) codecode += "\n";
            codecode += QString::asprintf("%08X %08X", code->Code[i], code->Code[i+1]);
        }

        ui->txtCodeCode->setPlainText(codecode);
    }
}
