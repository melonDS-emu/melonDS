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
#include <QStyleOptionGraphicsItem>
#include <QStyle>

#include "main.h"

using namespace melonDS;
using Platform::Log;
using Platform::LogLevel;

// --- MemViewDialog ---

MemViewDialog* MemViewDialog::currentDlg = nullptr;

CustomTextItem::CustomTextItem(const QString &text, QGraphicsItem *parent) : QGraphicsTextItem(text, parent) {
    this->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse);
    this->setFlag(QGraphicsItem::ItemIsSelectable, false);
    this->SetSize(QRectF(2, 6, 20, 15));
}

QRectF CustomTextItem::boundingRect() const {
    return this->size;
}

void CustomTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    this->QGraphicsTextItem::mousePressEvent(event);
}

// empty on purpose to disable the move behaviour
void CustomTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {}

void CustomGraphicsScene::wheelEvent(QGraphicsSceneWheelEvent *event) {
    this->QGraphicsScene::wheelEvent(event);

    if (this->scrollBar != nullptr) {
        QWheelEvent *pEvent = new QWheelEvent(
            event->pos(),
            event->screenPos(),
            QPoint(0, 0),
            QPoint(0, event->delta()),
            event->buttons(),
            event->modifiers(),
            event->phase(),
            event->isInverted()
        );

        QGraphicsItem* item = this->focusItem();

        if (item) {
            item->clearFocus();
        }

        QApplication::sendEvent(this->scrollBar, pEvent);
    }
}

void CustomGraphicsScene::onFocusItemChanged(QGraphicsItem *newFocus, QGraphicsItem *oldFocus, Qt::FocusReason reason) {
    MemViewDialog* dialog = (MemViewDialog*)this->parent();

    if (dialog != nullptr && newFocus != nullptr) {
        int index = dialog->GetItemIndex(newFocus);
        uint32_t addr = dialog->GetAddressFromItem((CustomTextItem*)newFocus);

        if (addr >= dialog->arm9AddrEnd + 0x100) {
            addr = dialog->arm9AddrEnd;
        }

        if (index >= 0) {
            QString text;
            text.setNum(addr + index, 16);
            dialog->GetAddrLabel()->setText(text.toUpper().rightJustified(8, '0').prepend("0x"));
            dialog->GetValueAddrLineEdit()->setText(text.toUpper().rightJustified(8, '0').prepend("0x"));
        }
    }
}

MemViewDialog::MemViewDialog(QWidget* parent) : QDialog(parent)
{
    this->arm9AddrStart = 0x02000000;
    this->arm9AddrEnd = 0x03000000 - 0x100;

    // set the dialog's basic properties
    this->setObjectName("MemViewDialog");
    this->setFixedSize(730, 300);
    this->setEnabled(true);
    this->setModal(false);
    this->setWindowTitle("Memory Viewer - melonDS");
    setAttribute(Qt::WA_DeleteOnClose);

    // create the widgets, maybe not necessary to keep a reference to everything but whatever
    this->gfxScene = new CustomGraphicsScene(this);
    this->gfxView = new QGraphicsView(this);
    this->scrollBar = new QScrollBar(this);
    this->updateThread = new MemViewThread(this);
    this->addrLabel = new QLabel(this);
    this->addrValueLabel = new QLabel(this);
    this->updateRateLabel = new QLabel(this);
    this->isBigEndian = new QCheckBox(this);
    this->addrLineEdit = new QLineEdit(this);
    this->updateRate = new QSpinBox(this);
    this->valueGroup = new QGroupBox(this); 
    this->valueTypeSelect = new QComboBox(this->valueGroup);
    this->valueSetBtn = new QPushButton(this->valueGroup);
    this->valueLineEdit = new QLineEdit(this->valueGroup);
    this->valueAddrLineEdit = new QLineEdit(this->valueGroup);

    this->addrLabel->setText("Address:");
    this->addrLabel->setGeometry(10, 20, 58, 18);

    this->addrValueLabel->setText("0x00000000");
    this->addrValueLabel->setGeometry(74, 20, 81, 18);
    this->addrValueLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextEditorInteraction);

    this->addrLineEdit->setMaxLength(10);
    this->addrLineEdit->setPlaceholderText("Search...");
    this->addrLineEdit->setGeometry(8, 40, 144, 32);

    this->isBigEndian->setText("Big Endian");
    this->isBigEndian->setGeometry(5, 233, 131, 22);

    this->updateRateLabel->setText("Update:");
    this->updateRateLabel->setGeometry(7, 264, 58, 18);

    this->updateRate->setGeometry(61, 257, 91, 32);
    this->updateRate->setMinimum(5); // below 5 ms it's causing slowdowns
    this->updateRate->setMaximum(10000);
    this->updateRate->setValue(20);
    this->updateRate->setSuffix(" ms");

    this->gfxView->setScene(this->gfxScene);
    this->gfxView->setGeometry(160, 10, 550, 280);

    this->gfxScene->clear();
    this->gfxScene->setSceneRect(0, 0, 550, 280);
    this->gfxScene->SetScrollBar(this->scrollBar);

    this->scrollBar->setGeometry(710, 10, 16, 280);
    this->scrollBar->setMinimum(this->arm9AddrStart);
    this->scrollBar->setMaximum(this->arm9AddrEnd);
    this->scrollBar->setSingleStep(16);
    this->scrollBar->setPageStep(16);
    this->scrollBar->setOrientation(Qt::Orientation::Vertical);

    this->valueGroup->setGeometry(8, 80, 143, 111);

    this->valueTypeSelect->addItem("8 bits");
    this->valueTypeSelect->addItem("16 bits");
    this->valueTypeSelect->addItem("32 bits");
    this->valueTypeSelect->setCurrentIndex(0);
    this->valueTypeSelect->setGeometry(6, 75, 68, 30);

    this->valueSetBtn->setText("Set");
    this->valueSetBtn->setGeometry(this->valueTypeSelect->width() + 5, 75, 65, 30);

    this->valueLineEdit->setGeometry(7, 7, 129, 30);
    this->valueLineEdit->setPlaceholderText("Set value...");

    this->valueAddrLineEdit->setGeometry(7, 40, 129, 30);
    this->valueAddrLineEdit->setPlaceholderText("At address...");

    // initialize the scene
    QString text;
    QColor color;
    QFont font("Monospace", 10, -1, false);

    // create hex digits texts
    for (int i = 0; i < 16; i++) {
        text.setNum(i, 16);
        QGraphicsTextItem* textItem = new QGraphicsTextItem(text.toUpper().rightJustified(2, '0'));
        textItem->setParent(this->gfxScene);
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
        text.setNum(i * 16 + this->arm9AddrStart, 16);

        QGraphicsTextItem* textItem = new QGraphicsTextItem(text.toUpper().rightJustified(8, '0').prepend("0x"));
        textItem->setParent(this->gfxScene);
        textItem->setFont(font);
        textItem->setPos(0, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
        this->gfxScene->addItem(textItem);
        this->addresses[i] = textItem;
    }

    // init memory view
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            CustomTextItem* textItem = new CustomTextItem("00");
            textItem->setParent(this->gfxScene);
            textItem->setFont(font);
            qreal x = j * textItem->font().pointSize() * 2; // column number * font size * text length
            qreal y = 0; // always zero

            // account for addresses column length
            x += textItem->font().pointSize() * 8;
            textItem->setPos(x + 10, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
            this->gfxScene->addItem(textItem);
            this->items[i][j] = textItem;
        }
    }

    // init ascii view header
    QGraphicsTextItem* textItem = new QGraphicsTextItem("0123456789ABCDEF");
    textItem->setParent(this->gfxScene);
    textItem->setFont(font);
    textItem->setPos(416, 0);
    this->gfxScene->addItem(textItem);

    // init ascii view
    for (int i = 0; i < 16; i++) {
        QGraphicsTextItem* textItem = new QGraphicsTextItem("................");
        textItem->setParent(this->gfxScene);
        textItem->setFont(font);
        textItem->setPos(416, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
        this->gfxScene->addItem(textItem);
        this->asciiStrings[i] = textItem;
    }

    // add separators
    QGraphicsLineItem* line1 = this->gfxScene->addLine(0, 25, this->gfxScene->width(), 25, QPen(color));
    QGraphicsLineItem* line2 = this->gfxScene->addLine(87, 0, 87, this->gfxScene->height(), QPen(color));
    // QGraphicsLineItem* line3 = this->gfxScene->addLine(415, 0, 415, this->gfxScene->height(), QPen(color));

    this->updateThread->Start();
    connect(this->updateThread, &MemViewThread::updateTextSignal, this, &MemViewDialog::updateText);
    connect(this->updateThread, &MemViewThread::updateAddressSignal, this, &MemViewDialog::updateAddress);
    connect(this->updateThread, &MemViewThread::updateDecodedSignal, this, &MemViewDialog::updateDecoded);
    connect(this->addrLineEdit, &QLineEdit::textChanged, this, &MemViewDialog::onAddressTextChanged);
    connect(this->valueSetBtn, &QPushButton::pressed, this, &MemViewDialog::onValueBtnSetPressed);
    connect(this->gfxScene, &QGraphicsScene::focusItemChanged, this->gfxScene, &CustomGraphicsScene::onFocusItemChanged);

    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<u32>("u32");
    qRegisterMetaType<s32>("s32");
    qRegisterMetaType<s16>("s16");
    qRegisterMetaType<s8>("s8");
}

MemViewDialog::~MemViewDialog()
{
    if (this->updateThread) {
        disconnect(this->updateThread, nullptr, this, nullptr);
        this->updateThread->Stop();
        delete this->updateThread;
        this->updateThread = nullptr;
    }
}

melonDS::NDS* MemViewDialog::GetNDS() {
    // fetch the emulator's instance and the NDS data from the main window
    // we're doing it each time we need to access the NDS data to prevent a segfault later
    // when accessing MainRAM and DTCM
    EmuInstance* emuInstance = ((MainWindow*)this->parent())->getEmuInstance();

    if (emuInstance) {
        return emuInstance->getNDS();
    }

    return nullptr;
}

int MemViewDialog::GetAddressFromItem(CustomTextItem* item)  {
    if (item != nullptr) {
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 16; j++) {
                if (this->items[i][j] == item) {
                    QGraphicsTextItem* item = this->addresses[i];

                    if (item != nullptr) {
                        return item->toPlainText().remove(0, 2).toUInt(0, 16);
                    }

                    return -1;
                }
            }
        }
    }

    return -1;
}

void MemViewDialog::done(int r)
{
    QDialog::done(r);
    closeDlg();
}

#define ALIGN16(n) ((n + 0xF) & ~0xF)

void MemViewDialog::updateText(int addrIndex, int index) {
    CustomTextItem* item = (CustomTextItem*)this->GetItem(addrIndex, index);
    QString text;
    melonDS::NDS* nds = this->GetNDS();

    if (item != nullptr && nds != nullptr) {
        uint32_t address = ALIGN16(this->scrollBar->value()) + addrIndex * 16;
        uint8_t byte;

        if (address < 0x027E0000) {
            if (nds->MainRAM) {
                byte = nds->MainRAM[(address + index) & nds->MainRAMMask];
            }
        } else {
            if (nds->ARM9.DTCM) {
                byte = nds->ARM9.DTCM[(address + index) & 0xFFFF];
            }
        }

        // only update the text when the item isn't focused so we can edit it
        if (!item->hasFocus()) {
            text.setNum(byte, 16);
            item->setPlainText(text.toUpper().rightJustified(2, '0'));
        }

        if (index == 0) {
            this->decodedStrings[addrIndex].clear();
        }

        if (this->decodedStrings[addrIndex].length() < 16) {
            // decode printable characters otherwise just use a dot
            if (byte >= 0x20 && byte <= 0x7E) {
                this->decodedStrings[addrIndex].append(QChar(byte));
            } else {
                this->decodedStrings[addrIndex].append(QChar('.'));
            }
        }
    }
}

void MemViewDialog::updateAddress(int index) {
    QGraphicsTextItem* item = (QGraphicsTextItem*)this->GetAddressItem(index);
    QString text;

    if (item != nullptr) {
        uint32_t address = ALIGN16(this->scrollBar->value()) + index * 16;

        text.setNum(address, 16);
        item->setPlainText(text.toUpper().rightJustified(8, '0').prepend("0x"));
    }
}

void MemViewDialog::updateDecoded(int index) {
    QGraphicsTextItem* item = (QGraphicsTextItem*)this->GetAsciiItem(index);
    QString text;

    if (item != nullptr) {
        item->setPlainText(this->decodedStrings[index]);
    }
}

void MemViewDialog::onAddressTextChanged(const QString &text) {
    QString rawAddr = text;
    uint32_t addr = 0;

    // remove the 0x prefix
    if (text.startsWith("0x")) {
        rawAddr = rawAddr.remove(0, 2);
    }

    // make sure the address is the right length
    if (rawAddr.length() < 8) {
        return;
    }

    addr = rawAddr.toUInt(0, 16);

    // make sure the address is in the main memory (counting dtcm)
    if (addr < this->arm9AddrStart || addr > this->arm9AddrEnd + 0x100) {
        return;
    }

    // scroll bars are dumb and the `value()` method doesn't always return what you want as the step
    // so we have to align the value...
    uint32_t alignedAddr = ALIGN16(addr);

    // compensate if the alignment makes the address higher than the user input
    if (alignedAddr > addr) {
        alignedAddr -= 0x10;
    }

    // to go to the desired address we just have to set the value of the scroll bar
    // since the thread will handle updating the view based on that
    this->scrollBar->setValue(alignedAddr);

    QString val;
    val.setNum(addr, 16);
    this->addrValueLabel->setText(val.toUpper().rightJustified(8, '0').prepend("0x"));
}

void MemViewDialog::onValueBtnSetPressed() {
    QString text = this->valueAddrLineEdit->text();

    if (text.startsWith("0x")) {
        text.remove(0, 2);
    }
    uint32_t address = text.toUInt(0, 16);

    text = this->valueLineEdit->text();
    if (text.startsWith("0x")) {
        text.remove(0, 2);
    }

    melonDS::NDS* nds = this->GetNDS();
    void* pRAM = nullptr;
    if (nds != nullptr) {
        if (address < 0x027E0000) {
            if (nds->MainRAM) {
                pRAM = (void*)&nds->MainRAM[address & nds->MainRAMMask];
            }
        } else {
            if (nds->ARM9.DTCM) {
                pRAM = (void*)&nds->ARM9.DTCM[address & 0xFFFF];
            }
        }
    }

    if (pRAM != nullptr) {
        switch (this->valueTypeSelect->currentIndex()) {
            case 0: // 8 bits
                *((uint8_t*)pRAM) = text.toUInt(0, 16);
                break;
            case 1: // 16 bits
                *((uint16_t*)pRAM) = text.toUInt(0, 16);
                break;
            case 2: // 32 bits
                *((uint32_t*)pRAM) = text.toUInt(0, 16);
                break;
            default:
                return;
        }
    }
}

// --- MemViewThread ---

MemViewThread::~MemViewThread() {
    this->Stop();
    this->dialog = nullptr;
}

void MemViewThread::Start() {
    this->start();
    this->running = true;
}

void MemViewThread::Stop() {
    this->running = false;
    this->quit();
    this->wait();
}

void MemViewThread::run() {
    while (this->running) {
        if (!this->dialog || !this->dialog->updateRate) {
            return;
        }

        int time = this->dialog->updateRate->value();

        // make sure it's never below 5 ms, this can happen if you have an empty field
        time = time < 5 ? 5 : time;

        QThread::msleep(time);

        for (int i = 0; i < 16; i++) {
            emit updateAddressSignal(i);

            for (int j = 0; j < 16; j++) {
                emit updateTextSignal(i, j);
            }

            emit updateDecodedSignal(i);
        }
    }
}
