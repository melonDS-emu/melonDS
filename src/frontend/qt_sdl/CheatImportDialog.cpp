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

    // TODO this could be a base class thing and support multiple types of database
    // also maybe don't hardcode the filename
    database = new ARDatabaseDAT("usrcheat.dat");
    if (database->Error)
    {
        printf("database shat itself\n");
        return;
    }

    // test
    database->Test();
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
