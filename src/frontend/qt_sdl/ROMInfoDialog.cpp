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

#include "ROMInfoDialog.h"
#include "ui_ROMInfoDialog.h"

#include <QFileDialog>

#include "NDS.h"
#include "NDSCart.h"
#include "Platform.h"
#include "Config.h"

QString IntToHex(u64 num)
{
    return ("0x" + QString::number(num, 16).toUpper());
}

QString QStringBytes(u64 num)
{
    return (QString::number(num) + " bytes");
}

ROMInfoDialog* ROMInfoDialog::currentDlg = nullptr;

ROMInfoDialog::ROMInfoDialog(QWidget* parent) : QDialog(parent), ui(new Ui::ROMInfoDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);


    u32 iconData[32 * 32];
    ROMManager::ROMIcon(NDSCart::Banner.Icon, NDSCart::Banner.Palette, iconData);
    iconImage = QImage(reinterpret_cast<unsigned char*>(iconData), 32, 32, QImage::Format_ARGB32).copy();
    ui->iconImage->setPixmap(QPixmap::fromImage(iconImage));

    if (NDSCart::Banner.Version == 0x103)
    {
        u32 animatedIconData[32 * 32 * 64] = {0};
        ROMManager::AnimatedROMIcon(NDSCart::Banner.DSiIcon, NDSCart::Banner.DSiPalette, NDSCart::Banner.DSiSequence, animatedIconData, animatedSequence);

        for (int i = 0; i < 64; i++)
        {
            if (animatedIconData[32 * 32 * i] == 0)
                break;
            animatedIconImages.push_back(QPixmap::fromImage(QImage(reinterpret_cast<unsigned char*>(&animatedIconData[32 * 32 * i]), 32, 32, QImage::Format_ARGB32).copy()));
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

    ui->iconTitle->setText(QString::fromUtf16(NDSCart::Banner.EnglishTitle));

    ui->japaneseTitle->setText(QString::fromUtf16(NDSCart::Banner.JapaneseTitle, 128));
    ui->englishTitle->setText(QString::fromUtf16(NDSCart::Banner.EnglishTitle, 128));
    ui->frenchTitle->setText(QString::fromUtf16(NDSCart::Banner.FrenchTitle, 128));
    ui->germanTitle->setText(QString::fromUtf16(NDSCart::Banner.GermanTitle, 128));
    ui->italianTitle->setText(QString::fromUtf16(NDSCart::Banner.ItalianTitle, 128));
    ui->spanishTitle->setText(QString::fromUtf16(NDSCart::Banner.SpanishTitle, 128));

    if (NDSCart::Banner.Version > 1)
        ui->chineseTitle->setText(QString::fromUtf16(NDSCart::Banner.ChineseTitle));
    else
        ui->chineseTitle->setText("None");

    if (NDSCart::Banner.Version > 2)
        ui->koreanTitle->setText(QString::fromUtf16(NDSCart::Banner.KoreanTitle));
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

void ROMInfoDialog::iconSetFrame(int frame)
{
    ui->dsiIconImage->setPixmap(animatedIconImages[animatedSequence[frame]]);
}
