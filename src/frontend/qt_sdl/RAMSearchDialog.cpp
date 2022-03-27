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

#include "RamSearchDialog.h"
#include "ui_RamSearchDialog.h"

#include "main.h"

s32 GetMainRAMValue(const u32& addr, const ramSearch_ByteType& byteType)
{
    switch (byteType)
    {
    case ramSearch_OneByte:
        return *(s8*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramSearch_TwoBytes:
        return *(s16*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramSearch_FourBytes:
        return *(s32*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    default:
        return 0;
    }
}

RamSearchDialog* RamSearchDialog::currentDlg = nullptr;

RamSearchDialog::RamSearchDialog(QWidget* parent) : QDialog(parent), ui(new Ui::RamSearchDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    RowDataVector = new std::vector<ramSearch_RowData>();

    TableUpdater = new QTimer(this);
    TableUpdater->setInterval(100);
    connect(TableUpdater, &QTimer::timeout, this, &RamSearchDialog::ShowRowsInTable);
    TableUpdater->start();

    // Clear Table (disable byte type radio buttons)
    ClearTableContents();
}

RamSearchDialog::~RamSearchDialog()
{
    if (TableUpdater->isActive())
        TableUpdater->stop();
    delete TableUpdater;
    delete RowDataVector;
    delete ui;
}

void RamSearchDialog::OnSearchFinished()
{
    ui->btnSearch->setEnabled(true);
    ui->ramTable->clearContents();
    ui->ramTable->setRowCount(RowDataVector->size());
    ui->ramTable->verticalScrollBar()->setSliderPosition(0);
    ui->txtFound->setText(QString("Found: %1").arg(RowDataVector->size()));

    TableUpdater->start();
}

void RamSearchDialog::ShowRowsInTable()
{
    const u32& scrollValue = ui->ramTable->verticalScrollBar()->sliderPosition();

    for (u32 row = scrollValue; row < std::min<u32>(scrollValue+25, RowDataVector->size()); row++)
    {
        ramSearch_RowData& rowData = RowDataVector->at(row);
        rowData.Update(SearchByteType);

        if (ui->ramTable->item(row, ramSearch_Address) == nullptr)
        {
            // A new row
            QTableWidgetItem* addressItem = new QTableWidgetItem(QString("%1").arg(rowData.Address, 8, 16));
            QTableWidgetItem* valueItem = new QTableWidgetItem(QString("%1").arg(rowData.Value));
            QTableWidgetItem* previousItem = new QTableWidgetItem(QString("%1").arg(rowData.Previous));

            addressItem->setFlags(addressItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setFlags(valueItem->flags() | Qt::ItemIsEditable);
            previousItem->setFlags(previousItem->flags() & ~Qt::ItemIsEditable);

            ui->ramTable->setItem(row, ramSearch_Address, addressItem);
            ui->ramTable->setItem(row, ramSearch_Value, valueItem);
            ui->ramTable->setItem(row, ramSearch_Previous, previousItem);
        }
        else
        {
            // A row that exists
            ui->ramTable->item(row, ramSearch_Address)->setText(QString("%1").arg(rowData.Address, 8, 16));
            ui->ramTable->item(row, ramSearch_Value)->setText(QString("%1").arg(rowData.Value));
            ui->ramTable->item(row, ramSearch_Previous)->setText(QString("%1").arg(rowData.Previous));
            if (rowData.Value != rowData.Previous) 
                ui->ramTable->item(row, ramSearch_Previous)->setForeground(Qt::red);
        }
    }
}

std::vector<ramSearch_RowData>* RamSearchDialog::GetRowDataVector() 
{
    return RowDataVector;
}

void RamSearchDialog::ClearTableContents()
{
    ui->ramTable->clearContents();
    ui->ramTable->setRowCount(0);
    
    RowDataVector->clear();
}

void RamSearchDialog::FirstSearch()
{
    // Search all value
    u32 progress = 0;
    TableUpdater->stop();

    for (u32 addr = 0x02000000; addr < 0x02000000+NDS::MainRAMMaxSize; addr += SearchByteType)
    {
        const s32& value = GetMainRAMValue(addr, SearchByteType);

        RowDataVector->push_back({ addr, value, value });

        // Update progress bar
        u32 newProgress = (int)((addr-0x02000000) / (NDS::MainRAMMaxSize-1.0f) * 100);
        if (progress < newProgress)
        {
            progress = newProgress;
            ui->progressBar->setValue(value);
        }
    }

    OnSearchFinished();
}

void RamSearchDialog::FirstSearch(const s32 searchValue)
{
    // First Search mode
    u32 progress = 0;
    TableUpdater->stop();

    for (u32 addr = 0x02000000; addr < 0x02000000+NDS::MainRAMMaxSize; addr += SearchByteType)
    {
        const s32& value = GetMainRAMValue(addr, SearchByteType);

        if (searchValue == value)
            RowDataVector->push_back({ addr, value, value });

        // Update progress bar
        u32 newProgress = (int)((addr-0x02000000) / (NDS::MainRAMMaxSize-1.0f) * 100);
        if (progress < newProgress)
        {
            progress = newProgress;
            ui->progressBar->setValue(value);
        }
    }

    OnSearchFinished();
}

void RamSearchDialog::NextSearch(const s32 searchValue)
{
    // Next search mode
    u32 progress = 0;
    TableUpdater->stop();
        
    std::vector<ramSearch_RowData>* newRowDataVector = new std::vector<ramSearch_RowData>();
    for (u32 row = 0; row < RowDataVector->size(); row++)
    {
        const u32& addr = RowDataVector->at(row).Address;
        const s32& value = GetMainRAMValue(addr, SearchByteType);

        if (searchValue == value)
            newRowDataVector->push_back({ addr, value, value });

        // A solution to prevent to call too many slot.
        u32 newProgress = (int)(row / (RowDataVector->size()-1.0f) * 100);
        if (progress < newProgress)
        {
            progress = newProgress;
            ui->progressBar->setValue(value);
        }
    }
    delete RowDataVector;
    RowDataVector = newRowDataVector;

    OnSearchFinished();
}

void RamSearchDialog::done(int r)
{
    QDialog::done(r);
    closeDlg();
}

void RamSearchDialog::on_btnSearch_clicked()
{
    ui->btnSearch->setEnabled(false);
    ui->radiobtn1byte->setEnabled(false);
    ui->radiobtn2bytes->setEnabled(false);
    ui->radiobtn4bytes->setEnabled(false);
    
    if (IsFirstSearch)
    {
        if (ui->txtSearch->text().isEmpty()) 
            FirstSearch();
        else
            FirstSearch(ui->txtSearch->text().toInt());
    }
    else
    {
        // TODO::show warning message
        if (!ui->txtSearch->text().isEmpty())
            NextSearch(ui->txtSearch->text().toInt());
    }

    if (!TableUpdater->isActive())
        TableUpdater->start();
}

void RamSearchDialog::on_btnClear_clicked()
{
    TableUpdater->stop();

    ui->radiobtn1byte->setEnabled(true);
    ui->radiobtn2bytes->setEnabled(true);
    ui->radiobtn4bytes->setEnabled(true);

    IsFirstSearch = true;

    ClearTableContents();
}

void RamSearchDialog::on_radiobtn1byte_clicked()
{
    SearchByteType = ramSearch_OneByte;
}

void RamSearchDialog::on_radiobtn2bytes_clicked()
{
    SearchByteType = ramSearch_TwoBytes;
}

void RamSearchDialog::on_radiobtn4bytes_clicked()
{
    SearchByteType = ramSearch_FourBytes;
}

void RamSearchDialog::on_ramTable_itemChanged(QTableWidgetItem *item)
{
    ramSearch_RowData& rowData = RowDataVector->at(item->row());
    s32 itemValue = item->text().toInt();

    if (rowData.Value != itemValue)
        rowData.SetValue(itemValue);
}
