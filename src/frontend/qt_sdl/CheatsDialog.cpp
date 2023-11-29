/*
    Copyright 2016-2023 melonDS team

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
#include "ROMManager.h"

#include "CheatsDialog.h"
#include "ui_CheatsDialog.h"

using namespace melonDS;
using Platform::Log;
using Platform::LogLevel;

CheatsDialog* CheatsDialog::currentDlg = nullptr;

extern std::string EmuDirectory;


CheatsDialog::CheatsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::CheatsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    codeFile = ROMManager::GetCheatFile();

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvCodeList->setModel(model);
    connect(model, &QStandardItemModel::itemChanged, this, &CheatsDialog::onCheatEntryModified);
    connect(ui->tvCodeList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CheatsDialog::onCheatSelectionChanged);

    {
        QStandardItem* root = model->invisibleRootItem();

        for (ARCodeCatList::iterator i = codeFile->Categories.begin(); i != codeFile->Categories.end(); i++)
        {
            ARCodeCat& cat = *i;

            QStandardItem* catitem = new QStandardItem(QString::fromStdString(cat.Name));
            catitem->setEditable(true);
            catitem->setData(QVariant::fromValue(i));
            root->appendRow(catitem);

            for (ARCodeList::iterator j = cat.Codes.begin(); j != cat.Codes.end(); j++)
            {
                ARCode& code = *j;

                QStandardItem* codeitem = new QStandardItem(QString::fromStdString(code.Name));
                codeitem->setEditable(true);
                codeitem->setCheckable(true);
                codeitem->setCheckState(code.Enabled ? Qt::Checked : Qt::Unchecked);
                codeitem->setData(QVariant::fromValue(j));
                catitem->appendRow(codeitem);
            }
        }
    }

    ui->txtCode->setPlaceholderText("");
    codeChecker = new ARCodeChecker(ui->txtCode->document());

    ui->btnNewARCode->setEnabled(false);
    ui->btnDeleteCode->setEnabled(false);
    ui->txtCode->setEnabled(false);
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
    catitem->setEditable(true);
    catitem->setData(QVariant::fromValue(id));
    root->appendRow(catitem);

    ui->tvCodeList->selectionModel()->select(catitem->index(), QItemSelectionModel::ClearAndSelect);
    ui->tvCodeList->edit(catitem->index());
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

    QVariant data = item->data();
    if (data.canConvert<ARCodeCatList::iterator>())
    {
        ARCodeCatList::iterator it_cat = data.value<ARCodeCatList::iterator>();

        (*it_cat).Codes.clear();
        codeFile->Categories.erase(it_cat);

        model->invisibleRootItem()->removeRow(item->row());
    }
    else if (data.canConvert<ARCodeList::iterator>())
    {
        ARCodeList::iterator it_code = data.value<ARCodeList::iterator>();
        ARCodeCatList::iterator it_cat = item->parent()->data().value<ARCodeCatList::iterator>();

        (*it_cat).Codes.erase(it_code);

        item->parent()->removeRow(item->row());
    }
}

void CheatsDialog::onCheatSelectionChanged(const QItemSelection& sel, const QItemSelection& desel)
{
    QModelIndexList indices = sel.indexes();
    if (indices.isEmpty())
    {
        ui->btnNewARCode->setEnabled(false);
        ui->btnDeleteCode->setEnabled(false);
        ui->txtCode->setEnabled(false);
        ui->txtCode->setPlaceholderText("");
        ui->txtCode->clear();
    }
    else
    {
        QStandardItem* item = ((QStandardItemModel*)ui->tvCodeList->model())->itemFromIndex(indices.first());

        QVariant data = item->data();
        if (data.canConvert<ARCodeCatList::iterator>())
        {
            ui->btnDeleteCode->setEnabled(true);
            ui->txtCode->setEnabled(false);
            ui->txtCode->setPlaceholderText("");
            ui->txtCode->clear();
        }
        else if (data.canConvert<ARCodeList::iterator>())
        {
            ARCode& code = *(data.value<ARCodeList::iterator>());

            ui->btnDeleteCode->setEnabled(true);
            ui->txtCode->setEnabled(true);
            ui->txtCode->setPlaceholderText("(enter AR code here)");

            QString codestr = "";
            for (size_t i = 0; i < code.Code.size(); i += 2)
            {
                u32 c0 = code.Code[i+0];
                u32 c1 = code.Code[i+1];
                //codestr += QString("%1 %2\n").arg(c0, 8, 16, '0').arg(c1, 8, 16, '0').toUpper();
                codestr += QString::asprintf("%08X %08X\n", c0, c1);
            }
            ui->txtCode->setPlainText(codestr);
        }

        ui->btnNewARCode->setEnabled(true);
    }
}

void CheatsDialog::onCheatEntryModified(QStandardItem* item)
{
    QVariant data = item->data();
    if (data.canConvert<ARCodeCatList::iterator>())
    {
        ARCodeCat& cat = *(data.value<ARCodeCatList::iterator>());

        if (item->text().isEmpty())
        {
            QString oldname = QString::fromStdString(cat.Name);
            item->setText(oldname.isEmpty() ? "(blank category name?)" : oldname);
        }
        else
        {
            cat.Name = item->text().toStdString();
        }
    }
    else if (data.canConvert<ARCodeList::iterator>())
    {
        ARCode& code = *(data.value<ARCodeList::iterator>());

        if (item->text().isEmpty())
        {
            QString oldname = QString::fromStdString(code.Name);
            item->setText(oldname.isEmpty() ? "(blank code name?)" : oldname);
        }
        else
        {
            code.Name = item->text().toStdString();
        }

        code.Enabled = (item->checkState() == Qt::Checked);
    }
}

void CheatsDialog::on_txtCode_textChanged()
{
    QModelIndexList indices = ui->tvCodeList->selectionModel()->selectedIndexes();
    if (indices.isEmpty())
        return;

    QStandardItem* item = ((QStandardItemModel*)ui->tvCodeList->model())->itemFromIndex(indices.first());
    QVariant data = item->data();
    if (!data.canConvert<ARCodeList::iterator>())
        return;

    bool error = false;
    std::vector<u32> codeout;
    codeout.reserve(64);

    QString text = ui->txtCode->document()->toPlainText();
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
            error = true;
            break;
        }

        QStringList numbers = line.split(' ');
        if (numbers.length() != 2)
        {
            error = true;
            break;
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
            error = true;
            break;
        }

        codeout.push_back(c0);
        codeout.push_back(c1);
    }

    ui->btnNewCat->setEnabled(!error);
    ui->btnNewARCode->setEnabled(!error);
    ui->btnDeleteCode->setEnabled(!error);
    ui->tvCodeList->setEnabled(!error);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!error);

    if (error) return;

    ARCode& code = *(data.value<ARCodeList::iterator>());
    code.Code = codeout;
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
