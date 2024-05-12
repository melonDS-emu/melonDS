/*
    Copyright 2016-2023 melonDS team

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

#include "ROMInfoDialog.h"
#include "ui_ROMInfoDialog.h"

#include <QFileDialog>

#include "gif-h/gif.h"

#include "NDS.h"
#include "NDSCart.h"
#include "Platform.h"
#include "Config.h"

using namespace melonDS;

QString IntToHex(u64 num)
{
    return ("0x" + QString::number(num, 16).toUpper());
}

QString QStringBytes(u64 num)
{
    return (QString::number(num) + " bytes");
}

ROMInfoDialog* ROMInfoDialog::currentDlg = nullptr;

ROMInfoDialog::ROMInfoDialog(QWidget* parent, const melonDS::NDSCart::CartCommon& rom) : QDialog(parent), ui(new Ui::ROMInfoDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    const NDSBanner* banner = rom.Banner();
    const NDSHeader& header = rom.GetHeader();
    u32 iconData[32 * 32];
    ROMManager::ROMIcon(banner->Icon, banner->Palette, iconData);
    iconImage = QImage(reinterpret_cast<u8*>(iconData), 32, 32, QImage::Format_RGBA8888).copy();
    ui->iconImage->setPixmap(QPixmap::fromImage(iconImage));

    if (banner->Version == 0x103)
    {
        ui->saveAnimatedIconButton->setEnabled(true);

        ROMManager::AnimatedROMIcon(banner->DSiIcon, banner->DSiPalette, banner->DSiSequence, animatedIconData, animatedSequence);

        for (u32* image: animatedIconData)
        {
            if (!image)
                break;
            animatedIconImages.push_back(QPixmap::fromImage(QImage(reinterpret_cast<u8*>(image), 32, 32, QImage::Format_RGBA8888).copy()));
        }
        iconTimeline = new QTimeLine(animatedSequence.size() / 60 * 1000, this);
        iconTimeline->setFrameRange(0, animatedSequence.size() - 1);
        iconTimeline->setLoopCount(0);
        iconTimeline->setEasingCurve(QEasingCurve::Linear);
        connect(iconTimeline, &QTimeLine::frameChanged, this, &ROMInfoDialog::iconSetFrame);
        iconTimeline->start();
    }
    else
    {
        ui->dsiIconImage->setPixmap(QPixmap::fromImage(iconImage));
    }

    ui->iconTitle->setText(QString::fromUtf16(banner->EnglishTitle));

    ui->japaneseTitle->setText(QString::fromUtf16(banner->JapaneseTitle));
    ui->englishTitle->setText(QString::fromUtf16(banner->EnglishTitle));
    ui->frenchTitle->setText(QString::fromUtf16(banner->FrenchTitle));
    ui->germanTitle->setText(QString::fromUtf16(banner->GermanTitle));
    ui->italianTitle->setText(QString::fromUtf16(banner->ItalianTitle));
    ui->spanishTitle->setText(QString::fromUtf16(banner->SpanishTitle));

    if (banner->Version > 1)
        ui->chineseTitle->setText(QString::fromUtf16(banner->ChineseTitle));
    else
        ui->chineseTitle->setText("None");

    if (banner->Version > 2)
        ui->koreanTitle->setText(QString::fromUtf16(banner->KoreanTitle));
    else
        ui->koreanTitle->setText("None");

    ui->gameTitle->setText(QString::fromLatin1(header.GameTitle, 12));
    ui->gameCode->setText(QString::fromLatin1(header.GameCode, 4));
    ui->makerCode->setText(QString::fromLatin1(header.MakerCode, 2));
    ui->cardSize->setText(QString::number(128 << header.CardSize) + " KB");

    ui->arm9RomOffset->setText(IntToHex(header.ARM9ROMOffset));
    ui->arm9EntryAddress->setText(IntToHex(header.ARM9EntryAddress));
    ui->arm9RamAddress->setText(IntToHex(header.ARM9RAMAddress));
    ui->arm9Size->setText(QStringBytes(header.ARM9Size));

    ui->arm7RomOffset->setText(IntToHex(header.ARM7ROMOffset));
    ui->arm7EntryAddress->setText(IntToHex(header.ARM7EntryAddress));
    ui->arm7RamAddress->setText(IntToHex(header.ARM7RAMAddress));
    ui->arm7Size->setText(QStringBytes(header.ARM7Size));

    ui->fntOffset->setText(IntToHex(header.FNTOffset));
    ui->fntSize->setText(QStringBytes(header.FNTSize));
    ui->fatOffset->setText(IntToHex(header.FATOffset));
    ui->fatSize->setText(QStringBytes(header.FATSize));

}

ROMInfoDialog::~ROMInfoDialog()
{
    delete ui;
}

void ROMInfoDialog::done(int r)
{
    QDialog::done(r);

    closeDlg();
}

void ROMInfoDialog::on_saveIconButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Save Icon",
                                                    QString::fromStdString(Config::LastROMFolder),
                                                    "PNG Images (*.png)");
    if (filename.isEmpty())
        return;

    iconImage.save(filename, "PNG");
}

void ROMInfoDialog::on_saveAnimatedIconButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Save Animated Icon",
                                                    QString::fromStdString(Config::LastROMFolder),
                                                    "GIF Images (*.gif)");
    if (filename.isEmpty())
        return;


    GifWriter writer;

    // The GIF format only supports delays of 0.01 seconds, so 0.0166... (60fps)
    // is rounded up to 0.02 (50fps)
    GifBegin(&writer, filename.toStdString().c_str(), 32, 32, 2);
    for (int i: animatedSequence)
    {
        if (animatedIconData[i] == 0)
            break;
        GifWriteFrame(&writer, reinterpret_cast<u8*>(animatedIconData[i]), 32, 32, 2);
    }
    GifEnd(&writer);
}

void ROMInfoDialog::iconSetFrame(int frame)
{
    ui->dsiIconImage->setPixmap(animatedIconImages[animatedSequence[frame]]);
}
