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

#ifndef RAMINFODIALOG_H
#define RAMINFODIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QScrollBar>
#include <QThread>
#include <QTimer>

#include "types.h"
#include "NDS.h"

namespace Ui { class RAMInfoDialog; }
class RAMInfoDialog;
class RAMSearchThread;
class RAMUpdateThread;
class EmuThread;

enum ramInfo_ByteType
{
    ramInfo_OneByte = 1, 
    ramInfo_TwoBytes = 2, 
    ramInfo_FourBytes = 4
};

enum ramInfoSTh_SearchMode
{
    ramInfoSTh_Default,
    ramInfoSTh_SearchAll
};

enum
{
    ramInfo_Address, 
    ramInfo_Value, 
    ramInfo_Previous
};

melonDS::s32 GetMainRAMValue(melonDS::NDS& nds, const melonDS::u32& addr, const ramInfo_ByteType& byteType);

struct ramInfo_RowData
{
    melonDS::u32 Address;
    melonDS::s32 Value;
    melonDS::s32 Previous;

    void Update(melonDS::NDS& nds, const ramInfo_ByteType& byteType)
    {
        Value = GetMainRAMValue(nds, Address, byteType);
    }

    void SetValue(melonDS::NDS& nds, const melonDS::s32& value)
    {
        nds.MainRAM[Address&nds.MainRAMMask] = (melonDS::u32)value;
        Value = value;
    }
};

class RAMInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RAMInfoDialog(QWidget* parent, EmuThread* emuThread);
    ~RAMInfoDialog();

    static RAMInfoDialog* currentDlg;
    static RAMInfoDialog* openDlg(QWidget* parent, EmuThread* emuThread)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new RAMInfoDialog(parent, emuThread);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

    melonDS::s32 SearchValue = 0;

    void ClearTableContents();

private slots:
    void done(int r);

    void on_btnSearch_clicked();
    void on_btnClear_clicked();
    void on_radiobtn1byte_clicked();
    void on_radiobtn2bytes_clicked();
    void on_radiobtn4bytes_clicked();
    void on_ramTable_itemChanged(QTableWidgetItem *item);

    void OnSearchFinished();
    void ShowRowsInTable();
    void SetProgressbarValue(const melonDS::u32& value);

private:
    EmuThread* emuThread;
    Ui::RAMInfoDialog* ui;
    
    RAMSearchThread* SearchThread;
    QTimer* TableUpdater;
};

class RAMSearchThread : public QThread
{
    Q_OBJECT

public:
    explicit RAMSearchThread(RAMInfoDialog* dialog);
    ~RAMSearchThread() override;

    void Start(const melonDS::s32& searchValue, const ramInfoSTh_SearchMode& searchMode = ramInfoSTh_Default);
    void Start(const ramInfoSTh_SearchMode& searchMode);
    
    void SetSearchByteType(const ramInfo_ByteType& bytetype);
    ramInfo_ByteType GetSearchByteType() const;
    std::vector<ramInfo_RowData>* GetResults();

    void Stop();

private:
    virtual void run() override;

    RAMInfoDialog* Dialog;
    bool SearchRunning = false;

    ramInfoSTh_SearchMode SearchMode;
    melonDS::s32 SearchValue;
    ramInfo_ByteType SearchByteType = ramInfo_OneByte;
    std::vector<ramInfo_RowData>* RowDataVector = nullptr;

    void ClearTableContents();

signals:
    void SetProgressbarValue(const melonDS::u32& value);
};

#endif // RAMINFODIALOG_H
