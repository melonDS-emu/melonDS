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

#include "RAMInfoDialog.h"
#include "ui_RAMInfoDialog.h"

#include "main.h"

using namespace melonDS;
extern EmuThread* emuThread;

s32 GetMainRAMValue(NDS& nds, const u32& addr, const ramInfo_ByteType& byteType)
{
    switch (byteType)
    {
    case ramInfo_OneByte:
        return *(s8*)(nds.MainRAM + (addr&nds.MainRAMMask));
    case ramInfo_TwoBytes:
        return *(s16*)(nds.MainRAM + (addr&nds.MainRAMMask));
    case ramInfo_FourBytes:
        return *(s32*)(nds.MainRAM + (addr&nds.MainRAMMask));
    default:
        return 0;
    }
}

RAMInfoDialog* RAMInfoDialog::currentDlg = nullptr;

RAMInfoDialog::RAMInfoDialog(QWidget* parent, EmuThread* emuThread) : QDialog(parent), emuThread(emuThread), ui(new Ui::RAMInfoDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<u32>("u32");
    qRegisterMetaType<s32>("s32");
    qRegisterMetaType<s16>("s16");
    qRegisterMetaType<s8>("s8");

    SearchThread = new RAMSearchThread(this);
    connect(SearchThread, &RAMSearchThread::SetProgressbarValue, this, &RAMInfoDialog::SetProgressbarValue);
    connect(SearchThread, &RAMSearchThread::finished, this, &RAMInfoDialog::OnSearchFinished);
    // First search (Show everything in main ram)
    SearchThread->Start(ramInfoSTh_SearchAll);

    TableUpdater = new QTimer(this);
    TableUpdater->setInterval(100);
    connect(TableUpdater, &QTimer::timeout, this, &RAMInfoDialog::ShowRowsInTable);
    TableUpdater->start();
}

RAMInfoDialog::~RAMInfoDialog()
{
    delete SearchThread;
    if (TableUpdater->isActive())
        TableUpdater->stop();
    delete TableUpdater;
    delete ui;
}

void RAMInfoDialog::OnSearchFinished()
{
    SearchThread->wait();
    ui->btnSearch->setEnabled(true);
    ui->ramTable->clearContents();
    ui->ramTable->setRowCount(SearchThread->GetResults()->size());
    ui->ramTable->verticalScrollBar()->setSliderPosition(0);
    ui->txtFound->setText(QString("Found: %1").arg(SearchThread->GetResults()->size()));
}

void RAMInfoDialog::ShowRowsInTable()
{
    const u32& scrollValue = ui->ramTable->verticalScrollBar()->sliderPosition();
    std::vector<ramInfo_RowData>* RowDataVector = SearchThread->GetResults();

    for (u32 row = scrollValue; row < std::min<u32>(scrollValue+25, RowDataVector->size()); row++)
    {
        ramInfo_RowData& rowData = RowDataVector->at(row);
        rowData.Update(*emuThread->NDS, SearchThread->GetSearchByteType());

        if (ui->ramTable->item(row, ramInfo_Address) == nullptr)
        {
            // A new row
            QTableWidgetItem* addressItem = new QTableWidgetItem(QString("%1").arg(rowData.Address, 8, 16));
            QTableWidgetItem* valueItem = new QTableWidgetItem(QString("%1").arg(rowData.Value));
            QTableWidgetItem* previousItem = new QTableWidgetItem(QString("%1").arg(rowData.Previous));

            addressItem->setFlags(addressItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setFlags(valueItem->flags() | Qt::ItemIsEditable);
            previousItem->setFlags(previousItem->flags() & ~Qt::ItemIsEditable);

            ui->ramTable->setItem(row, ramInfo_Address, addressItem);
            ui->ramTable->setItem(row, ramInfo_Value, valueItem);
            ui->ramTable->setItem(row, ramInfo_Previous, previousItem);
        }
        else
        {
            // A row that exists
            ui->ramTable->item(row, ramInfo_Address)->setText(QString("%1").arg(rowData.Address, 8, 16));
            ui->ramTable->item(row, ramInfo_Value)->setText(QString("%1").arg(rowData.Value));
            ui->ramTable->item(row, ramInfo_Previous)->setText(QString("%1").arg(rowData.Previous));
            if (rowData.Value != rowData.Previous) 
                ui->ramTable->item(row, ramInfo_Previous)->setForeground(Qt::red);
        }
    }
}

void RAMInfoDialog::ClearTableContents()
{
    ui->ramTable->clearContents();
    ui->ramTable->setRowCount(0);
}

void RAMInfoDialog::SetProgressbarValue(const u32& value)
{
    ui->progressBar->setValue(value);
}

void RAMInfoDialog::done(int r)
{
    QDialog::done(r);
    closeDlg();
}

void RAMInfoDialog::on_btnSearch_clicked()
{
    ui->btnSearch->setEnabled(false);
    ui->radiobtn1byte->setEnabled(false);
    ui->radiobtn2bytes->setEnabled(false);
    ui->radiobtn4bytes->setEnabled(false);

    if (ui->txtSearch->text().isEmpty())
        SearchThread->Start(ramInfoSTh_SearchAll);
    else
        SearchThread->Start(ui->txtSearch->text().toInt());
    
    if (!TableUpdater->isActive())
        TableUpdater->start();
}

void RAMInfoDialog::on_btnClear_clicked()
{
    SearchThread->Stop();
    TableUpdater->stop();

    ui->radiobtn1byte->setEnabled(true);
    ui->radiobtn2bytes->setEnabled(true);
    ui->radiobtn4bytes->setEnabled(true);

    OnSearchFinished();
}

void RAMInfoDialog::on_radiobtn1byte_clicked()
{
    SearchThread->SetSearchByteType(ramInfo_OneByte);
}

void RAMInfoDialog::on_radiobtn2bytes_clicked()
{
    SearchThread->SetSearchByteType(ramInfo_TwoBytes);
}

void RAMInfoDialog::on_radiobtn4bytes_clicked()
{
    SearchThread->SetSearchByteType(ramInfo_FourBytes);
}

void RAMInfoDialog::on_ramTable_itemChanged(QTableWidgetItem *item)
{
    ramInfo_RowData& rowData = SearchThread->GetResults()->at(item->row());
    s32 itemValue = item->text().toInt();

    if (rowData.Value != itemValue)
        rowData.SetValue(*emuThread->NDS, itemValue);
}

/**
 * RAMSearchThread 
 */

RAMSearchThread::RAMSearchThread(RAMInfoDialog* dialog) : Dialog(dialog)
{
    RowDataVector = new std::vector<ramInfo_RowData>();
}

RAMSearchThread::~RAMSearchThread()
{
    Stop();
    if (RowDataVector)
    {
        delete RowDataVector;
        RowDataVector = nullptr;
    }
}

void RAMSearchThread::Start(const s32& searchValue, const ramInfoSTh_SearchMode& searchMode)
{
    SearchValue = searchValue;
    SearchMode = searchMode;
    start();
}

void RAMSearchThread::Start(const ramInfoSTh_SearchMode& searchMode)
{
    SearchMode = searchMode;
    start();
}

void RAMSearchThread::Stop()
{
    SearchRunning = false;
    RowDataVector->clear();
    quit();
    wait();
}

void RAMSearchThread::run()
{
    SearchRunning = true;
    u32 progress = 0;

    // Pause game running
    emuThread->emuPause();

    // For following search modes below, RowDataVector must be filled.
    if (SearchMode == ramInfoSTh_SearchAll || RowDataVector->size() == 0)
    {
        // First search mode
        for (u32 addr = 0x02000000; SearchRunning && addr < 0x02000000+MainRAMMaxSize; addr += SearchByteType)
        {
            const s32& value = GetMainRAMValue(*emuThread->NDS, addr, SearchByteType);

            RowDataVector->push_back({ addr, value, value });

            // A solution to prevent to call too many slot.
            u32 newProgress = (int)((addr-0x02000000) / (MainRAMMaxSize-1.0f) * 100);
            if (progress < newProgress)
            {
                progress = newProgress;
                emit SetProgressbarValue(progress);
            }
        }
    }
    
    if (SearchMode == ramInfoSTh_Default)
    {
        // Next search mode
        std::vector<ramInfo_RowData>* newRowDataVector = new std::vector<ramInfo_RowData>();
        for (u32 row = 0; SearchRunning && row < RowDataVector->size(); row++)
        {
            const u32& addr = RowDataVector->at(row).Address;
            const s32& value = GetMainRAMValue(*emuThread->NDS, addr, SearchByteType);

            if (SearchValue == value)
                newRowDataVector->push_back({ addr, value, value });

            // A solution to prevent to call too many slot.
            u32 newProgress = (int)(row / (RowDataVector->size()-1.0f) * 100);
            if (progress < newProgress)
            {
                progress = newProgress;
                emit SetProgressbarValue(progress);
            }
        }
        delete RowDataVector;
        RowDataVector = newRowDataVector;
    }

    // Unpause game running
    emuThread->emuUnpause();

    SearchRunning = false;
}

void RAMSearchThread::SetSearchByteType(const ramInfo_ByteType& bytetype)
{
    SearchByteType = bytetype;
}

ramInfo_ByteType RAMSearchThread::GetSearchByteType() const
{
    return SearchByteType;
}

std::vector<ramInfo_RowData>* RAMSearchThread::GetResults()
{
    return RowDataVector;
}
