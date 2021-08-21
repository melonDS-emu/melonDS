/*
    Copyright 2016-2021 Arisotura

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
#include <QMessageBox>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "TitleManagerDialog.h"
#include "ui_TitleManagerDialog.h"


TitleManagerDialog* TitleManagerDialog::currentDlg = nullptr;


TitleManagerDialog::TitleManagerDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TitleManagerDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    //ui->lstTitleList->setViewMode(QListView::IconMode);
    //ui->lstTitleList->setFlow(QListView::LeftToRight);
    ui->lstTitleList->setIconSize(QSize(32, 32));

    {
        QPixmap boobs(32, 32);
        boobs.fill(Qt::blue);
        QIcon piss(boobs);

        QListWidgetItem* derp = new QListWidgetItem("完全放棄宣言\nナナヲアカリ");
        derp->setIcon(piss);
        ui->lstTitleList->addItem(derp);
    }
    {
        QPixmap boobs(32, 32);
        boobs.fill(Qt::red);
        QIcon piss(boobs);

        QListWidgetItem* derp = new QListWidgetItem("death to\ncapitalism");
        derp->setIcon(piss);
        ui->lstTitleList->addItem(derp);
    }
    {
        QPixmap boobs(32, 32);
        boobs.fill(Qt::green);
        QIcon piss(boobs);

        QListWidgetItem* derp = new QListWidgetItem("piles of\ncontent");
        derp->setIcon(piss);
        ui->lstTitleList->addItem(derp);
    }
    {
        QPixmap boobs(32, 32);
        boobs.fill(Qt::yellow);
        QIcon piss(boobs);

        QListWidgetItem* derp = new QListWidgetItem("trans\nrights");
        derp->setIcon(piss);
        ui->lstTitleList->addItem(derp);
    }
}

TitleManagerDialog::~TitleManagerDialog()
{
    delete ui;
}
