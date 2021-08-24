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
#include <QFileDialog>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"
#include "FrontendUtil.h"
#include "DSi_NAND.h"

#include "TitleManagerDialog.h"
#include "ui_TitleManagerDialog.h"
#include "ui_TitleImportDialog.h"


FILE* TitleManagerDialog::curNAND = nullptr;
TitleManagerDialog* TitleManagerDialog::currentDlg = nullptr;

extern char* EmuDirectory;


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
        createTitleItem(category, titleid);
    }

    ui->lstTitleList->sortItems();

    ui->btnDeleteTitle->setEnabled(false);
}

TitleManagerDialog::~TitleManagerDialog()
{
    delete ui;
}

void TitleManagerDialog::createTitleItem(u32 category, u32 titleid)
{
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
    item->setData(Qt::UserRole, (((u64)category<<32) | (u64)titleid));
    ui->lstTitleList->addItem(item);
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

void TitleManagerDialog::on_btnImportTitle_clicked()
{
    TitleImportDialog* importdlg = new TitleImportDialog(this, importAppPath, importTmdData, importReadOnly);
    importdlg->open();
    connect(importdlg, &TitleImportDialog::finished, this, &TitleManagerDialog::onImportTitleFinished);

    importdlg->show();
}

void TitleManagerDialog::onImportTitleFinished(int res)
{
    if (res != QDialog::Accepted) return;

    u32 titleid[2];
    titleid[0] = (importTmdData[0x18C] << 24) | (importTmdData[0x18D] << 16) | (importTmdData[0x18E] << 8) | importTmdData[0x18F];
    titleid[1] = (importTmdData[0x190] << 24) | (importTmdData[0x191] << 16) | (importTmdData[0x192] << 8) | importTmdData[0x193];

    bool importres = DSi_NAND::ImportTitle(importAppPath.toStdString().c_str(), importTmdData, importReadOnly);
    if (!importres)
    {
        // remove a potential half-completed install
        DSi_NAND::DeleteTitle(titleid[0], titleid[1]);

        QMessageBox::critical(this,
                              "Import title - melonDS",
                              "An error occured while installing the title to the NAND.\nCheck that your NAND dump is valid.");
    }
    else
    {
        // it worked, wee!
        createTitleItem(titleid[0], titleid[1]);
        ui->lstTitleList->sortItems();
    }
}

void TitleManagerDialog::on_btnExportTitle_clicked()
{
    //
}

void TitleManagerDialog::on_btnDeleteTitle_clicked()
{
    QListWidgetItem* cur = ui->lstTitleList->currentItem();
    if (!cur) return;

    if (QMessageBox::question(this,
                              "Delete title - melonDS",
                              "The title and its associated data will be permanently deleted. Are you sure?",
                              QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No),
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    u64 titleid = cur->data(Qt::UserRole).toLongLong();
    DSi_NAND::DeleteTitle((u32)(titleid >> 32), (u32)titleid);

    delete cur;
}

void TitleManagerDialog::on_lstTitleList_currentItemChanged(QListWidgetItem* cur, QListWidgetItem* prev)
{
    if (!cur)
    {
        ui->btnDeleteTitle->setEnabled(false);
    }
    else
    {
        ui->btnDeleteTitle->setEnabled(true);
    }
}


TitleImportDialog::TitleImportDialog(QWidget* parent, QString& apppath, u8* tmd, bool& readonly)
: QDialog(parent), ui(new Ui::TitleImportDialog), appPath(apppath), tmdData(tmd), readOnly(readonly)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    grpTmdSource = new QButtonGroup(this);
    grpTmdSource->addButton(ui->rbTmdFromFile, 0);
    grpTmdSource->addButton(ui->rbTmdFromNUS, 1);
    connect(grpTmdSource, SIGNAL(buttonClicked(int)), this, SLOT(onChangeTmdSource(int)));
    grpTmdSource->button(0)->setChecked(true);
}

TitleImportDialog::~TitleImportDialog()
{
    delete ui;
}

void TitleImportDialog::accept()
{
    QString path;
    FILE* f;

    bool tmdfromfile = (grpTmdSource->checkedId() == 0);

    path = ui->txtAppFile->text();
    f = fopen(path.toStdString().c_str(), "rb");
    if (!f)
    {
        QMessageBox::critical(this,
                              "Import title - melonDS",
                              "Could not open executable file.\nCheck that the path is correct and that the file is accessible.");
        return;
    }

    fseek(f, 0x230, SEEK_SET);
    fread(titleid, 8, 1, f);
    fclose(f);

    if (titleid[1] != 0x00030004)
    {
        QMessageBox::critical(this,
                              "Import title - melonDS",
                              "Executable file is not a DSiware title.");
        return;
    }

    if (tmdfromfile)
    {
        path = ui->txtTmdFile->text();
        f = fopen(path.toStdString().c_str(), "rb");
        if (!f)
        {
            QMessageBox::critical(this,
                                  "Import title - melonDS",
                                  "Could not open metadata file.\nCheck that the path is correct and that the file is accessible.");
            return;
        }

        fread(tmdData, 0x208, 1, f);
        fclose(f);

        u32 tmdtitleid[2];
        tmdtitleid[0] = (tmdData[0x18C] << 24) | (tmdData[0x18D] << 16) | (tmdData[0x18E] << 8) | tmdData[0x18F];
        tmdtitleid[1] = (tmdData[0x190] << 24) | (tmdData[0x191] << 16) | (tmdData[0x192] << 8) | tmdData[0x193];

        if (tmdtitleid[1] != titleid[0] || tmdtitleid[0] != titleid[1])
        {
            QMessageBox::critical(this,
                                  "Import title - melonDS",
                                  "Title ID in metadata file does not match executable file.");
            return;
        }
    }

    if (DSi_NAND::TitleExists(titleid[1], titleid[0]))
    {
        if (QMessageBox::question(this,
                                  "Import title - melonDS",
                                  "The selected title is already installed. Overwrite it?",
                                  QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No),
                                  QMessageBox::No) != QMessageBox::Yes)
            return;

        DSi_NAND::DeleteTitle(titleid[1], titleid[0]);
    }

    if (!tmdfromfile)
    {
        network = new QNetworkAccessManager(this);

        char url[256];
        sprintf(url, "http://nus.cdn.t.shop.nintendowifi.net/ccs/download/%08x%08x/tmd", titleid[1], titleid[0]);

        QNetworkRequest req;
        req.setUrl(QUrl(url));

        netreply = network->get(req);
        connect(netreply, &QNetworkReply::finished, this, &TitleImportDialog::tmdDownloaded);

        setEnabled(false);
    }
}

void TitleImportDialog::tmdDownloaded()
{
    bool good = false;

    if (netreply->error() != QNetworkReply::NoError)
    {
        QMessageBox::critical(this,
                              "Import title - melonDS",
                              QString("An error occurred while trying to download the metadata file:\n\n") + netreply->errorString());
    }
    else if (netreply->bytesAvailable() < 2312)
    {
        QMessageBox::critical(this,
                              "Import title - melonDS",
                              "NUS returned a malformed metadata file.");
    }
    else
    {
        netreply->read((char*)tmdData, 520);

        u32 tmdtitleid[2];
        tmdtitleid[0] = (tmdData[0x18C] << 24) | (tmdData[0x18D] << 16) | (tmdData[0x18E] << 8) | tmdData[0x18F];
        tmdtitleid[1] = (tmdData[0x190] << 24) | (tmdData[0x191] << 16) | (tmdData[0x192] << 8) | tmdData[0x193];

        if (tmdtitleid[1] != titleid[0] || tmdtitleid[0] != titleid[1])
        {
            QMessageBox::critical(this,
                                  "Import title - melonDS",
                                  "NUS returned a malformed metadata file.");
        }
        else
            good = true;
    }

    netreply->deleteLater();
    setEnabled(true);

    if (good)
    {
        appPath = ui->txtAppFile->text();
        readOnly = ui->cbReadOnly->isChecked();
        QDialog::accept();
    }
}

void TitleImportDialog::on_TitleImportDialog_accepted()
{
    setEnabled(false);
}

void TitleImportDialog::on_TitleImportDialog_rejected()
{
    printf("rejected\n");
}

void TitleImportDialog::on_TitleImportDialog_reject()
{
    printf("reject\n");
}

void TitleImportDialog::on_btnAppBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select title executable...",
                                                EmuDirectory,
                                                "DSiware executables (*.app *.nds *.dsi *.srl);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtAppFile->setText(file);
}

void TitleImportDialog::on_btnTmdBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select title metadata...",
                                                EmuDirectory,
                                                "DSiware metadata (*.tmd);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtTmdFile->setText(file);
}

void TitleImportDialog::onChangeTmdSource(int id)
{
    bool pathenable = (id==0);

    ui->txtTmdFile->setEnabled(pathenable);
    ui->btnTmdBrowse->setEnabled(pathenable);
}
