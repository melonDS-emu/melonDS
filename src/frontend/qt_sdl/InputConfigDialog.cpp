/*
    Copyright 2016-2020 Arisotura

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

//

#include "types.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "InputConfigDialog.h"
#include "ui_InputConfigDialog.h"


InputConfigDialog* InputConfigDialog::currentDlg = nullptr;


InputConfigDialog::InputConfigDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InputConfigDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    //
}

InputConfigDialog::~InputConfigDialog()
{
    delete ui;
}

void InputConfigDialog::on_InputConfigDialog_accepted()
{
    closeDlg();
}

void InputConfigDialog::on_InputConfigDialog_rejected()
{
    closeDlg();
}
