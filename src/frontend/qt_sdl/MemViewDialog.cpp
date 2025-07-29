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

// --- MemViewDialog ---

MemViewDialog* MemViewDialog::currentDlg = nullptr;

CustomTextItem::CustomTextItem(const QString &text, QGraphicsItem *parent) : QGraphicsTextItem(text, parent) {
    this->SetSelectionFlags();
    this->setFlag(QGraphicsItem::ItemIsSelectable, false);
    this->SetSize(QRectF(2, 6, 20, 15));
}

QRectF CustomTextItem::boundingRect() const {
    return this->size;
}

bool CustomTextItem::isKeyValid(int key) {
    // hex chars
    if ((key >= Qt::Key_0 && key <= Qt::Key_9) || (key >= Qt::Key_A && key <= Qt::Key_F)) {
        return true;
    }

    // navigation arrows
    if (key >= Qt::Key_Left && key <= Qt::Key_Down) {
        return true;
    }

    // other important keys
    switch (key) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            return true;
        default:
            break;
    }

    return false;
}

void CustomTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    this->SetSelectionFlags();
    this->QGraphicsTextItem::mousePressEvent(event);
    this->SetTextSelection(true);
}

void CustomTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    this->SetEditionFlags();
    this->QGraphicsTextItem::mouseDoubleClickEvent(event);
}

// empty on purpose to disable the move behaviour
void CustomTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {}

void CustomTextItem::keyPressEvent(QKeyEvent *event) {
    QString text = this->toPlainText();
    int key = event->key();

    // not accepting or ignoring the event to prevent closing the window
    if (key == Qt::Key_Escape) {
        this->clearFocus();
        return;
    }

    if (this->isEditing) {
        // make sure that:
        // - 1. the item is focused (probably always true though but wanna be safe)
        // - 2. the key is valid, so either an hex digit or other keys like enter or delete
        // - 3. the current text length don't exceed 2
        if (!this->hasFocus() || !this->isKeyValid(key) || text.length() > 2) {
            this->isEditing = false;
            event->ignore();
            return;
        }

        // turn enter keys to a validator
        if (key == Qt::Key_Enter || key == Qt::Key_Return) {
            if (this->isEditing && !text.isEmpty()) {
                emit applyEditToRAM(text.toUInt(0, 16), this);
                this->SetSelectionFlags();
                this->SetTextSelection(true);
            }
            event->accept();
            return;
        }

        // if the validation checks pass and it's not the enter key, process it
        this->QGraphicsTextItem::keyPressEvent(event);
    } else {
        switch (key) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
                this->SetEditionFlags();
                break;
            case Qt::Key_Left:
                emit switchFocus(FocusDirection_Left);
                break;
            case Qt::Key_Up:
                emit switchFocus(FocusDirection_Up);
                break;
            case Qt::Key_Right:
                emit switchFocus(FocusDirection_Right);
                break;
            case Qt::Key_Down:
                emit switchFocus(FocusDirection_Down);
                break;
            default:
                event->ignore();
                return;
        }

        event->accept();
    }
}

void CustomTextItem::focusOutEvent(QFocusEvent *event) {
    QString text = this->toPlainText();

    // apply the value when the focus is cleared, if necessary
    if (this->isEditing && !text.isEmpty()) {
        emit applyEditToRAM(text.toUInt(0, 16), this);
        this->isEditing = false;
    }

    this->SetTextSelection(false);
    this->QGraphicsTextItem::focusOutEvent(event);
}

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
        uint32_t addr = dialog->GetFocusAddress(newFocus);

        if (addr != -1) {
            QString text;
            text.setNum(addr, 16);
            dialog->GetAddrLabel()->setText(text.toUpper().rightJustified(8, '0').prepend("0x"));

            if (dialog->GetFocusCheckbox()->isChecked()) {
                dialog->GetValueAddrLineEdit()->setText(text.toUpper().rightJustified(8, '0').prepend("0x"));
            }
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
    this->addrDescLabel = new QLabel(this);
    this->addrLabel = new QLabel(this);
    this->updateRateLabel = new QLabel(this);
    this->searchLineEdit = new QLineEdit(this);
    this->updateRate = new QSpinBox(this);
    this->setValGroup = new QGroupBox(this); 
    this->setValFocus = new QCheckBox(this->setValGroup);
    this->setValBits = new QComboBox(this->setValGroup);
    this->setValBtn = new QPushButton(this->setValGroup);
    this->setValNumber = new QLineEdit(this->setValGroup);
    this->setValAddr = new QLineEdit(this->setValGroup);

    this->addrDescLabel->setText("Address:");
    this->addrDescLabel->setGeometry(10, 20, 58, 18);

    this->addrLabel->setText("0x02000000");
    this->addrLabel->setGeometry(74, 20, 81, 18);
    this->addrLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextEditorInteraction);

    this->searchLineEdit->setMaxLength(10);
    this->searchLineEdit->setPlaceholderText("Search...");
    this->searchLineEdit->setGeometry(8, 40, 144, 32);

    this->setValFocus->setText("Address on focus");
    this->setValFocus->setGeometry(4, 106, 131, 22);

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

    this->setValGroup->setGeometry(8, 80, 143, 131);

    this->setValBits->addItem("8 bits");
    this->setValBits->addItem("16 bits");
    this->setValBits->addItem("32 bits");
    this->setValBits->setCurrentIndex(0);
    this->setValBits->setGeometry(6, 75, 68, 30);

    this->setValBtn->setText("Set");
    this->setValBtn->setGeometry(this->setValBits->width() + 5, 75, 65, 30);

    this->setValNumber->setGeometry(7, 7, 129, 30);
    this->setValNumber->setPlaceholderText("Value");

    this->setValAddr->setGeometry(7, 40, 129, 30);
    this->setValAddr->setPlaceholderText("Address");

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
        this->leftAddrItems[i] = textItem;
    }

    // init memory view
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            CustomTextItem* textItem = new CustomTextItem("00");
            connect(textItem, &CustomTextItem::applyEditToRAM, this, &MemViewDialog::onApplyEditToRAM);
            connect(textItem, &CustomTextItem::switchFocus, this, &MemViewDialog::onSwitchFocus);
            textItem->setParent(this->gfxScene);
            textItem->setFont(font);
            qreal x = j * textItem->font().pointSize() * 2; // column number * font size * text length
            qreal y = 0; // always zero

            // account for addresses column length
            x += textItem->font().pointSize() * 8;
            textItem->setPos(x + 10, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
            this->gfxScene->addItem(textItem);
            this->ramTextItems[i][j] = textItem;
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
    connect(this->searchLineEdit, &QLineEdit::textChanged, this, &MemViewDialog::onAddressTextChanged);
    connect(this->setValBtn, &QPushButton::pressed, this, &MemViewDialog::onValueBtnSetPressed);
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
                if (this->ramTextItems[i][j] == item) {
                    QGraphicsTextItem* item = this->leftAddrItems[i];

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

void* MemViewDialog::GetRAM(uint32_t address) {
    melonDS::NDS* nds = this->GetNDS();

    //! TODO: use Armv5::ReadMem instead
    if (nds != nullptr) {
        if (address < 0x027E0000) {
            if (nds->MainRAM) {
                return &nds->MainRAM[address & nds->MainRAMMask];
            }
        } else {
            if (nds->ARM9.DTCM) {
                return &nds->ARM9.DTCM[address & 0xFFFF];
            }
        }
    }

    return nullptr;
}

uint32_t MemViewDialog::GetFocusAddress(QGraphicsItem *focus) {
    if (focus != nullptr) {
        uint32_t addr = this->GetAddressFromItem((CustomTextItem*)focus);
        int index = this->GetItemIndex(focus) & 0xFF;

        if (addr >= this->arm9AddrEnd + 0x100) {
            addr = this->arm9AddrEnd;
        }

        if (index >= 0) {
            return addr + index;
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

    if (this->scrollBar == nullptr) {
        return;
    }

    uint32_t address = ALIGN16(this->scrollBar->value()) + addrIndex * 16;
    uint8_t* pRAM = (uint8_t*)this->GetRAM(address + index);

    if (item != nullptr && pRAM != nullptr) {
        uint8_t byte = *pRAM;

        // only update the text when the item isn't focused so we can edit it
        if (!item->hasFocus() || this->forceTextUpdate) {
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
    this->StripStringHex(&rawAddr);

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
    this->addrLabel->setText(val.toUpper().rightJustified(8, '0').prepend("0x"));
}

void MemViewDialog::onValueBtnSetPressed() {
    QString text = this->setValAddr->text();

    // don't do anything if the address/value isn't set
    if (text.isEmpty() || this->setValNumber->text().isEmpty()) {
        return;
    }

    this->StripStringHex(&text);
    uint32_t address = text.toUInt(0, 16);

    text = this->setValNumber->text();
    this->StripStringHex(&text);

    void* pRAM = this->GetRAM(address);

    if (pRAM != nullptr) {
        switch (this->setValBits->currentIndex()) {
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

void MemViewDialog::onApplyEditToRAM(uint8_t value, QGraphicsItem *focus) {
    uint32_t addr = this->GetFocusAddress(focus);
    if (addr == -1) {
        return;
    }

    uint8_t* pRAM = (uint8_t*)this->GetRAM(addr);
    if (pRAM == nullptr) {
        return;
    }

    *pRAM = value;
}

void MemViewDialog::onSwitchFocus(FocusDirection eDirection) {
    CustomTextItem* focusItem = (CustomTextItem*)this->gfxScene->focusItem();

    if (focusItem == nullptr) {
        return;
    }

    int packed = this->GetItemIndex(focusItem);
    int scrollValue = this->scrollBar->value();

    if (packed == -1) {
        return;
    }

    int index = packed & 0xFF;
    int addrIndex = (packed >> 8) & 0xFF;

    /* 
        Different things will happen depending on the position and the direction:
            - direction up: decrease the address index if above 0, otherwise decrease the scroll bar's value by 16,
            - direction down: increase the address index if under 15, otherwise increase the scroll bar's value by 16,
            - direction left: decrease item index if above 0, otherwise set it to 15 and do the same actions as going up,
            - direction right: increase item index if under 15, otherwise set it to 0 and do the same actions as going down,
        and of course return immediately if the direction value is not valid, but that should never happen
    */

    switch (eDirection) {
        case FocusDirection_Left:
            if (index == 0) {
                index = 15;
                // fallthrough
            } else {
                index--;
                break;
            }
        case FocusDirection_Up:
            if (addrIndex == 0) {
                scrollValue -= 0x10;
            } else {
                addrIndex--;
            }
            break;
        case FocusDirection_Right:
            if (index == 15) {
                index = 0;
                // fallthrough
            } else {
                index++;
                break;
            }
        case FocusDirection_Down:
            if (addrIndex == 15) {
                scrollValue += 0x10;
            } else {
                addrIndex++;
            }
            break;
        default:
            return;
    }

    CustomTextItem* item = (CustomTextItem*)this->GetItem(addrIndex, index);
    if (item != nullptr) {
        this->gfxScene->setFocusItem(item);

        if (scrollValue >= this->arm9AddrStart && scrollValue <= this->arm9AddrEnd) {
            this->scrollBar->setValue(scrollValue);

            // updateText requires to check for hasFocus to prevent deselecting the text
            // so we need to force an update to make sure what we focus has the right value
            this->forceTextUpdate = true;
            this->updateText(addrIndex, index);
            this->forceTextUpdate = false;
            item->SetTextSelection(true);
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
