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

CustomTextItem::CustomTextItem(const QString &text, QGraphicsItem *parent) : QGraphicsTextItem(text, parent)
{
    this->SetSelectionFlags();
    this->setFlag(QGraphicsItem::ItemIsSelectable, false);
    this->SetSize(QRectF(2, 6, 20, 15));
}

QRectF CustomTextItem::boundingRect() const
{
    return this->Size;
}

bool CustomTextItem::IsKeyValid(int key)
{
    // hex chars
    if (this->IsKeyHex(key))
    {
        return true;
    }

    // navigation arrows
    if (key >= Qt::Key_Left && key <= Qt::Key_Down)
    {
        return true;
    }

    // other important keys
    switch (key)
    {
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

void CustomTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    this->SetSelectionFlags();
    this->QGraphicsTextItem::mousePressEvent(event);
    this->SetTextSelection(true);
}

void CustomTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    this->SetEditionFlags();
    this->QGraphicsTextItem::mouseDoubleClickEvent(event);
}

// empty on purpose to disable the move behaviour
void CustomTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
}

void CustomTextItem::keyPressEvent(QKeyEvent *event)
{
    QString text = this->toPlainText();
    int key = event->key();

    // not accepting or ignoring the event to prevent closing the window
    if (key == Qt::Key_Escape)
    {
        this->clearFocus();
        return;
    }

    if (this->IsEditing)
    {
        QTextCursor cursor = this->textCursor();
        int cursorPos = cursor.position();
        int anchorPos = cursor.anchor();
        bool hasSelection = cursor.hasSelection();
        qsizetype textLength = text.length();

        // make sure that:
        // - 1. the item is focused (probably always true though but wanna be safe)
        // - 2. the key is valid, so either an hex digit or other keys like enter or delete
        // - 3. if the key is a hex digit (0-9 A-F), make sure we won't exceed a length of 2
        if (!this->hasFocus() || !this->IsKeyValid(key) || (this->IsKeyHex(key) && (cursorPos + 1) < 2 && (textLength + 1) > 2))
        {
            this->IsEditing = false;
            event->ignore();
            return;
        }

        // turn enter keys to a validator
        if (key == Qt::Key_Enter || key == Qt::Key_Return)
        {
            if (this->IsEditing && !text.isEmpty())
            {
                emit applyEditToRAM(text.toUInt(0, 16), this);
                this->SetSelectionFlags();
                this->SetTextSelection(true);
            }
            event->accept();
            return;
        }

        // if the validation checks pass and it's not the enter key, process it
        this->QGraphicsTextItem::keyPressEvent(event);

        // skip to the next byte and enter edition mode (handled by the signal)
        if (this->IsKeyHex(key) && this->toPlainText().length() == 2 && !hasSelection)
        {
            emit switchFocus(focusDirection_Right, focusAction_SetEditionModeNoPos);
        }
    }
    else
    {
        switch (key)
        {
            case Qt::Key_Enter:
            case Qt::Key_Return:
                this->SetEditionFlags();
                break;
            case Qt::Key_Left:
                emit switchFocus(focusDirection_Left, focusAction_SetSelectionMode);
                break;
            case Qt::Key_Up:
                emit switchFocus(focusDirection_Up, focusAction_SetSelectionMode);
                break;
            case Qt::Key_Right:
                emit switchFocus(focusDirection_Right, focusAction_SetSelectionMode);
                break;
            case Qt::Key_Down:
                emit switchFocus(focusDirection_Down, focusAction_SetSelectionMode);
                break;
            default:
                event->ignore();
                return;
        }

        event->accept();
    }
}

void CustomTextItem::focusOutEvent(QFocusEvent *event)
{
    QString text = this->toPlainText();

    // apply the value when the focus is cleared, if necessary
    if (this->IsEditing && !text.isEmpty())
    {
        emit applyEditToRAM(text.toUInt(0, 16), this);
        this->IsEditing = false;
    }

    this->SetTextSelection(false);
    this->QGraphicsTextItem::focusOutEvent(event);
}

void CustomGraphicsScene::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    this->QGraphicsScene::wheelEvent(event);

    if (this->ScrollBar != nullptr)
    {
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

        QApplication::sendEvent(this->ScrollBar, pEvent);
    }
}

void CustomGraphicsScene::onFocusItemChanged(QGraphicsItem *newFocus, QGraphicsItem *oldFocus, Qt::FocusReason reason)
{
    MemViewDialog* dialog = (MemViewDialog*)this->parent();

    if (dialog != nullptr && newFocus != nullptr)
    {
        uint32_t addr = dialog->GetFocusAddress(newFocus);

        if (addr != -1)
        {
            QString text;
            text.setNum(addr, 16);
            dialog->GetAddrLabel()->setText(text.toUpper().rightJustified(8, '0').prepend("0x"));

            if (dialog->GetFocusCheckbox()->isChecked())
            {
                dialog->GetValueAddrLineEdit()->setText(text.toUpper().rightJustified(8, '0').prepend("0x"));
            }
        }
    }
}

MemViewDialog::MemViewDialog(QWidget* parent) : QDialog(parent)
{
    this->ARM9AddrStart = 0x02000000;
    this->ARM9AddrEnd = 0x03000000 - 0x100;

    // set the Dialog's basic properties
    this->setObjectName("MemViewDialog");
    this->setFixedSize(730, 300);
    this->setEnabled(true);
    this->setModal(false);
    this->setWindowTitle("Memory Viewer - melonDS");
    setAttribute(Qt::WA_DeleteOnClose);

    QColor placeholderColor = QColor(160, 160, 160);

    // create the widgets, maybe not necessary to keep a reference to everything but whatever
    this->GfxScene = new CustomGraphicsScene(this);
    this->GfxView = new QGraphicsView(this);
    this->ScrollBar = new QScrollBar(this);
    this->UpdateThread = new MemViewThread(this);
    this->AddrDescLabel = new QLabel(this);
    this->AddrLabel = new QLabel(this);
    this->UpdateRateLabel = new QLabel(this);
    this->SearchLineEdit = new QLineEdit(this);
    this->UpdateRate = new QSpinBox(this);
    this->SetValGroup = new QGroupBox(this); 
    this->SetValFocus = new QCheckBox(this->SetValGroup);
    this->SetValBits = new QComboBox(this->SetValGroup);
    this->SetValBtn = new QPushButton(this->SetValGroup);
    this->SetValNumber = new QLineEdit(this->SetValGroup);
    this->SetValAddr = new QLineEdit(this->SetValGroup);

    this->AddrDescLabel->setText("Address:");
    this->AddrDescLabel->setGeometry(10, 20, 58, 18);

    this->AddrLabel->setText("0x02000000");
    this->AddrLabel->setGeometry(74, 20, 81, 18);
    this->AddrLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextEditorInteraction);

    this->SearchLineEdit->setMaxLength(10);
    this->SearchLineEdit->setPlaceholderText("Search...");
    this->SearchLineEdit->setGeometry(8, 40, 144, 32);
    QPalette paletteSearch = this->SearchLineEdit->palette();
    paletteSearch.setColor(QPalette::ColorRole::PlaceholderText, placeholderColor);
    this->SearchLineEdit->setPalette(paletteSearch);

    this->SetValFocus->setText("Address on focus");
    this->SetValFocus->setGeometry(4, 106, 131, 22);

    this->UpdateRateLabel->setText("Update:");
    this->UpdateRateLabel->setGeometry(7, 264, 58, 18);

    this->UpdateRate->setGeometry(61, 257, 91, 32);
    this->UpdateRate->setMinimum(5); // below 5 ms it's causing slowdowns
    this->UpdateRate->setMaximum(10000);
    this->UpdateRate->setValue(20);
    this->UpdateRate->setSuffix(" ms");

    this->GfxView->setScene(this->GfxScene);
    this->GfxView->setGeometry(160, 10, 550, 280);

    this->GfxScene->clear();
    this->GfxScene->setSceneRect(0, 0, 550, 280);
    this->GfxScene->SetScrollBar(this->ScrollBar);

    this->ScrollBar->setGeometry(710, 10, 16, 280);
    this->ScrollBar->setMinimum(this->ARM9AddrStart);
    this->ScrollBar->setMaximum(this->ARM9AddrEnd);
    this->ScrollBar->setSingleStep(16);
    this->ScrollBar->setPageStep(16);
    this->ScrollBar->setOrientation(Qt::Orientation::Vertical);

    this->SetValGroup->setGeometry(8, 80, 143, 131);

    this->SetValBits->addItem("8 bits");
    this->SetValBits->addItem("16 bits");
    this->SetValBits->addItem("32 bits");
    this->SetValBits->setCurrentIndex(0);
    this->SetValBits->setGeometry(6, 75, 68, 30);

    this->SetValBtn->setText("Set");
    this->SetValBtn->setGeometry(this->SetValBits->width() + 5, 75, 65, 30);

    this->SetValNumber->setGeometry(7, 7, 129, 30);
    this->SetValNumber->setPlaceholderText("Value");
    QPalette paletteNum = this->SetValNumber->palette();
    paletteNum.setColor(QPalette::ColorRole::PlaceholderText, placeholderColor);
    this->SetValNumber->setPalette(paletteNum);

    this->SetValAddr->setGeometry(7, 40, 129, 30);
    this->SetValAddr->setPlaceholderText("Address");
    QPalette paletteAddr = this->SetValAddr->palette();
    paletteAddr.setColor(QPalette::ColorRole::PlaceholderText, placeholderColor);
    this->SetValAddr->setPalette(paletteAddr);

    // initialize the scene
    QString text;
    QColor color;
    QFont font("Monospace", 10, -1, false);

    // create hex digits texts
    for (int i = 0; i < 16; i++)
    {
        text.setNum(i, 16);
        QGraphicsTextItem* textItem = new QGraphicsTextItem(text.toUpper().rightJustified(2, '0'));
        textItem->setParent(this->GfxScene);
        textItem->setFont(font);
        qreal x = i * textItem->font().pointSize() * 2; // column number * font size * text length
        qreal y = 0; // always zero

        // account for addresses column length
        x += textItem->font().pointSize() * 8;

        textItem->setPos(x + 10, y);
        this->GfxScene->addItem(textItem);

        // kinda hacky but we need to sync the line's color on the text
        // and since the text color depends on the system's theme... easy way for now...
        if (i == 0)
        {
            color = textItem->defaultTextColor();
        }
    }

    // create addresses texts
    for (int i = 0; i < 16; i++)
    {
        text.setNum(i * 16 + this->ARM9AddrStart, 16);

        QGraphicsTextItem* textItem = new QGraphicsTextItem(text.toUpper().rightJustified(8, '0').prepend("0x"));
        textItem->setParent(this->GfxScene);
        textItem->setFont(font);
        textItem->setPos(0, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
        this->GfxScene->addItem(textItem);
        this->LeftAddrItems[i] = textItem;
    }

    // init memory view
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            CustomTextItem* textItem = new CustomTextItem("00");
            connect(textItem, &CustomTextItem::applyEditToRAM, this, &MemViewDialog::onApplyEditToRAM);
            connect(textItem, &CustomTextItem::switchFocus, this, &MemViewDialog::onSwitchFocus);
            textItem->setParent(this->GfxScene);
            textItem->setFont(font);
            qreal x = j * textItem->font().pointSize() * 2; // column number * font size * text length
            qreal y = 0; // always zero

            // account for addresses column length
            x += textItem->font().pointSize() * 8;
            textItem->setPos(x + 10, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
            this->GfxScene->addItem(textItem);
            this->RAMTextItems[i][j] = textItem;
        }
    }

    // init ascii view header
    QGraphicsTextItem* textItem = new QGraphicsTextItem("0123456789ABCDEF");
    textItem->setParent(this->GfxScene);
    textItem->setFont(font);
    textItem->setPos(416, 0);
    this->GfxScene->addItem(textItem);

    // init ascii view
    for (int i = 0; i < 16; i++)
    {
        QGraphicsTextItem* textItem = new QGraphicsTextItem("................");
        textItem->setParent(this->GfxScene);
        textItem->setFont(font);
        textItem->setPos(416, i * textItem->font().pointSize() * 1.5f + textItem->font().pointSize() + 15);
        this->GfxScene->addItem(textItem);
        this->AsciiStrings[i] = textItem;
    }

    // add separators
    QGraphicsLineItem* line1 = this->GfxScene->addLine(0, 25, this->GfxScene->width(), 25, QPen(color));
    QGraphicsLineItem* line2 = this->GfxScene->addLine(87, 0, 87, this->GfxScene->height(), QPen(color));
    // QGraphicsLineItem* line3 = this->GfxScene->addLine(415, 0, 415, this->GfxScene->height(), QPen(color));

    this->UpdateThread->Start();
    connect(this->UpdateThread, &MemViewThread::updateTextSignal, this, &MemViewDialog::UpdateText);
    connect(this->UpdateThread, &MemViewThread::updateAddressSignal, this, &MemViewDialog::UpdateAddress);
    connect(this->UpdateThread, &MemViewThread::updateDecodedSignal, this, &MemViewDialog::UpdateDecoded);
    connect(this->SearchLineEdit, &QLineEdit::textChanged, this, &MemViewDialog::onAddressTextChanged);
    connect(this->SetValBtn, &QPushButton::pressed, this, &MemViewDialog::onValueBtnSetPressed);
    connect(this->GfxScene, &QGraphicsScene::focusItemChanged, this->GfxScene, &CustomGraphicsScene::onFocusItemChanged);
    connect(this->ScrollBar, &QScrollBar::valueChanged, this, &MemViewDialog::onScrollBarValueChanged);

    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<u32>("u32");
    qRegisterMetaType<s32>("s32");
    qRegisterMetaType<s16>("s16");
    qRegisterMetaType<s8>("s8");
}

MemViewDialog::~MemViewDialog()
{
    if (this->UpdateThread)
    {
        disconnect(this->UpdateThread, nullptr, this, nullptr);
        this->UpdateThread->Stop();
        delete this->UpdateThread;
        this->UpdateThread = nullptr;
    }
}

melonDS::NDS* MemViewDialog::GetNDS()
{
    // fetch the emulator's instance and the NDS data from the main window
    // we're doing it each time we need to access the NDS data to prevent a segfault later
    // when accessing MainRAM and DTCM
    EmuInstance* emuInstance = ((MainWindow*)this->parent())->getEmuInstance();

    if (emuInstance)
    {
        return emuInstance->getNDS();
    }

    return nullptr;
}

int MemViewDialog::GetAddressFromItem(CustomTextItem* item) 
{
    if (item != nullptr)
    {
        for (int i = 0; i < 16; i++)
        {
            for (int j = 0; j < 16; j++)
            {
                if (this->RAMTextItems[i][j] == item)
                {
                    QGraphicsTextItem* item = this->LeftAddrItems[i];

                    if (item != nullptr)
                    {
                        return item->toPlainText().remove(0, 2).toUInt(0, 16);
                    }

                    return -1;
                }
            }
        }
    }

    return -1;
}

void* MemViewDialog::GetRAM(uint32_t address)
{
    melonDS::NDS* nds = this->GetNDS();

    if (nds != nullptr && nds->ARM9.DTCM != nullptr && nds->MainRAM != nullptr)
    {
        if (address < nds->ARM9.ITCMSize)
        {
            return &nds->ARM9.ITCM[address & (ITCMPhysicalSize - 1)];
        }
        else if ((address & nds->ARM9.DTCMMask) == nds->ARM9.DTCMBase)
        {
            return &nds->ARM9.DTCM[address & (DTCMPhysicalSize - 1)];
        }
        else
        {
            return &nds->MainRAM[address & nds->MainRAMMask];
        }
    }

    return nullptr;
}

uint32_t MemViewDialog::GetFocusAddress(QGraphicsItem *focus)
{
    if (focus != nullptr)
    {
        uint32_t addr = this->GetAddressFromItem((CustomTextItem*)focus);
        int index = this->GetItemIndex(focus) & 0xFF;

        if (addr >= this->ARM9AddrEnd + 0x100)
        {
            addr = this->ARM9AddrEnd;
        }

        if (index >= 0)
        {
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

void MemViewDialog::UpdateText(int addrIndex, int index)
{
    CustomTextItem* item = (CustomTextItem*)this->GetItem(addrIndex, index);
    QString text;

    if (this->ScrollBar == nullptr)
    {
        return;
    }

    uint32_t address = ALIGN16(this->ScrollBar->value()) + addrIndex * 16;
    uint8_t* pRAM = (uint8_t*)this->GetRAM(address + index);

    if (item != nullptr && pRAM != nullptr)
    {
        uint8_t byte = *pRAM;

        // only update the text when the item isn't focused so we can edit it
        if (!item->hasFocus() || this->ForceTextUpdate)
        {
            text.setNum(byte, 16);
            item->setPlainText(text.toUpper().rightJustified(2, '0'));
        }

        if (index == 0)
        {
            this->DecodedStrings[addrIndex].clear();
        }

        if (this->DecodedStrings[addrIndex].length() < 16)
        {
            // decode printable characters otherwise just use a dot
            if (byte >= 0x20 && byte <= 0x7E)
            {
                this->DecodedStrings[addrIndex].append(QChar(byte));
            }
            else
            {
                this->DecodedStrings[addrIndex].append(QChar('.'));
            }
        }
    }

    if (this->ForceTextUpdate) {
        this->ForceTextUpdate = false;
    }
}

void MemViewDialog::UpdateAddress(int index)
{
    QGraphicsTextItem* item = (QGraphicsTextItem*)this->GetAddressItem(index);
    QString text;

    if (item != nullptr)
    {
        uint32_t address = ALIGN16(this->ScrollBar->value()) + index * 16;

        text.setNum(address, 16);
        item->setPlainText(text.toUpper().rightJustified(8, '0').prepend("0x"));
    }
}

void MemViewDialog::UpdateDecoded(int index)
{
    QGraphicsTextItem* item = (QGraphicsTextItem*)this->GetAsciiItem(index);
    QString text;

    if (item != nullptr)
    {
        item->setPlainText(this->DecodedStrings[index]);
    }
}

void MemViewDialog::onAddressTextChanged(const QString &text)
{
    QString rawAddr = text;
    uint32_t addr = 0;

    // remove the 0x prefix
    this->StripStringHex(&rawAddr);

    // make sure the address is the right length
    if (rawAddr.length() < 8)
    {
        return;
    }

    addr = rawAddr.toUInt(0, 16);

    // make sure the address is in the main memory (counting dtcm)
    if (addr < this->ARM9AddrStart || addr > this->ARM9AddrEnd + 0x100)
    {
        return;
    }

    // scroll bars are dumb and the `value()` method doesn't always return what you want as the step
    // so we have to align the value...
    uint32_t alignedAddr = ALIGN16(addr);

    // compensate if the alignment makes the address higher than the user input
    if (alignedAddr > addr)
    {
        alignedAddr -= 0x10;
    }

    // to go to the desired address we just have to set the value of the scroll bar
    // since the thread will handle updating the view based on that
    this->ScrollBar->setValue(alignedAddr);

    QString val;
    val.setNum(addr, 16);
    this->AddrLabel->setText(val.toUpper().rightJustified(8, '0').prepend("0x"));
}

void MemViewDialog::onValueBtnSetPressed()
{
    QString text = this->SetValAddr->text();

    // don't do anything if the address/value isn't set
    if (text.isEmpty() || this->SetValNumber->text().isEmpty())
    {
        return;
    }

    this->StripStringHex(&text);
    uint32_t address = text.toUInt(0, 16);

    text = this->SetValNumber->text();
    this->StripStringHex(&text);

    void* pRAM = this->GetRAM(address);

    if (pRAM != nullptr)
    {
        switch (this->SetValBits->currentIndex())
        {
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

void MemViewDialog::onApplyEditToRAM(uint8_t value, QGraphicsItem *focus)
{
    uint32_t addr = this->GetFocusAddress(focus);
    if (addr == -1)
    {
        return;
    }

    uint8_t* pRAM = (uint8_t*)this->GetRAM(addr);
    if (pRAM != nullptr)
    {
        *pRAM = value;
    }
}

void MemViewDialog::onSwitchFocus(FocusDirection eDirection, FocusAction eAction)
{
    CustomTextItem* focusItem = (CustomTextItem*)this->GfxScene->focusItem();

    if (focusItem == nullptr)
    {
        return;
    }

    int packed = this->GetItemIndex(focusItem);
    int scrollValue = this->ScrollBar->value();

    if (packed == -1)
    {
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

    switch (eDirection)
    {
        case focusDirection_Left:
            if (index == 0)
            {
                index = 15;
                // fallthrough
            }
            else
            {
                index--;
                break;
            }
        case focusDirection_Up:
            if (addrIndex == 0)
            {
                scrollValue -= 0x10;
            }
            else
            {
                addrIndex--;
            }
            break;
        case focusDirection_Right:
            if (index == 15)
            {
                index = 0;
                // fallthrough
            }
            else
            {
                index++;
                break;
            }
        case focusDirection_Down:
            if (addrIndex == 15)
            {
                scrollValue += 0x10;
            }
            else
            {
                addrIndex++;
            }
            break;
        default:
            return;
    }

    CustomTextItem* item = (CustomTextItem*)this->GetItem(addrIndex, index);
    if (item != nullptr)
    {
        this->GfxScene->setFocusItem(item);

        if (scrollValue >= this->ARM9AddrStart && scrollValue <= this->ARM9AddrEnd)
        {
            this->ScrollBar->setValue(scrollValue);

            // UpdateText requires to check for hasFocus to prevent deselecting the text
            // so we need to force an update to make sure what we focus has the right value
            this->ForceTextUpdate = true;
            this->UpdateText(addrIndex, index);

            switch (eAction) {
                case focusAction_SetSelectionMode:
                    item->SetSelectionFlags();
                    item->SetTextSelection(true);
                    break;
                case focusAction_SetEditionModeNoPos:
                    item->SetTextSelection(true);
                    // fallthrough
                case focusAction_SetEditionMode:
                    item->SetEditionFlags();

                    if (eAction == focusAction_SetEditionMode) {
                        item->SetCursorPosition(eDirection == focusDirection_Left ? 0 : 2);
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

void MemViewDialog::onScrollBarValueChanged(int value) {
    CustomTextItem* item = (CustomTextItem*)this->GfxScene->focusItem();
    int packed = this->GetItemIndex(item);

    if (item != nullptr && packed != -1)
    {
        this->ForceTextUpdate = true;
        this->UpdateText(packed >> 8, packed & 0xFF);
        item->SetTextSelection(true);
    }
}

// --- MemViewThread ---

MemViewThread::~MemViewThread()
{
    this->Stop();
    this->Dialog = nullptr;
}

void MemViewThread::Start()
{
    this->start();
    this->Running = true;
}

void MemViewThread::Stop()
{
    this->Running = false;
    this->quit();
    this->wait();
}

void MemViewThread::run()
{
    while (this->Running)
    {
        if (!this->Dialog || !this->Dialog->UpdateRate)
        {
            return;
        }

        int time = this->Dialog->UpdateRate->value();

        // make sure it's never below 5 ms, this can happen if you have an empty field
        time = time < 5 ? 5 : time;

        QThread::msleep(time);

        for (int i = 0; i < 16; i++)
        {
            emit updateAddressSignal(i);

            for (int j = 0; j < 16; j++)
            {
                emit updateTextSignal(i, j);
            }

            emit updateDecodedSignal(i);
        }
    }
}
