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

#ifndef MPSETTINGSDIALOG_H
#define MPSETTINGSDIALOG_H

#include <QDialog>
#include <QButtonGroup>

namespace Ui { class MPSettingsDialog; }
class MPSettingsDialog;

class MPSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MPSettingsDialog(QWidget* parent);
    ~MPSettingsDialog();

    static MPSettingsDialog* currentDlg;
    static MPSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new MPSettingsDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void done(int r);

    //

private:
    Ui::MPSettingsDialog* ui;

    QButtonGroup* grpAudioMode;
};

#endif // MPSETTINGSDIALOG_H
