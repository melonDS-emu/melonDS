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

#ifndef ROMINFODIALOG_H
#define ROMINFODIALOG_H

#include <QDialog>
#include <QTimeLine>
#include <QPixmap>
#include <QImage>

#include "types.h"
#include "ROMManager.h"

namespace Ui { class ROMInfoDialog; }
class ROMInfoDialog;
namespace melonDS::NDSCart { class CartCommon; }
class ROMInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ROMInfoDialog(QWidget* parent, const melonDS::NDSCart::CartCommon& rom);
    ~ROMInfoDialog();

    static ROMInfoDialog* currentDlg;
    static ROMInfoDialog* openDlg(QWidget* parent, const melonDS::NDSCart::CartCommon& rom)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new ROMInfoDialog(parent, rom);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void done(int r);

    void on_saveIconButton_clicked();
    void on_saveAnimatedIconButton_clicked();

    void iconSetFrame(int frame);

private:
    Ui::ROMInfoDialog* ui;

    QImage iconImage;
    QTimeLine* iconTimeline;
    melonDS::u32 animatedIconData[64][32*32] = {0};
    std::vector<QPixmap> animatedIconImages;
    std::vector<int> animatedSequence;
};

#endif // ROMINFODIALOG_H
