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

#include <stdio.h>
#include <QFileDialog>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "VideoSettingsDialog.h"
#include "ui_VideoSettingsDialog.h"


VideoSettingsDialog* VideoSettingsDialog::currentDlg = nullptr;


VideoSettingsDialog::VideoSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    //

    grp3DRenderer = new QButtonGroup(this);
    grp3DRenderer->addButton(ui->rb3DSoftware, 0);
    grp3DRenderer->addButton(ui->rb3DOpenGL,   1);
    //connect(grp3DRenderer, SIGNAL(buttonClicked(int)), this, SLOT(onChange3DRenderer(int)));
}

VideoSettingsDialog::~VideoSettingsDialog()
{
    delete ui;
}

void VideoSettingsDialog::on_VideoSettingsDialog_accepted()
{
    //
    Config::Save();

    closeDlg();
}

void VideoSettingsDialog::on_VideoSettingsDialog_rejected()
{
    //

    closeDlg();
}

//
