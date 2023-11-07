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

#ifndef DATETIMEDIALOG_H
#define DATETIMEDIALOG_H

#include <QDialog>
#include <QButtonGroup>
#include <QDateTime>

namespace Ui {class DateTimeDialog; }
class DateTimeDialog;

class DateTimeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DateTimeDialog(QWidget* parent);
    ~DateTimeDialog();

    static DateTimeDialog* currentDlg;
    static DateTimeDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new DateTimeDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

protected:
    void timerEvent(QTimerEvent* event) override;

private slots:
    void done(int r);

    void on_chkChangeTime_clicked(bool checked);
    void on_chkResetTime_clicked(bool checked);

private:
    Ui::DateTimeDialog* ui;

    QDateTime customTime;
};

#endif // DATETIMEDIALOG_H
