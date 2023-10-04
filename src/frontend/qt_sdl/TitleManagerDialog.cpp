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
#include <QMenu>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "ROMManager.h"
#include "DSi_NAND.h"

#include "TitleManagerDialog.h"
#include "ui_TitleManagerDialog.h"
#include "ui_TitleImportDialog.h"

using namespace Platform;

std::unique_ptr<DSi_NAND::NANDImage> TitleManagerDialog::nand = nullptr;
TitleManagerDialog* TitleManagerDialog::currentDlg = nullptr;

extern std::string EmuDirectory;


TitleManagerDialog::TitleManagerDialog(QWidget* parent, DSi_NAND::NANDImage& image) : QDialog(parent), ui(new Ui::TitleManagerDialog), nandmount(image)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->lstTitleList->setIconSize(QSize(32, 32));

    const u32 category = 0x00030004;
    std::vector<u32> titlelist;
    nandmount.ListTitles(category, titlelist);

    for (std::vector<u32>::iterator it = titlelist.begin(); it != titlelist.end(); it++)
    {
        u32 titleid = *it;
        createTitleItem(category, titleid);
    }

    ui->lstTitleList->sortItems();

    ui->btnImportTitleData->setEnabled(false);
    ui->btnExportTitleData->setEnabled(false);
    ui->btnDeleteTitle->setEnabled(false);

    {
        QMenu* menu = new QMenu(ui->btnImportTitleData);

        actImportTitleData[0] = menu->addAction("public.sav");
        actImportTitleData[0]->setData(QVariant(DSi_NAND::TitleData_PublicSav));
        connect(actImportTitleData[0], &QAction::triggered, this, &TitleManagerDialog::onImportTitleData);

        actImportTitleData[1] = menu->addAction("private.sav");
        actImportTitleData[1]->setData(QVariant(DSi_NAND::TitleData_PrivateSav));
        connect(actImportTitleData[1], &QAction::triggered, this, &TitleManagerDialog::onImportTitleData);

        actImportTitleData[2] = menu->addAction("banner.sav");
        actImportTitleData[2]->setData(QVariant(DSi_NAND::TitleData_BannerSav));
        connect(actImportTitleData[2], &QAction::triggered, this, &TitleManagerDialog::onImportTitleData);

        ui->btnImportTitleData->setMenu(menu);
    }

    {
        QMenu* menu = new QMenu(ui->btnExportTitleData);

        actExportTitleData[0] = menu->addAction("public.sav");
        actExportTitleData[0]->setData(QVariant(DSi_NAND::TitleData_PublicSav));
        connect(actExportTitleData[0], &QAction::triggered, this, &TitleManagerDialog::onExportTitleData);

        actExportTitleData[1] = menu->addAction("private.sav");
        actExportTitleData[1]->setData(QVariant(DSi_NAND::TitleData_PrivateSav));
        connect(actExportTitleData[1], &QAction::triggered, this, &TitleManagerDialog::onExportTitleData);

        actExportTitleData[2] = menu->addAction("banner.sav");
        actExportTitleData[2]->setData(QVariant(DSi_NAND::TitleData_BannerSav));
        connect(actExportTitleData[2], &QAction::triggered, this, &TitleManagerDialog::onExportTitleData);

        ui->btnExportTitleData->setMenu(menu);
    }
}

TitleManagerDialog::~TitleManagerDialog()
{
    delete ui;
}

void TitleManagerDialog::createTitleItem(u32 category, u32 titleid)
{
    u32 version;
    NDSHeader header;
    NDSBanner banner;

    nandmount.GetTitleInfo(category, titleid, version, &header, &banner);

    u32 icondata[32*32];
    ROMManager::ROMIcon(banner.Icon, banner.Palette, icondata);
    QImage iconimg((const uchar*)icondata, 32, 32, QImage::Format_ARGB32);
    QIcon icon(QPixmap::fromImage(iconimg.copy()));

    // TODO: make it possible to select other languages?
    QString title = QString::fromUtf16(banner.EnglishTitle, 128);
    title.replace("\n", " · ");

    char gamecode[5];
    *(u32*)&gamecode[0] = *(u32*)&header.GameCode[0];
    gamecode[4] = '\0';
    char extra[128];
    sprintf(extra, "\n(title ID: %s · %08x/%08x · version %08x)", gamecode, category, titleid, version);

    QListWidgetItem* item = new QListWidgetItem(title + QString(extra));
    item->setIcon(icon);
    item->setData(Qt::UserRole, QVariant((qulonglong)(((u64)category<<32) | (u64)titleid)));
    item->setData(Qt::UserRole+1, QVariant(header.DSiPublicSavSize)); // public.sav size
    item->setData(Qt::UserRole+2, QVariant(header.DSiPrivateSavSize)); // private.sav size
    item->setData(Qt::UserRole+3, QVariant((u32)((header.AppFlags & 0x04) ? 0x4000 : 0))); // banner.sav size
    ui->lstTitleList->addItem(item);
}

bool TitleManagerDialog::openNAND()
{
    nand = nullptr;

    FileHandle* bios7i = Platform::OpenLocalFile(Config::DSiBIOS7Path, FileMode::Read);
    if (!bios7i)
        return false;

    u8 es_keyY[16];
    FileSeek(bios7i, 0x8308, FileSeekOrigin::Start);
    FileRead(es_keyY, 16, 1, bios7i);
    CloseFile(bios7i);

    FileHandle* nandfile = Platform::OpenLocalFile(Config::DSiNANDPath, FileMode::ReadWriteExisting);
    if (!nandfile)
        return false;

    nand = std::make_unique<DSi_NAND::NANDImage>(nandfile, es_keyY);
    if (!*nand)
    { // If loading and mounting the NAND image failed...
        nand = nullptr;
        return false;
        // NOTE: The NANDImage takes ownership of the FileHandle,
        // so it will be closed even if the NANDImage constructor fails.
    }

    return true;
}

void TitleManagerDialog::closeNAND()
{
    nand = nullptr;
}

void TitleManagerDialog::done(int r)
{
    QDialog::done(r);

    closeDlg();
}

void TitleManagerDialog::on_btnImportTitle_clicked()
{
    TitleImportDialog* importdlg = new TitleImportDialog(this, importAppPath, &importTmdData, importReadOnly, nandmount);
    importdlg->open();
    connect(importdlg, &TitleImportDialog::finished, this, &TitleManagerDialog::onImportTitleFinished);

    importdlg->show();
}

void TitleManagerDialog::onImportTitleFinished(int res)
{
    if (res != QDialog::Accepted) return;

    u32 titleid[2];
    titleid[0] = importTmdData.GetCategory();
    titleid[1] = importTmdData.GetID();

    assert(nand != nullptr);
    assert(*nand);
    // remove anything that might hinder the install
    nandmount.DeleteTitle(titleid[0], titleid[1]);

    bool importres = nandmount.ImportTitle(importAppPath.toStdString().c_str(), importTmdData, importReadOnly);
    if (!importres)
    {
        // remove a potential half-completed install
        nandmount.DeleteTitle(titleid[0], titleid[1]);

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

    u64 titleid = cur->data(Qt::UserRole).toULongLong();
    nandmount.DeleteTitle((u32)(titleid >> 32), (u32)titleid);

    delete cur;
}

void TitleManagerDialog::on_lstTitleList_currentItemChanged(QListWidgetItem* cur, QListWidgetItem* prev)
{
    if (!cur)
    {
        ui->btnImportTitleData->setEnabled(false);
        ui->btnExportTitleData->setEnabled(false);
        ui->btnDeleteTitle->setEnabled(false);
    }
    else
    {
        ui->btnImportTitleData->setEnabled(true);
        ui->btnExportTitleData->setEnabled(true);
        ui->btnDeleteTitle->setEnabled(true);

        u32 val;
        val = cur->data(Qt::UserRole+1).toUInt();
        actImportTitleData[0]->setEnabled(val != 0);
        actExportTitleData[0]->setEnabled(val != 0);
        val = cur->data(Qt::UserRole+2).toUInt();
        actImportTitleData[1]->setEnabled(val != 0);
        actExportTitleData[1]->setEnabled(val != 0);
        val = cur->data(Qt::UserRole+3).toUInt();
        actImportTitleData[2]->setEnabled(val != 0);
        actExportTitleData[2]->setEnabled(val != 0);
    }
}

void TitleManagerDialog::onImportTitleData()
{
    int type = ((QAction*)sender())->data().toInt();

    QListWidgetItem* cur = ui->lstTitleList->currentItem();
    if (!cur)
    {
        Log(LogLevel::Error, "what??\n");
        return;
    }

    QString extensions = "*.sav";
    u32 wantedsize;
    switch (type)
    {
    case DSi_NAND::TitleData_PublicSav:
        extensions += " *.pub";
        wantedsize = cur->data(Qt::UserRole+1).toUInt();
        break;
    case DSi_NAND::TitleData_PrivateSav:
        extensions += " *.prv";
        wantedsize = cur->data(Qt::UserRole+2).toUInt();
        break;
    case DSi_NAND::TitleData_BannerSav:
        extensions += " *.bnr";
        wantedsize = cur->data(Qt::UserRole+3).toUInt();
        break;
    default:
        Log(LogLevel::Warn, "what??\n");
        return;
    }

    QString file = QFileDialog::getOpenFileName(this,
                                                "Select file to import...",
                                                QString::fromStdString(EmuDirectory),
                                                "Title data files (" + extensions + ");;Any file (*.*)");

    if (file.isEmpty()) return;

    FILE* f = fopen(file.toStdString().c_str(), "rb");
    if (!f)
    {
        QMessageBox::critical(this,
                              "Import title data - melonDS",
                              "Could not open data file.\nCheck that the file is accessible.");
        return;
    }

    fseek(f, 0, SEEK_END);
    u64 len = ftell(f);
    fclose(f);

    if (len != wantedsize)
    {
        QMessageBox::critical(this,
                              "Import title data - melonDS",
                              QString("Cannot import this data file: size is incorrect (expected: %1 bytes).").arg(wantedsize));
        return;
    }

    u64 titleid = cur->data(Qt::UserRole).toULongLong();
    bool res = nandmount.ImportTitleData((u32)(titleid >> 32), (u32)titleid, type, file.toStdString().c_str());
    if (!res)
    {
        QMessageBox::critical(this,
                              "Import title data - melonDS",
                              "Failed to import the data file. Check that your NAND is accessible and valid.");
    }
}

void TitleManagerDialog::onExportTitleData()
{
    int type = ((QAction*)sender())->data().toInt();

    QListWidgetItem* cur = ui->lstTitleList->currentItem();
    if (!cur)
    {
        Log(LogLevel::Error, "what??\n");
        return;
    }

    QString exportname;
    QString extensions = "*.sav";
    u32 wantedsize;
    switch (type)
    {
    case DSi_NAND::TitleData_PublicSav:
        exportname = "/public.sav";
        extensions += " *.pub";
        wantedsize = cur->data(Qt::UserRole+1).toUInt();
        break;
    case DSi_NAND::TitleData_PrivateSav:
        exportname = "/private.sav";
        extensions += " *.prv";
        wantedsize = cur->data(Qt::UserRole+2).toUInt();
        break;
    case DSi_NAND::TitleData_BannerSav:
        exportname = "/banner.sav";
        extensions += " *.bnr";
        wantedsize = cur->data(Qt::UserRole+3).toUInt();
        break;
    default:
        Log(LogLevel::Warn, "what??\n");
        return;
    }

    QString file = QFileDialog::getSaveFileName(this,
                                                "Select path to export to...",
                                                QString::fromStdString(EmuDirectory) + exportname,
                                                "Title data files (" + extensions + ");;Any file (*.*)");

    if (file.isEmpty()) return;

    u64 titleid = cur->data(Qt::UserRole).toULongLong();
    bool res = nandmount.ExportTitleData((u32)(titleid >> 32), (u32)titleid, type, file.toStdString().c_str());
    if (!res)
    {
        QMessageBox::critical(this,
                              "Export title data - melonDS",
                              "Failed to Export the data file. Check that the destination directory is writable.");
    }
}


TitleImportDialog::TitleImportDialog(QWidget* parent, QString& apppath, const DSi_TMD::TitleMetadata* tmd, bool& readonly, DSi_NAND::NANDMount& nandmount)
: QDialog(parent), ui(new Ui::TitleImportDialog), appPath(apppath), tmdData(tmd), readOnly(readonly), nandmount(nandmount)
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
                              "Executable file is not a DSiWare title.");
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

        fread((void *) tmdData, sizeof(DSi_TMD::TitleMetadata), 1, f);
        fclose(f);

        u32 tmdtitleid[2];
        tmdtitleid[0] = tmdData->GetCategory();
        tmdtitleid[1] = tmdData->GetID();

        if (tmdtitleid[1] != titleid[0] || tmdtitleid[0] != titleid[1])
        {
            QMessageBox::critical(this,
                                  "Import title - melonDS",
                                  "Title ID in metadata file does not match executable file.");
            return;
        }
    }

    if (nandmount.TitleExists(titleid[1], titleid[0]))
    {
        if (QMessageBox::question(this,
                                  "Import title - melonDS",
                                  "The selected title is already installed. Overwrite it?",
                                  QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No),
                                  QMessageBox::No) != QMessageBox::Yes)
            return;
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
    else
    {
        appPath = ui->txtAppFile->text();
        readOnly = ui->cbReadOnly->isChecked();
        QDialog::accept();
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
        netreply->read((char*)tmdData, sizeof(*tmdData));

        u32 tmdtitleid[2];
        tmdtitleid[0] = tmdData->GetCategory();
        tmdtitleid[1] = tmdData->GetID();

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

void TitleImportDialog::on_btnAppBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select title executable...",
                                                QString::fromStdString(EmuDirectory),
                                                "DSiWare executables (*.app *.nds *.dsi *.srl);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtAppFile->setText(file);
}

void TitleImportDialog::on_btnTmdBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select title metadata...",
                                                QString::fromStdString(EmuDirectory),
                                                "DSiWare metadata (*.tmd);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtTmdFile->setText(file);
}

void TitleImportDialog::onChangeTmdSource(int id)
{
    bool pathenable = (id==0);

    ui->txtTmdFile->setEnabled(pathenable);
    ui->btnTmdBrowse->setEnabled(pathenable);
}
