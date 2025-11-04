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

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvCodeList->setModel(model);
    connect(model, &QStandardItemModel::itemChanged, this, &CheatsDialog::onCheatEntryModified);
    connect(ui->tvCodeList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CheatsDialog::onCheatSelectionChanged);

    populateCheatList();

    //ui->txtCode->setPlaceholderText("");
    codeChecker = new ARCodeChecker(ui->txtCode->document());

    ui->btnNewARCode->setEnabled(false);
    ui->btnDeleteCode->setEnabled(false);
}

CheatsDialog::~CheatsDialog()
{
    QAbstractItemModel* model = ui->tvCodeList->model();
    ui->tvCodeList->setModel(nullptr);
    delete model;

    delete codeChecker;

    delete ui;
}

void CheatsDialog::on_CheatsDialog_accepted()
{
    codeFile->Save();

    closeDlg();
}

void CheatsDialog::on_CheatsDialog_rejected()
{
    codeFile->Load();

    closeDlg();
}

void CheatsDialog::on_btnNewCat_clicked()
{
    QStandardItem* root = ((QStandardItemModel*)ui->tvCodeList->model())->invisibleRootItem();

    ARCodeCat cat;
    cat.Codes.clear();
    cat.Name = "(new category)";

    codeFile->Categories.push_back(cat);
    ARCodeCatList::iterator id = codeFile->Categories.end(); id--;

    QStandardItem* catitem = new QStandardItem(QString::fromStdString(cat.Name));
    //catitem->setEditable(true);
    catitem->setData(QVariant::fromValue(id));
    root->appendRow(catitem);

    ui->tvCodeList->selectionModel()->select(catitem->index(), QItemSelectionModel::ClearAndSelect);
    ui->tvCodeList->scrollTo(catitem->index());
    //ui->tvCodeList->edit(catitem->index());
}

void CheatsDialog::on_btnNewARCode_clicked()
{
    QModelIndexList indices = ui->tvCodeList->selectionModel()->selectedIndexes();
    if (indices.isEmpty())
    {
        // ????
        return;
    }

    QStandardItemModel* model = (QStandardItemModel*)ui->tvCodeList->model();
    QStandardItem* item = model->itemFromIndex(indices.first());
    QStandardItem* parentitem;

    QVariant data = item->data();
    if (data.canConvert<ARCodeCatList::iterator>())
    {
        parentitem = item;
    }
    else if (data.canConvert<ARCodeList::iterator>())
    {
        parentitem = item->parent();
    }
    else
    {
        Log(LogLevel::Warn, "what?? :(\n");
        return;
    }

    ARCodeCatList::iterator it_cat = parentitem->data().value<ARCodeCatList::iterator>();
    ARCodeCat& cat = *it_cat;

    ARCode code;
    code.Name = "(new AR code)";
    code.Enabled = true;
    code.Code.clear();

    cat.Codes.push_back(code);
    ARCodeList::iterator id = cat.Codes.end(); id--;

    QStandardItem* codeitem = new QStandardItem(QString::fromStdString(code.Name));
    codeitem->setEditable(true);
    codeitem->setCheckable(true);
    codeitem->setCheckState(code.Enabled ? Qt::Checked : Qt::Unchecked);
    codeitem->setData(QVariant::fromValue(id));
    parentitem->appendRow(codeitem);

    ui->tvCodeList->selectionModel()->select(codeitem->index(), QItemSelectionModel::ClearAndSelect);
    ui->tvCodeList->edit(codeitem->index());
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

    QStandardItemModel* model = (QStandardItemModel*)ui->tvCodeList->model();
    QStandardItem* item = model->itemFromIndex(indices.first());

    QStandardItem* itemparent = item->parent();
    if (!itemparent) itemparent = model->invisibleRootItem();

    QVariant data = item->data();
    if (data.canConvert<ARCodeCatList::iterator>())
    {
        ARCodeCatList::iterator it_cat = data.value<ARCodeCatList::iterator>();

        (*it_cat).Codes.clear();
        codeFile->Categories.erase(it_cat);

        itemparent->removeRow(item->row());
    }
    else if (data.canConvert<ARCodeList::iterator>())
    {
        ARCodeList::iterator it_code = data.value<ARCodeList::iterator>();
        auto cat = (*it_code).Parent;

        cat->Codes.erase(it_code);

        itemparent->removeRow(item->row());
    }
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

    QVariant data = item->data();
    if (data.canConvert<ARCodeList::iterator>())
    {
        ARCode& code = *(data.value<ARCodeList::iterator>());

        code.Enabled = (item->checkState() == Qt::Checked);

        if (code.Enabled && code.Parent->OnlyOneCodeEnabled)
        {
            // if needed, disable other codes

            updatingEnableChk = true;

            auto parent = item->parent();
            for (int i = 0; i < parent->rowCount(); i++)
            {
                auto child = parent->child(i);
                if (child == item) continue;

                QVariant childdata = child->data();
                if (childdata.canConvert<ARCodeList::iterator>())
                {
                    ARCode& childcode = *(childdata.value<ARCodeList::iterator>());
                    childcode.Enabled = false;

                    child->setCheckState(Qt::Unchecked);
                }
            }

            updatingEnableChk = false;
        }

        // if this item is selected, reflect the checkbox change
        auto selmodel = ui->tvCodeList->selectionModel();
        if (selmodel->hasSelection())
        {
            auto index = selmodel->selectedIndexes()[0];
            if (index == item->index())
            {
                ui->chkItemOption->setChecked(code.Enabled);
            }
        }
    }
}

void CheatsDialog::on_btnEditCode_clicked()
{
    ui->tvCodeList->setEnabled(false);

    ui->btnEditCode->hide();
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
    QVariant data = index.data(Qt::UserRole + 1);

    int valres = validateInput(data.canConvert<ARCodeList::iterator>());
    if (valres < 0)
    {
        QString errmsg = "Error: invalid input.";
        switch (valres)
        {
            case -1: errmsg = "Error: no name entered."; break;
            case -2: errmsg = "Error: no code entered."; break;
            case -3: errmsg = "Error: the code entered is invalid."; break;
        }

        QMessageBox::critical(this, "melonDS", errmsg);
        return;
    }

    if (data.canConvert<ARCodeCatList::iterator>())
    {
        ARCodeCat& cat = *(data.value<ARCodeCatList::iterator>());

        cat.Name = ui->txtItemName->text().toStdString();
        cat.Description = ui->txtItemDesc->text().toStdString();
        cat.OnlyOneCodeEnabled = ui->chkItemOption->isChecked();

        auto tvmodel = (QStandardItemModel*)ui->tvCodeList->model();
        auto tvitem = tvmodel->itemFromIndex(index);
        tvitem->setText(ui->txtItemName->text());
    }
    else if (data.canConvert<ARCodeList::iterator>())
    {
        ARCode& code = *(data.value<ARCodeList::iterator>());

        auto codeconv = convertCodeInput();
        if (codeconv.empty())
        {
            QMessageBox::critical(this, "melonDS", "Error: the code entered is empty or invalid.");
            return;
        }

        code.Name = ui->txtItemName->text().toStdString();
        code.Description = ui->txtItemDesc->text().toStdString();
        code.Enabled = ui->chkItemOption->isChecked();
        code.Code = codeconv;

        updatingEnableChk = true;

        auto tvmodel = (QStandardItemModel*)ui->tvCodeList->model();
        auto tvitem = tvmodel->itemFromIndex(index);
        tvitem->setText(ui->txtItemName->text());
        tvitem->setCheckState(code.Enabled ? Qt::Checked : Qt::Unchecked);

        updatingEnableChk = false;
    }

    ui->tvCodeList->setEnabled(true);

    ui->btnEditCode->show();
    ui->btnSaveCode->hide();
    ui->btnCancelEdit->hide();

    ui->txtItemName->setReadOnly(true);
    ui->txtItemDesc->setReadOnly(true);
    ui->chkItemOption->setEnabled(false);
    ui->txtCode->setReadOnly(true);

    // TODO actually save shit?
}

void CheatsDialog::on_btnCancelEdit_clicked()
{
    ui->tvCodeList->setEnabled(true);

    ui->btnEditCode->show();
    ui->btnSaveCode->hide();
    ui->btnCancelEdit->hide();

    populateCheatInfo();
}

void CheatsDialog::populateCheatList()
{
    auto model = (QStandardItemModel*)ui->tvCodeList->model();
    model->clear();

    QStandardItem* root = model->invisibleRootItem();

    for (auto i = codeFile->Categories.begin(); i != codeFile->Categories.end(); i++)
    {
        ARCodeCat& cat = *i;

        QStandardItem* catitem;
        if (cat.IsRoot)
        {
            catitem = root;
        }
        else
        {
            catitem = new QStandardItem(QString::fromStdString(cat.Name));
            catitem->setData(QVariant::fromValue(i));
            root->appendRow(catitem);
        }

        for (auto j = cat.Codes.begin(); j != cat.Codes.end(); j++)
        {
            ARCode& code = *j;

            auto codeitem = new QStandardItem(QString::fromStdString(code.Name));
            codeitem->setCheckable(true);
            codeitem->setCheckState(code.Enabled ? Qt::Checked : Qt::Unchecked);
            codeitem->setData(QVariant::fromValue(j));
            catitem->appendRow(codeitem);
        }
    }

    populateCheatInfo();
}

void CheatsDialog::populateCheatInfo()
{
    auto selmodel = ui->tvCodeList->selectionModel();
    if (!selmodel->hasSelection())
    {
        ui->splitter->widget(1)->setEnabled(false);

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
    QVariant data = index.data(Qt::UserRole + 1);
    if (data.canConvert<ARCodeCatList::iterator>())
    {
        ARCodeCat& cat = *(data.value<ARCodeCatList::iterator>());

        ui->btnDeleteCode->setEnabled(true);
        ui->btnEditCode->show();
        ui->btnEditCode->setEnabled(true);
        ui->btnSaveCode->hide();
        ui->btnCancelEdit->hide();

        ui->txtItemName->setText(QString::fromStdString(cat.Name));
        ui->txtItemName->setReadOnly(true);
        ui->txtItemDesc->setText(QString::fromStdString(cat.Description));
        ui->txtItemDesc->setReadOnly(true);

        ui->chkItemOption->setText("Only allow one code enabled");
        ui->chkItemOption->setChecked(cat.OnlyOneCodeEnabled);
        ui->chkItemOption->setEnabled(false);

        ui->lblCode->hide();
        ui->txtCode->hide();
    }
    else if (data.canConvert<ARCodeList::iterator>())
    {
        ARCode& code = *(data.value<ARCodeList::iterator>());

        ui->btnDeleteCode->setEnabled(true);
        ui->btnEditCode->show();
        ui->btnEditCode->setEnabled(true);
        ui->btnSaveCode->hide();
        ui->btnCancelEdit->hide();

        ui->txtItemName->setText(QString::fromStdString(code.Name));
        ui->txtItemName->setReadOnly(true);
        ui->txtItemDesc->setText(QString::fromStdString(code.Description));
        ui->txtItemDesc->setReadOnly(true);

        ui->chkItemOption->setText("Enabled");
        ui->chkItemOption->setChecked(code.Enabled);
        ui->chkItemOption->setEnabled(false);

        ui->lblCode->show();
        ui->txtCode->show();

        QString codestr = "";
        for (size_t i = 0; i < code.Code.size(); i += 2)
        {
            u32 c0 = code.Code[i+0];
            u32 c1 = code.Code[i+1];
            //codestr += QString("%1 %2\n").arg(c0, 8, 16, '0').arg(c1, 8, 16, '0').toUpper();
            codestr += QString::asprintf("%08X %08X\n", c0, c1);
        }
        ui->txtCode->setPlainText(codestr);
        ui->txtCode->setReadOnly(true);
    }
}

int CheatsDialog::validateInput(bool iscode)
{
    if (ui->txtItemName->text().trimmed().isEmpty())
        return -1;

    if (!iscode)
        return 0;

    QString text = ui->txtCode->toPlainText();
    if (text.trimmed().isEmpty())
        return -2;

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
            return -3;
        }

        QStringList numbers = line.split(' ');
        if (numbers.length() != 2)
        {
            return -3;
        }

        QStringList::iterator jt = numbers.begin();
        QString s0 = *jt++;
        QString s1 = *jt++;

        bool c0good, c1good;

        s0.toUInt(&c0good, 16);
        s1.toUInt(&c1good, 16);

        if (!c0good || !c1good)
        {
            return -3;
        }
    }

    return 0;
}

std::vector<u32> CheatsDialog::convertCodeInput()
{
    std::vector<u32> codeout;

    QString text = ui->txtCode->toPlainText();
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

void ARCodeChecker::highlightBlock(const QString& text)
{
    QTextCharFormat errformat; errformat.setForeground(Qt::red);

    {
        QRegularExpression expr("^\\s*[0-9A-Fa-f]{1,8} [0-9A-Fa-f]{1,8}\\s*$");
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
