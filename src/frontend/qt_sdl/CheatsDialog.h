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

#ifndef CHEATSDIALOG_H
#define CHEATSDIALOG_H

#include <QDialog>
#include <QAbstractItemModel>
#include <QStandardItemModel>
#include <QItemSelection>
#include <QSyntaxHighlighter>

#include "ARCodeFile.h"

namespace Ui { class CheatsDialog; }
class CheatsDialog;
class CheatImportDialog;

class EmuInstance;

namespace melonDS
{
    class ARDatabaseDAT;
}

class CheatListModel : public QStandardItemModel
{
    Q_OBJECT

public:
    CheatListModel(QObject* parent = nullptr) : QStandardItemModel(parent) {}

    void populateCheatListCat(QStandardItem* parentitem, melonDS::ARCodeCat& parentcat);

    Qt::DropActions supportedDragActions() const override { return Qt::MoveAction; }
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }

    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* mime, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* mime, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
};

class ARCodeChecker : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    ARCodeChecker(QTextDocument* parent) : QSyntaxHighlighter(parent) {}
    ~ARCodeChecker() {}

protected:
    void highlightBlock(const QString& text) override;
};

class CheatsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheatsDialog(QWidget* parent);
    ~CheatsDialog();

    static CheatsDialog* currentDlg;
    static CheatsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new CheatsDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void done(int r) override;

    void on_btnNewCat_clicked();
    void on_btnNewARCode_clicked();
    void on_btnDeleteCode_clicked();
    void on_btnImportCheats_clicked();
    void onImportCheatsFinished(int res);

    void onCheatSelectionChanged(const QItemSelection& sel, const QItemSelection& desel);
    void onCheatEntryModified(QStandardItem* item);

    void on_btnEditCode_clicked();
    void on_btnSaveCode_clicked();
    void on_btnCancelEdit_clicked();

private:
    Ui::CheatsDialog* ui;

    EmuInstance* emuInstance;

    melonDS::u32 gameCode;
    melonDS::u32 gameChecksum;

    melonDS::ARCodeFile* codeFile;
    ARCodeChecker* codeChecker;

    melonDS::ARDatabaseDAT* importDB;
    CheatImportDialog* importDlg;

    bool updatingEnableChk;

    void populateCheatList();
    void populateCheatInfo();
    std::vector<melonDS::u32> convertCodeInput();
};

#endif // CHEATSDIALOG_H
