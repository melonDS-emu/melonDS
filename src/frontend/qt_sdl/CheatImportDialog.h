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
#include <QAbstractItemModel>

#include "ARCodeFile.h"

namespace Ui { class CheatImportDialog; }
class CheatImportDialog;

class EmuInstance;

namespace melonDS
{
    class ARDatabaseDAT;
}

class CheatImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheatImportDialog(QWidget* parent);
    ~CheatImportDialog();

private slots:
    void accept() override;

private:
    Ui::CheatImportDialog* ui;
    melonDS::ARDatabaseDAT* database;
};

#endif // CHEATIMPORTDIALOG_H
