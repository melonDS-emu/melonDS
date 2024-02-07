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

#ifndef CHEATSDIALOG_H
#define CHEATSDIALOG_H

#include <QDialog>
#include <QAbstractItemModel>
#include <QStandardItemModel>
#include <QItemSelection>
#include <QSyntaxHighlighter>

#include "ARCodeFile.h"

Q_DECLARE_METATYPE(melonDS::ARCodeList::iterator)
Q_DECLARE_METATYPE(melonDS::ARCodeCatList::iterator)

namespace Ui { class CheatsDialog; }
class CheatsDialog;

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
    void on_CheatsDialog_accepted();
    void on_CheatsDialog_rejected();

    void on_btnNewCat_clicked();
    void on_btnNewARCode_clicked();
    void on_btnDeleteCode_clicked();

    void onCheatSelectionChanged(const QItemSelection& sel, const QItemSelection& desel);
    void onCheatEntryModified(QStandardItem* item);

    void on_txtCode_textChanged();

private:
    Ui::CheatsDialog* ui;

    melonDS::ARCodeFile* codeFile;
    ARCodeChecker* codeChecker;
};

#endif // CHEATSDIALOG_H
