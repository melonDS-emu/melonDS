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

#include <QtGlobal>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>

#include "types.h"
#include "Platform.h"
#include "EmuInstance.h"
#include "ARDatabaseDAT.h"

#include "CheatsDialog.h"
#include "ui_CheatsDialog.h"
#include "CheatImportDialog.h"

using namespace melonDS;
using Platform::Log;
using Platform::LogLevel;

CheatsDialog* CheatsDialog::currentDlg = nullptr;


CheatsDialog::CheatsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::CheatsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    importDB = nullptr;
    importDlg = nullptr;
    updatingEnableChk = false;

    ui->splitter->setStretchFactor(0, 2);
    ui->splitter->setStretchFactor(1, 3);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto rom = emuInstance->getNDS()->NDSCartSlot.GetCart();
    gameCode = rom->GetHeader().GameCodeAsU32();
    gameChecksum = ~CRC32(rom->GetROM(), 0x200, 0);

    codeFile = emuInstance->getCheatFile();

    auto* model = new CheatListModel(ui->tvCodeList);
    ui->tvCodeList->setModel(model);
    connect(model, &QStandardItemModel::itemChanged, this, &CheatsDialog::onCheatEntryModified);
    connect(ui->tvCodeList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CheatsDialog::onCheatSelectionChanged);

    populateCheatList();

    codeChecker = new ARCodeChecker(ui->txtCode->document());
}

CheatsDialog::~CheatsDialog()
{
    delete codeChecker;

    delete ui;
}

void CheatsDialog::done(int r)
{
    codeFile->Save();

    QDialog::done(r);
    closeDlg();
}

void CheatsDialog::on_btnNewCat_clicked()
{
    QStandardItem* root = ((QStandardItemModel*)ui->tvCodeList->model())->invisibleRootItem();

    ARCodeCat& parentcat = codeFile->RootCat;

    ARCodeCat cat;
    cat.Parent = &parentcat;
    cat.Name = "(new category)";
    cat.OnlyOneCodeEnabled = false;

    parentcat.Children.emplace_back(cat);
    auto catptr = &std::get<ARCodeCat>(parentcat.Children.back());

    auto catitem = new QStandardItem(QString::fromStdString(cat.Name));
    catitem->setDropEnabled(true);
    catitem->setData(1, Qt::UserRole);
    catitem->setData(QVariant::fromValue(catptr), Qt::UserRole+1);
    root->appendRow(catitem);

    ui->tvCodeList->selectionModel()->select(catitem->index(), QItemSelectionModel::ClearAndSelect);
    ui->tvCodeList->scrollTo(catitem->index());

    ui->btnEditCode->click();
}

void CheatsDialog::on_btnNewARCode_clicked()
{
    auto model = (QStandardItemModel*)ui->tvCodeList->model();
    auto selmodel = ui->tvCodeList->selectionModel();
    QStandardItem* parentitem = nullptr;
    ARCodeCat* cat;

    if (selmodel->hasSelection())
    {
        QModelIndexList indices = selmodel->selectedIndexes();
        QStandardItem *item = model->itemFromIndex(indices.first());

        int itemtype = item->data(Qt::UserRole).toInt();
        if (itemtype == 1)
            parentitem = item;
        else if (itemtype == 2)
            parentitem = item->parent();
        else
            assert(false);
    }

    if (!parentitem) parentitem = model->invisibleRootItem();
    cat = parentitem->data(Qt::UserRole+1).value<ARCodeCat*>();

    ARCode code;
    code.Parent = cat;
    code.Name = "(new AR code)";
    code.Enabled = false;

    cat->Children.emplace_back(code);
    auto codeptr = &std::get<ARCode>(cat->Children.back());

    auto codeitem = new QStandardItem(QString::fromStdString(code.Name));
    codeitem->setCheckable(true);
    codeitem->setCheckState(code.Enabled ? Qt::Checked : Qt::Unchecked);
    codeitem->setDropEnabled(false);
    codeitem->setData(2, Qt::UserRole);
    codeitem->setData(QVariant::fromValue(codeptr), Qt::UserRole+1);
    parentitem->appendRow(codeitem);

    ui->tvCodeList->selectionModel()->select(codeitem->index(), QItemSelectionModel::ClearAndSelect);
    ui->tvCodeList->scrollTo(codeitem->index());

    ui->btnEditCode->click();
}

void CheatsDialog::on_btnDeleteCode_clicked()
{
    QModelIndexList indices = ui->tvCodeList->selectionModel()->selectedIndexes();
    if (indices.isEmpty())
    {
        // ????
        return;
    }

    QMessageBox::StandardButton res = QMessageBox::question(this,
                                                            "Confirm deletion",
                                                            "Really delete the selected item?",
                                                            QMessageBox::Yes|QMessageBox::No,
                                                            QMessageBox::No);
    if (res != QMessageBox::Yes) return;

    auto model = (QStandardItemModel*)ui->tvCodeList->model();
    QStandardItem* item = model->itemFromIndex(indices.first());

    QStandardItem* itemparent = item->parent();
    if (!itemparent) itemparent = model->invisibleRootItem();

    int itemtype = item->data(Qt::UserRole).toInt();
    if (itemtype == 1)
    {
        auto cat = item->data(Qt::UserRole+1).value<ARCodeCat*>();

        auto parentcat = cat->Parent;
        for (auto it = parentcat->Children.begin(); it != parentcat->Children.end(); it++)
        {
            auto& childitem = *it;
            if (!std::holds_alternative<ARCodeCat>(childitem))
                continue;
            if (&std::get<ARCodeCat>(childitem) != cat)
                continue;

            parentcat->Children.erase(it);
            break;
        }
    }
    else if (itemtype == 2)
    {
        auto code = item->data(Qt::UserRole+1).value<ARCode*>();

        auto parentcat = code->Parent;
        for (auto it = parentcat->Children.begin(); it != parentcat->Children.end(); it++)
        {
            auto& childitem = *it;
            if (!std::holds_alternative<ARCode>(childitem))
                continue;
            if (&std::get<ARCode>(childitem) != code)
                continue;

            parentcat->Children.erase(it);
            break;
        }
    }
    else
        assert(false);

    itemparent->removeRow(item->row());
}

void CheatsDialog::on_btnImportCheats_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select cheat database...",
                                                emuDirectory,
                                                "R4 cheat database (*.dat);;Any file (*.*)");

    if (file.isEmpty()) return;

    importDB = new ARDatabaseDAT(file.toStdString());
    if (importDB->Error)
    {
        QMessageBox::critical(this, "melonDS",
                              "Failed to open this cheat database file.");
        delete importDB;
        return;
    }

    if (!importDB->FindGameCode(gameCode))
    {
        QMessageBox::critical(this, "melonDS",
                              "No cheat codes were found in this database for the current game.");
        delete importDB;
        return;
    }

    importDlg = new CheatImportDialog(this, importDB, gameCode, gameChecksum);
    importDlg->open();
    connect(importDlg, &CheatImportDialog::finished, this, &CheatsDialog::onImportCheatsFinished);

    importDlg->show();
}

void CheatsDialog::onImportCheatsFinished(int res)
{
    if (!importDlg) return;

    if (res == QDialog::Accepted)
    {
        auto& cheats = importDlg->getImportCheats();
        auto& enablemap = importDlg->getImportEnableMap();
        auto removeold = importDlg->getRemoveOldCodes();

        codeFile->Import(cheats, enablemap, removeold);

        populateCheatList();
    }

    delete importDB;
    importDB = nullptr;
    importDlg = nullptr;
}

void CheatsDialog::onCheatSelectionChanged(const QItemSelection& sel, const QItemSelection& desel)
{
    populateCheatInfo();
}

void CheatsDialog::onCheatEntryModified(QStandardItem* item)
{
    if (updatingEnableChk) return;

    auto selmodel = ui->tvCodeList->selectionModel();
    int itemtype = item->data(Qt::UserRole).toInt();
    if (itemtype == 2)
    {
        auto code = item->data(Qt::UserRole+1).value<ARCode*>();

        code->Enabled = (item->checkState() == Qt::Checked);

        if (code->Enabled && code->Parent->OnlyOneCodeEnabled)
        {
            // if needed, disable other codes

            updatingEnableChk = true;

            auto parent = item->parent();
            for (int i = 0; i < parent->rowCount(); i++)
            {
                auto child = parent->child(i);
                if (child == item) continue;

                int childtype = child->data(Qt::UserRole).toInt();
                if (childtype == 2)
                {
                    auto childcode = child->data(Qt::UserRole+1).value<ARCode*>();
                    childcode->Enabled = false;

                    child->setCheckState(Qt::Unchecked);

                    if (selmodel->hasSelection())
                    {
                        auto index = selmodel->selectedIndexes()[0];
                        if (index == child->index())
                        {
                            ui->chkItemOption->setChecked(childcode->Enabled);
                        }
                    }
                }
            }

            updatingEnableChk = false;
        }

        // if this item is selected, reflect the checkbox change
        if (selmodel->hasSelection())
        {
            auto index = selmodel->selectedIndexes()[0];
            if (index == item->index())
            {
                ui->chkItemOption->setChecked(code->Enabled);
            }
        }
    }
}

void CheatsDialog::on_btnEditCode_clicked()
{
    ui->btnNewCat->setEnabled(false);
    ui->btnNewARCode->setEnabled(false);
    ui->btnImportCheats->setEnabled(false);
    ui->tvCodeList->setEnabled(false);

    ui->btnEditCode->hide();
    ui->btnDeleteCode->setEnabled(false);
    ui->btnSaveCode->show();
    ui->btnCancelEdit->show();

    ui->txtItemName->setReadOnly(false);
    ui->txtItemDesc->setReadOnly(false);
    ui->chkItemOption->setEnabled(true);
    ui->txtCode->setReadOnly(false);
}

void CheatsDialog::on_btnSaveCode_clicked()
{
    auto selmodel = ui->tvCodeList->selectionModel();
    assert(selmodel->hasSelection());
    auto index = selmodel->selectedIndexes()[0];

    if (ui->txtItemName->text().trimmed().isEmpty())
    {
        QMessageBox::critical(this, "melonDS", "Error: no name entered.");
        return;
    }

    int itemtype = index.data(Qt::UserRole).toInt();
    if (itemtype == 1)
    {
        auto cat = index.data(Qt::UserRole+1).value<ARCodeCat*>();

        cat->Name = ui->txtItemName->text().toStdString();
        cat->Description = ui->txtItemDesc->text().toStdString();
        cat->OnlyOneCodeEnabled = ui->chkItemOption->isChecked();

        auto tvmodel = (QStandardItemModel*)ui->tvCodeList->model();
        auto tvitem = tvmodel->itemFromIndex(index);
        tvitem->setText(ui->txtItemName->text());
    }
    else if (itemtype == 2)
    {
        auto code = index.data(Qt::UserRole+1).value<ARCode*>();

        auto codeconv = convertCodeInput();
        if (codeconv.empty())
        {
            QMessageBox::critical(this, "melonDS", "Error: the code entered is empty or invalid.");
            return;
        }

        code->Name = ui->txtItemName->text().toStdString();
        code->Description = ui->txtItemDesc->text().toStdString();
        code->Enabled = ui->chkItemOption->isChecked();
        code->Code = codeconv;

        //updatingEnableChk = true;

        auto tvmodel = (QStandardItemModel*)ui->tvCodeList->model();
        auto tvitem = tvmodel->itemFromIndex(index);
        tvitem->setText(ui->txtItemName->text());
        tvitem->setCheckState(code->Enabled ? Qt::Checked : Qt::Unchecked);

        //updatingEnableChk = false;
    }
    else
        assert(false);

    ui->btnNewCat->setEnabled(true);
    ui->btnNewARCode->setEnabled(true);
    ui->btnImportCheats->setEnabled(true);
    ui->tvCodeList->setEnabled(true);

    ui->btnEditCode->show();
    ui->btnDeleteCode->setEnabled(true);
    ui->btnSaveCode->hide();
    ui->btnCancelEdit->hide();

    populateCheatInfo();
}

void CheatsDialog::on_btnCancelEdit_clicked()
{
    ui->btnNewCat->setEnabled(true);
    ui->btnNewARCode->setEnabled(true);
    ui->btnImportCheats->setEnabled(true);
    ui->tvCodeList->setEnabled(true);

    ui->btnEditCode->show();
    ui->btnDeleteCode->setEnabled(true);
    ui->btnSaveCode->hide();
    ui->btnCancelEdit->hide();

    populateCheatInfo();
}

void CheatsDialog::populateCheatList()
{
    auto model = (CheatListModel*)ui->tvCodeList->model();
    model->clear();

    QStandardItem* root = model->invisibleRootItem();
    root->setData(0, Qt::UserRole);
    root->setData(QVariant::fromValue(&codeFile->RootCat), Qt::UserRole+1);
    model->populateCheatListCat(root, codeFile->RootCat);

    populateCheatInfo();
}

void CheatsDialog::populateCheatInfo()
{
    auto selmodel = ui->tvCodeList->selectionModel();
    if (!selmodel->hasSelection())
    {
        ui->splitter->widget(1)->setEnabled(false);

        ui->btnDeleteCode->show();
        ui->btnDeleteCode->setEnabled(false);
        ui->btnEditCode->show();
        ui->btnEditCode->setEnabled(false);
        ui->btnSaveCode->hide();
        ui->btnCancelEdit->hide();

        ui->txtItemName->clear();
        ui->txtItemName->setReadOnly(true);
        ui->txtItemDesc->clear();
        ui->txtItemDesc->setReadOnly(true);

        ui->chkItemOption->setText("Enabled");
        ui->chkItemOption->setChecked(false);
        ui->chkItemOption->setEnabled(false);

        ui->lblCode->show();
        ui->txtCode->show();
        ui->txtCode->setPlainText("");
        ui->txtCode->setReadOnly(true);

        return;
    }

    ui->splitter->widget(1)->setEnabled(true);

    auto index = selmodel->selectedIndexes()[0];
    int itemtype = index.data(Qt::UserRole).toInt();
    if (itemtype == 1)
    {
        auto cat = index.data(Qt::UserRole+1).value<ARCodeCat*>();

        ui->btnDeleteCode->show();
        ui->btnDeleteCode->setEnabled(true);
        ui->btnEditCode->show();
        ui->btnEditCode->setEnabled(true);
        ui->btnSaveCode->hide();
        ui->btnCancelEdit->hide();

        ui->txtItemName->setText(QString::fromStdString(cat->Name));
        ui->txtItemName->setReadOnly(true);
        ui->txtItemDesc->setText(QString::fromStdString(cat->Description));
        ui->txtItemDesc->setReadOnly(true);

        ui->chkItemOption->setText("Only allow one code enabled");
        ui->chkItemOption->setChecked(cat->OnlyOneCodeEnabled);
        ui->chkItemOption->setEnabled(false);

        ui->lblCode->hide();
        ui->txtCode->hide();
    }
    else if (itemtype == 2)
    {
        auto code = index.data(Qt::UserRole+1).value<ARCode*>();

        ui->btnDeleteCode->show();
        ui->btnDeleteCode->setEnabled(true);
        ui->btnEditCode->show();
        ui->btnEditCode->setEnabled(true);
        ui->btnSaveCode->hide();
        ui->btnCancelEdit->hide();

        ui->txtItemName->setText(QString::fromStdString(code->Name));
        ui->txtItemName->setReadOnly(true);
        ui->txtItemDesc->setText(QString::fromStdString(code->Description));
        ui->txtItemDesc->setReadOnly(true);

        ui->chkItemOption->setText("Enabled");
        ui->chkItemOption->setChecked(code->Enabled);
        ui->chkItemOption->setEnabled(false);

        ui->lblCode->show();
        ui->txtCode->show();

        QString codestr = "";
        for (size_t i = 0; i < code->Code.size(); i += 2)
        {
            u32 c0 = code->Code[i+0];
            u32 c1 = code->Code[i+1];
            //codestr += QString("%1 %2\n").arg(c0, 8, 16, '0').arg(c1, 8, 16, '0').toUpper();
            codestr += QString::asprintf("%08X %08X\n", c0, c1);
        }
        ui->txtCode->setPlainText(codestr);
        ui->txtCode->setReadOnly(true);
    }
}

std::vector<u32> CheatsDialog::convertCodeInput()
{
    std::vector<u32> codeout;

    QString text = ui->txtCode->toPlainText();
    text = text.replace((QChar)'o', (QChar)'0', Qt::CaseInsensitive);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
#else
    QStringList lines = text.split('\n', QString::SkipEmptyParts);
#endif
    for (QStringList::iterator it = lines.begin(); it != lines.end(); it++)
    {
        QString line = *it;
        line = line.trimmed();
        if (line.isEmpty()) continue;

        if (line.length() > 17)
        {
            return {};
        }

        QStringList numbers = line.split(' ');
        if (numbers.length() != 2)
        {
            return {};
        }

        QStringList::iterator jt = numbers.begin();
        QString s0 = *jt++;
        QString s1 = *jt++;

        bool c0good, c1good;
        u32 c0, c1;

        c0 = s0.toUInt(&c0good, 16);
        c1 = s1.toUInt(&c1good, 16);

        if (!c0good || !c1good)
        {
            return {};
        }

        codeout.push_back(c0);
        codeout.push_back(c1);
    }

    return codeout;
}


void CheatListModel::populateCheatListCat(QStandardItem* parentitem, ARCodeCat& parentcat)
{
    for (auto& item : parentcat.Children)
    {
        if (std::holds_alternative<ARCodeCat>(item))
        {
            auto& cat = std::get<ARCodeCat>(item);

            auto catitem = new QStandardItem(QString::fromStdString(cat.Name));
            catitem->setDropEnabled(true);
            catitem->setData(1, Qt::UserRole);
            catitem->setData(QVariant::fromValue(&cat), Qt::UserRole+1);
            parentitem->appendRow(catitem);

            populateCheatListCat(catitem, cat);
        }
        else
        {
            auto& code = std::get<ARCode>(item);

            auto codeitem = new QStandardItem(QString::fromStdString(code.Name));
            codeitem->setCheckable(true);
            codeitem->setCheckState(code.Enabled ? Qt::Checked : Qt::Unchecked);
            codeitem->setDropEnabled(false);
            codeitem->setData(2, Qt::UserRole);
            codeitem->setData(QVariant::fromValue(&code), Qt::UserRole+1);
            parentitem->appendRow(codeitem);
        }
    }
}

QMimeData* CheatListModel::mimeData(const QModelIndexList& indexes) const
{
    QMimeData* mime = new QMimeData();
    QByteArray array;

    auto& index = indexes[0];
    int itemtype = index.data(Qt::UserRole).toInt();
    quintptr itemptr;

    auto parentlist = (QTreeView*)this->parent();
    unsigned char flags = (unsigned char)itemtype;
    if (parentlist->isExpanded(index))
        flags |= 0x10;

    array.append((char)flags);
    if (itemtype == 1)
    {
        auto cat = index.data(Qt::UserRole+1).value<ARCodeCat*>();
        itemptr = (quintptr)cat;

    }
    else if (itemtype == 2)
    {
        auto code = index.data(Qt::UserRole+1).value<ARCode*>();
        itemptr = (quintptr)code;
    }

    for (int i = 0; i < sizeof(quintptr); i++)
    {
        array.append((char)(itemptr & 0xFF));
        itemptr >>= 8;
    }

    mime->setData("application/x-qabstractitemmodeldatalist", array);
    return mime;
}

bool CheatListModel::canDropMimeData(const QMimeData* mime, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    if (action != Qt::MoveAction)
        return false;

    QByteArray data = mime->data("application/x-qabstractitemmodeldatalist");
    if (data.length() != (1 + sizeof(quintptr)))
        return false;

    int flags = (unsigned char)data[0];
    int itemtype = flags & 0xF;

    QStandardItem* parentitem = itemFromIndex(parent);
    if (!parentitem) parentitem = invisibleRootItem();

    if (itemtype == 1)
    {
        // if we are trying to drop a category into another category: reject this move
        if (parentitem != invisibleRootItem())
            return false;
    }

    return true;
}

bool CheatListModel::dropMimeData(const QMimeData* mime, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    if (action != Qt::MoveAction)
        return false;

    QByteArray data = mime->data("application/x-qabstractitemmodeldatalist");
    if (data.length() != (1 + sizeof(quintptr)))
        return false;

    int flags = (unsigned char)data[0];
    int itemtype = flags & 0xF;

    quintptr itemptr = 0;
    for (int i = sizeof(quintptr) - 1; i >= 0; i--)
    {
        itemptr <<= 8;
        itemptr |= (unsigned char)data[1 + i];
    }

    QStandardItem* parentitem = itemFromIndex(parent);
    if (!parentitem) parentitem = invisibleRootItem();
    if (row < 0) row = parentitem->rowCount();

    auto newparent = parentitem->data(Qt::UserRole+1).value<ARCodeCat*>();
    QStandardItem* newitem;

    if (itemtype == 1)
    {
        if (parentitem != invisibleRootItem())
            return false;

        ARCodeCat* cat = (ARCodeCat*)itemptr;
        auto oldparent = cat->Parent;

        // retrieve iterator to insert new category at
        auto newit = newparent->Children.begin();
        for (int i = 0; newit != newparent->Children.end(); i++)
        {
            if (i == row) break;
            newit++;
        }

        // create new category
        ARCodeCat newcat = {
            .Parent = newparent,
            .Name = cat->Name,
            .Description = cat->Description,
            .OnlyOneCodeEnabled = cat->OnlyOneCodeEnabled,
            .Children = cat->Children
        };

        // delete category from old parent
        for (auto it = oldparent->Children.begin(); it != oldparent->Children.end(); it++)
        {
            auto& childitem = *it;
            if (!std::holds_alternative<ARCodeCat>(childitem))
                continue;
            if (&std::get<ARCodeCat>(childitem) != cat)
                continue;

            oldparent->Children.erase(it);
            break;
        }

        // add it to new parent
        newit = newparent->Children.emplace(newit, newcat);
        ARCodeCat* newcatptr = &std::get<ARCodeCat>(*newit);

        // reparent the children
        for (auto& childitem : newcatptr->Children)
        {
            if (std::holds_alternative<ARCodeCat>(childitem))
            {
                auto& thiscat = std::get<ARCodeCat>(childitem);
                thiscat.Parent = newcatptr;
            }
            else
            {
                auto& thiscode = std::get<ARCode>(childitem);
                thiscode.Parent = newcatptr;
            }
        }

        // create and add new item
        auto catitem = new QStandardItem(QString::fromStdString(newcatptr->Name));
        catitem->setDropEnabled(true);
        catitem->setData(1, Qt::UserRole);
        catitem->setData(QVariant::fromValue(newcatptr), Qt::UserRole+1);

        parentitem->insertRow(row, catitem);
        newitem = catitem;

        populateCheatListCat(catitem, *newcatptr);
    }
    else if (itemtype == 2)
    {
        ARCode* code = (ARCode*)itemptr;
        auto oldparent = code->Parent;

        // retrieve iterator to insert new code at
        auto newit = newparent->Children.begin();
        for (int i = 0; newit != newparent->Children.end(); i++)
        {
            if (i == row) break;
            newit++;
        }

        // create new code
        ARCode newcode = {
            .Parent = newparent,
            .Name = code->Name,
            .Description = code->Description,
            .Enabled = code->Enabled,
            .Code = code->Code
        };

        // delete code from old parent
        for (auto it = oldparent->Children.begin(); it != oldparent->Children.end(); it++)
        {
            auto& childitem = *it;
            if (!std::holds_alternative<ARCode>(childitem))
                continue;
            if (&std::get<ARCode>(childitem) != code)
                continue;

            oldparent->Children.erase(it);
            break;
        }

        // add it to new parent
        newit = newparent->Children.emplace(newit, newcode);
        ARCode* newcodeptr = &std::get<ARCode>(*newit);

        // create and add new item
        auto codeitem = new QStandardItem(QString::fromStdString(newcodeptr->Name));
        codeitem->setCheckable(true);
        codeitem->setCheckState(newcodeptr->Enabled ? Qt::Checked : Qt::Unchecked);
        codeitem->setDropEnabled(false);
        codeitem->setData(2, Qt::UserRole);
        codeitem->setData(QVariant::fromValue(newcodeptr), Qt::UserRole+1);

        parentitem->insertRow(row, codeitem);
        newitem = codeitem;
    }
    else
        return false;

    auto parentlist = (QTreeView*)this->parent();
    auto newindex = newitem->index();
    if (flags & 0x10)
        parentlist->expand(newindex);

    // quality of life: select the item after it's been moved
    // we have to add a small delay here
    // if we do this immediately, it will cause the dragdrop operation to fail
    QTimer::singleShot(1, [=]()
    {
        parentlist->selectionModel()->select(newindex, QItemSelectionModel::ClearAndSelect);
        parentlist->scrollTo(newindex);
    });

    return true;
}


void ARCodeChecker::highlightBlock(const QString& text)
{
    QTextCharFormat errformat; errformat.setForeground(Qt::red);

    {
        QRegularExpression expr("^\\s*[0-9A-Fa-foO]{1,8} [0-9A-Fa-foO]{1,8}\\s*$");
        QRegularExpressionMatchIterator it = expr.globalMatch(text);
        if (!it.hasNext())
        {
            setFormat(0, text.length(), errformat);
        }
    }

    /*{
        QRegularExpression expr("[^0-9A-Fa-f\\s]+");
        QRegularExpressionMatchIterator it = expr.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), errformat);
        }
    }
    {
        QRegularExpression expr("[0-9A-Fa-f]{9,}");
        QRegularExpressionMatchIterator it = expr.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), errformat);
        }
    }*/
}
