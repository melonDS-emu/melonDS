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

#ifndef RAMSEARCHDIALOG_H
#define RAMSEARCHDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QScrollBar>
#include <QThread>
#include <QTimer>

#include "types.h"
#include "NDS.h"

namespace Ui { class RAMSearchDialog; }
class RAMSearchDialog;

enum ramSearch_ByteType
{
    ramSearch_OneByte = 1, 
    ramSearch_TwoBytes = 2, 
    ramSearch_FourBytes = 4
};

enum ramSearch_SearchMode
{
    ramSearch_Next,
    ramSearch_First
};

enum
{
    ramSearch_Address, 
    ramSearch_Value, 
    ramSearch_Previous
};

s32 GetMainRAMValue(const u32& addr, const ramSearch_ByteType& byteType);

struct ramSearch_RowData
{
    u32 Address;
    s32 Value;
    s32 Previous;

    void Update(const ramSearch_ByteType& byteType)
    {
        Value = GetMainRAMValue(Address, byteType);
    }

    void SetValue(const s32& value)
    {
        NDS::MainRAM[Address&NDS::MainRAMMask] = (u32)value;
        Value = value;
    }
};

class RAMSearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RAMSearchDialog(QWidget* parent);
    ~RAMSearchDialog();

    static RAMSearchDialog* currentDlg;
    static RAMSearchDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new RAMSearchDialog(parent);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

    void FirstSearch();
    void FirstSearch(const s32 value);
    void NextSearch(const s32 value);
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
    std::vector<ramSearch_RowData>* GetRowDataVector();

private:
    Ui::RAMSearchDialog* ui;

    QTimer* TableUpdater;

    bool IsFirstSearch = true;

    std::vector<ramSearch_RowData>* RowDataVector = nullptr;
    ramSearch_ByteType SearchByteType = ramSearch_OneByte;
    
};

#endif // RAMSEARCHDIALOG_H
