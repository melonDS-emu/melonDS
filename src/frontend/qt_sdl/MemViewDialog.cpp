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

#include "MemViewDialog.h"
#include <QPainter>
#include <QGraphicsTextItem>
#include <QTextCursor>

#include "main.h"

using namespace melonDS;
using Platform::Log;
using Platform::LogLevel;

// --- MemViewDialog ---

MemViewDialog* MemViewDialog::currentDlg = nullptr;

CustomTextItem::CustomTextItem(const QString &text, QGraphicsItem *parent) : QGraphicsTextItem(text, parent) {
    this->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse | Qt::TextInteractionFlag::TextSelectableByKeyboard);
    this->setFlag(QGraphicsItem::ItemIsFocusable, false);
}

void CustomTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // select everything
        QTextCursor cursor = this->textCursor();
        cursor.select(QTextCursor::Document);
        this->setTextCursor(cursor);
    }
}

void CustomTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {}

void CustomGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    // if (event->button() == Qt::LeftButton) {
    //     // deselect items if selected
    //     foreach(QGraphicsItem* item, this->items()) {
    //         item->setSelected(false);
    //     }
    // }
}

MemViewDialog::MemViewDialog(QWidget* parent) : QDialog(parent)
{
    this->setObjectName("MemViewDialog");
    this->setFixedSize(550, 410);
    this->setEnabled(true);
    this->setModal(false);
    this->setWindowTitle("Memory Viewer - melonDS");
    setAttribute(Qt::WA_DeleteOnClose);

    this->emuInstance = ((MainWindow*)parent)->getEmuInstance();
    this->nds = this->emuInstance->getNDS();
    this->gfxScene = new CustomGraphicsScene(this);
    this->gfxView = new QGraphicsView(this);
    this->scrollBar = new QScrollBar(this);
    this->updateThread = new MemViewThread();

    this->gfxView->setScene(this->gfxScene);
    this->gfxView->setGeometry(10, 120, 520, 275);
    this->gfxScene->clear();
    this->gfxScene->setSceneRect(0, 0, 520, 275);
    this->scrollBar->setGeometry(530, 120, 16, 275);
    this->scrollBar->setMinimum(0);
    this->scrollBar->setMaximum((this->arm9AddrEnd - this->arm9Addr) / 16);

    QString text;
    QColor color;
    QFont font("Monospace", 10, -1, false);

    // create hex digits texts
    for (int i = 0; i < 16; i++) {
        text.setNum(i, 16);
        QGraphicsTextItem* textItem = new QGraphicsTextItem(text.toUpper().rightJustified(2, '0'));
        textItem->setFont(font);
        qreal x = i * textItem->font().pointSize() * 2; // column number * font size * text length
        qreal y = 0; // always zero

        // account for addresses column length
        x += textItem->font().pointSize() * 8;

        textItem->setPos(x + 10, y);
        this->gfxScene->addItem(textItem);

        // kinda hacky but we need to sync the line's color on the text
        // and since the text color depends on the system's theme... easy way for now...
        if (i == 0) {
            color = textItem->defaultTextColor();
        }
    }

    // create addresses texts
    for (int i = 0; i < 16; i++) {
        text.setNum(i * 16 + this->arm9Addr, 16);

        QGraphicsTextItem* textItem = new QGraphicsTextItem(text.toUpper().rightJustified(8, '0').prepend("0x"));
        textItem->setFont(font);
        textItem->setPos(0, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
        this->gfxScene->addItem(textItem);
        this->addresses[i] = textItem;
    }

    // init memory view
    int index = 0;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            CustomTextItem* textItem = new CustomTextItem("00");
            textItem->setFont(font);
            qreal x = j * textItem->font().pointSize() * 2; // column number * font size * text length
            qreal y = 0; // always zero

            // account for addresses column length
            x += textItem->font().pointSize() * 8;
            textItem->setPos(x + 10, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
            textItem->setTextInteractionFlags(Qt::TextInteractionFlag::TextEditorInteraction);
            this->gfxScene->addItem(textItem);
            this->items[index++] = textItem;
        }
    }

    // add separators
    QGraphicsLineItem* line1 = this->gfxScene->addLine(0, 25, this->gfxScene->width(), 25, QPen(color));
    QGraphicsLineItem* line2 = this->gfxScene->addLine(87, 0, 87, this->gfxScene->height(), QPen(color));

    this->updateThread->Start();
    connect(this->updateThread, &MemViewThread::updateTextSignal, this, &MemViewDialog::updateText);
    connect(this->updateThread, &MemViewThread::updateAddressSignal, this, &MemViewDialog::updateAddress);

    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<u32>("u32");
    qRegisterMetaType<s32>("s32");
    qRegisterMetaType<s16>("s16");
    qRegisterMetaType<s8>("s8");
}

MemViewDialog::~MemViewDialog()
{
    this->gfxScene->deleteLater();
    this->gfxView->deleteLater();
    this->scrollBar->deleteLater();
    this->updateThread->deleteLater();
    delete this->gfxScene;
    delete this->gfxView;
    delete this->scrollBar;
    delete this->updateThread;
}

void MemViewDialog::done(int r)
{
    QDialog::done(r);
    closeDlg();
}

void MemViewDialog::updateText(int index) {
    QGraphicsTextItem* item = (QGraphicsTextItem*)this->GetItem(index);
    QString text;

    if (item != nullptr) {
        u8 value = this->GetNDS()->ARM9Read8(this->arm9Addr + (this->scrollBar->value() * 16) + index);

        text.setNum(value, 16);
        item->setPlainText(text.toUpper().rightJustified(2, '0'));
    }
}

void MemViewDialog::updateAddress(int index) {
    QGraphicsTextItem* item = (QGraphicsTextItem*)this->GetAddressItem(index);
    QString text;

    if (item != nullptr) {
        uint32_t address = this->arm9Addr + (this->scrollBar->value() + index) * 16;

        text.setNum(address, 16);
        item->setPlainText(text.toUpper().rightJustified(8, '0').prepend("0x"));
    }
}

// --- MemViewThread ---

MemViewThread::~MemViewThread() {
    this->quit();
    this->wait();
}

void MemViewThread::Start() {
    this->start();
    Log(LogLevel::Debug, "MEMVIEW THREAD STARTED!!!!!");
}

void MemViewThread::run() {
    while (true) {
        QThread::msleep(50);

        for (int i = 0; i < 256; i++) {
            emit updateTextSignal(i);
        }

        for (int i = 0; i < 16; i++) {
            emit updateAddressSignal(i);
        }
    }
}
