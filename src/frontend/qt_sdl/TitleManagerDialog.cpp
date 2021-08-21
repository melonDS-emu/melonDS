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

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"
#include "FrontendUtil.h"
#include "DSi_NAND.h"

#include "TitleManagerDialog.h"
#include "ui_TitleManagerDialog.h"


FILE* TitleManagerDialog::curNAND = nullptr;
TitleManagerDialog* TitleManagerDialog::currentDlg = nullptr;


TitleManagerDialog::TitleManagerDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TitleManagerDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->lstTitleList->setIconSize(QSize(32, 32));

    const u32 category = 0x00030004;
    std::vector<u32> titlelist;
    DSi_NAND::ListTitles(category, titlelist);

    for (std::vector<u32>::iterator it = titlelist.begin(); it != titlelist.end(); it++)
    {
        u32 titleid = *it;

        u32 version;
        u8 header[0x1000];
        u8 banner[0x2400];

        DSi_NAND::GetTitleInfo(category, titleid, version, header, banner);

        u8 icongfx[512];
        u16 iconpal[16];
        memcpy(icongfx, &banner[0x20], 512);
        memcpy(iconpal, &banner[0x220], 16*2);
        u32 icondata[32*32];
        Frontend::ROMIcon(icongfx, iconpal, icondata);
        QImage iconimg((const uchar*)icondata, 32, 32, QImage::Format_ARGB32);
        QIcon icon(QPixmap::fromImage(iconimg.copy()));

        // TODO: make it possible to select other languages?
        u16 titleraw[129];
        memcpy(titleraw, &banner[0x340], 128*sizeof(u16));
        titleraw[128] = '\0';
        QString title = QString::fromUtf16(titleraw);
        title.replace("\n", " · ");

        char gamecode[5];
        *(u32*)&gamecode[0] = *(u32*)&header[0xC];
        gamecode[4] = '\0';
        char extra[128];
        sprintf(extra, "\n(title ID: %s · %08x/%08x · version %08x)", gamecode, category, titleid, version);

        QListWidgetItem* item = new QListWidgetItem(title + QString(extra));
        item->setIcon(icon);
        ui->lstTitleList->addItem(item);
    }

    /*{
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
    }*/
}

TitleManagerDialog::~TitleManagerDialog()
{
    delete ui;
}

bool TitleManagerDialog::openNAND()
{
    FILE* bios7i = Platform::OpenLocalFile(Config::DSiBIOS7Path, "rb");
    if (!bios7i)
        return false;

    u8 es_keyY[16];
    fseek(bios7i, 0x8308, SEEK_SET);
    fread(es_keyY, 16, 1, bios7i);
    fclose(bios7i);

    curNAND = Platform::OpenLocalFile(Config::DSiNANDPath, "r+b");
    if (!curNAND)
        return false;

    if (!DSi_NAND::Init(curNAND, es_keyY))
    {
        fclose(curNAND);
        curNAND = nullptr;
        return false;
    }

    return true;
}

void TitleManagerDialog::closeNAND()
{
    if (curNAND)
    {
        DSi_NAND::DeInit();

        fclose(curNAND);
        curNAND = nullptr;
    }
}

void TitleManagerDialog::done(int r)
{
    QDialog::done(r);

    closeDlg();
}
