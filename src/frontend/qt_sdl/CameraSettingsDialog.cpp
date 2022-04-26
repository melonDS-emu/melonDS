/*
    Copyright 2016-2022 melonDS team

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
#include <QFileDialog>

#include "types.h"
#include "Platform.h"
#include "Config.h"

#include "CameraSettingsDialog.h"
#include "ui_CameraSettingsDialog.h"


CameraSettingsDialog* CameraSettingsDialog::currentDlg = nullptr;

extern std::string EmuDirectory;


CameraSettingsDialog::CameraSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::CameraSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    //
}

CameraSettingsDialog::~CameraSettingsDialog()
{
    delete ui;
}

void CameraSettingsDialog::on_CameraSettingsDialog_accepted()
{
    //
    Config::Save();

    closeDlg();
}

void CameraSettingsDialog::on_CameraSettingsDialog_rejected()
{
    //

    closeDlg();
}

//
