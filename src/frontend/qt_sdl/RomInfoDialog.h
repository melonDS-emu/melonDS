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

#ifndef ROMINFODIALOG_H
#define ROMINFODIALOG_H

#include <QDialog>
#include <QTimeLine>
#include <QPixmap>

#include "types.h"
#include "FrontendUtil.h"

namespace Ui { class RomInfoDialog; }
class RomInfoDialog;

class RomInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RomInfoDialog(QWidget* parent);
    ~RomInfoDialog();

    static RomInfoDialog* currentDlg;
    static RomInfoDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new RomInfoDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void done(int r);
    
    void iconSetFrame(int frame);

private:
    Ui::RomInfoDialog* ui;

    QTimeLine* iconTimeline;
    std::vector<QPixmap> animatedIconImages;
    std::vector<int> animatedSequence;
};

#endif // ROMINFODIALOG_H
