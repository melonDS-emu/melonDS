/*
    Copyright 2016-2025 melonDS team

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

#ifndef FIRMWARESETTINGSDIALOG_H
#define FIRMWARESETTINGSDIALOG_H

#include <QDialog>
#include <QWidget>

namespace Ui { class FirmwareSettingsDialog; }
class FirmwareSettingsDialog;

class EmuInstance;

class FirmwareSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    const QStringList colornames
    {
        "灰蓝色",
        "棕色",
        "红色",
        "亮粉色",
        "橙色",
        "黄色",
        "青柠色",
        "亮绿色",
        "暗绿色",
        "绿松石色",
        "亮蓝色",
        "蓝色",
        "暗蓝色",
        "暗紫色",
        "亮紫色",
        "暗粉色"
    };

    const QColor colors[16] =
    {
        QColor(97, 130, 154),
        QColor(186, 73, 0),
        QColor(251, 0, 24),
        QColor(251, 138, 251),
        QColor(251, 146, 0),
        QColor(243, 227, 0),
        QColor(170, 251, 0),
        QColor(0, 251, 0),
        QColor(0, 162, 56),
        QColor(73, 219, 138),
        QColor(48, 186, 243),
        QColor(0, 89, 243),
        QColor(0, 0, 146),
        QColor(138, 0, 211),
        QColor(211, 0, 235),
        QColor(251, 0, 246)
    };

    const QStringList languages
    {
        "日语",
        "英语",
        "法语",
        "德语",
        "意大利语",
        "西班牙语"
    };

    const QStringList months
    {
        "一月",
        "二月",
        "三月",
        "四月",
        "五月",
        "六月",
        "七月",
        "八月",
        "九月",
        "十月",
        "十一月",
        "十二月"
    };

    explicit FirmwareSettingsDialog(QWidget* parent);
    ~FirmwareSettingsDialog();

    static FirmwareSettingsDialog* currentDlg;
    static FirmwareSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new FirmwareSettingsDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

    static bool needsReset;

private slots:
    void done(int r);

    void on_cbxBirthdayMonth_currentIndexChanged(int idx);

private:
    bool verifyMAC();

    Ui::FirmwareSettingsDialog* ui;
    EmuInstance* emuInstance;
};

#endif // FIRMWARESETTINGSDIALOG_H
