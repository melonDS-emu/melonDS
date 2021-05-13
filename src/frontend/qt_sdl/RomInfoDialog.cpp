/*
    Copyright 2016-2021 Arisotura, WaluigiWare64

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

#include "RomInfoDialog.h"
#include "ui_RomInfoDialog.h"

#include "NDS.h"
#include "NDSCart.h"

QString IntToHex(u64 num)
{
    return ("0x" + QString::number(num, 16).toUpper());
}

QString QStringBytes(u64 num)
{
    return (QString::number(num) + " Bytes");
}

RomInfoDialog* RomInfoDialog::currentDlg = nullptr;

RomInfoDialog::RomInfoDialog(QWidget* parent) : QDialog(parent), ui(new Ui::RomInfoDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    u16 palette[16];
    memcpy(palette, NDSCart::Banner.Palette, sizeof(NDSCart::Banner.Palette)); // Access unaligned palette variable safely
    std::shared_ptr<u32> iconData = Frontend::ROMIcon(NDSCart::Banner.Icon, palette);
    QImage iconImage(reinterpret_cast<unsigned char*>(iconData.get()), 32, 32, QImage::Format_ARGB32);
    ui->iconImage->setPixmap(QPixmap::fromImage(iconImage));

    if (NDSCart::Banner.Version == 0x103)
    {
        u16 animatedPalette[8][16];
        u16 sequence[64];
        memcpy(animatedPalette, NDSCart::Banner.DSiPalette, sizeof(NDSCart::Banner.DSiPalette));
        memcpy(sequence, NDSCart::Banner.DSiSequence, sizeof(NDSCart::Banner.DSiSequence));
        animatedIconData = Frontend::AnimatedROMIcon(NDSCart::Banner.DSiIcon, animatedPalette, sequence);
        
        for (std::shared_ptr<u32> frame: animatedIconData)
        {
            animatedIconImages.push_back(QPixmap::fromImage(QImage(reinterpret_cast<unsigned char*>(frame.get()), 32, 32, QImage::Format_ARGB32)));
        }
        iconTimeline = new QTimeLine(animatedIconImages.size() / 60 * 1000, this);
        iconTimeline->setFrameRange(0, animatedIconImages.size() - 1);
        iconTimeline->setLoopCount(0);
        iconTimeline->setEasingCurve(QEasingCurve::Linear);
        connect(iconTimeline, &QTimeLine::frameChanged, this, &RomInfoDialog::iconSetFrame);
        iconTimeline->start();
    }
    else
    {
        ui->dsiIconImage->setPixmap(QPixmap::fromImage(iconImage));
    }
    
    char16_t titles[8][128];
    memcpy(&titles, NDSCart::Banner.Titles, sizeof(NDSCart::Banner.Titles));

    ui->iconTitle->setText(QString::fromUtf16(titles[1]));

    ui->japaneseTitle->setText(QString::fromUtf16(titles[0]));
    ui->englishTitle->setText(QString::fromUtf16(titles[1]));
    ui->frenchTitle->setText(QString::fromUtf16(titles[2]));
    ui->germanTitle->setText(QString::fromUtf16(titles[3]));
    ui->italianTitle->setText(QString::fromUtf16(titles[4]));
    ui->spanishTitle->setText(QString::fromUtf16(titles[5]));

    if (NDSCart::Banner.Version > 1)
        ui->chineseTitle->setText(QString::fromUtf16(titles[6]));
    else
        ui->chineseTitle->setText("None");
    
    if (NDSCart::Banner.Version > 2)
        ui->koreanTitle->setText(QString::fromUtf16(titles[7]));
    else
        ui->koreanTitle->setText("None");

    ui->gameTitle->setText(QString::fromLatin1(NDSCart::Header.GameTitle, 12));
    ui->gameCode->setText(QString::fromLatin1(NDSCart::Header.GameCode, 4));
    ui->makerCode->setText(QString::fromLatin1(NDSCart::Header.MakerCode, 2));
    ui->cardSize->setText(QString::number(128 << NDSCart::Header.CardSize) + " KB");

    ui->arm9RomOffset->setText(IntToHex(NDSCart::Header.ARM9ROMOffset));
    ui->arm9EntryAddress->setText(IntToHex(NDSCart::Header.ARM9EntryAddress));
    ui->arm9RamAddress->setText(IntToHex(NDSCart::Header.ARM9RAMAddress));
    ui->arm9Size->setText(QStringBytes(NDSCart::Header.ARM9Size));

    ui->arm7RomOffset->setText(IntToHex(NDSCart::Header.ARM7ROMOffset));
    ui->arm7EntryAddress->setText(IntToHex(NDSCart::Header.ARM7EntryAddress));
    ui->arm7RamAddress->setText(IntToHex(NDSCart::Header.ARM7RAMAddress));
    ui->arm7Size->setText(QStringBytes(NDSCart::Header.ARM7Size));
    
    ui->fntOffset->setText(IntToHex(NDSCart::Header.FNTOffset));
    ui->fntSize->setText(QStringBytes(NDSCart::Header.FNTSize));
    ui->fatOffset->setText(IntToHex(NDSCart::Header.FATOffset));
    ui->fatSize->setText(QStringBytes(NDSCart::Header.FATSize));
    
}

RomInfoDialog::~RomInfoDialog()
{
    delete iconTimeline;
    delete ui;
}

void RomInfoDialog::done(int r)
{
    QDialog::done(r);

    closeDlg();
}

void RomInfoDialog::iconSetFrame(int frame)
{
    ui->dsiIconImage->setPixmap(animatedIconImages[frame]);
}
