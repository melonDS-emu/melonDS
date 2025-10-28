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

#ifndef MEMVIEWDIALOG_H
#define MEMVIEWDIALOG_H

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QThread>
#include <QScrollBar>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>

#include "types.h"
#include "NDS.h"

class NDS;
class EmuInstance;
class MemViewThread;

typedef enum HighlightState {
    hightlightState_Init,
    hightlightState_Draw,
    hightlightState_Stop,
} HighlightState;

typedef enum FocusDirection
{
    focusDirection_Up,
    focusDirection_Down,
    focusDirection_Left,
    focusDirection_Right,
} FocusDirection;

typedef enum FocusAction
{
    focusAction_SetSelectionMode,
    focusAction_SetEditionMode,
    focusAction_SetEditionModeNoPos,
} FocusAction;

// this need to match the order of the item from `MemRegionBox`.
enum MemRegionType {
    memRegionType_Default,
    memRegionType_ITCM,
    memRegionType_Main,
    memRegionType_DTCM,
    memRegionType_SWRAM,
    memRegionType_Palettes,
    memRegionType_OAM,
    memRegionType_BIOS,
};

// --- main window ---

class CustomTextItem : public QGraphicsTextItem
{
    Q_OBJECT

public:
    explicit CustomTextItem(const QString &text, QGraphicsItem *parent = nullptr);
    ~CustomTextItem() {}
    QRectF boundingRect() const override;
    bool IsKeyValid(int key);
    void setPlainText(const QString &text, bool highlight);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void SetSize(QRectF newSize)
    {
        this->Size = newSize;
    }

    void SetEditionFlags()
    {
        this->IsEditing = true;
        this->setTextInteractionFlags(Qt::TextInteractionFlag::TextEditorInteraction);
    }

    void SetSelectionFlags()
    {
        this->IsEditing = false;
        this->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse);
    }

    void SetTextSelection(bool selected)
    {
        QTextCursor cursor = this->textCursor();

        if (selected)
        {
            cursor.select(QTextCursor::Document);
        } else
        {
            cursor.clearSelection();
        }

        this->setTextCursor(cursor);
    }

    bool IsTextSelected()
    {
        QTextCursor cursor = this->textCursor();
        return cursor.hasSelection();
    }

    void SetCursorPosition(int value)
    {
        QTextCursor cursor = this->textCursor();
        cursor.setPosition(value);
        this->setTextCursor(cursor);
    }

    int GetCursorPosition()
    {
        QTextCursor cursor = this->textCursor();
        return cursor.position();
    }

    int GetCursorAnchor()
    {
        QTextCursor cursor = this->textCursor();
        return cursor.anchor();
    }

    bool IsKeyHex(int key)
    {
        return (key >= Qt::Key_0 && key <= Qt::Key_9) || (key >= Qt::Key_A && key <= Qt::Key_F);
    }

signals:
    void applyEditToRAM(uint8_t value, QGraphicsItem *focus);
    void switchFocus(FocusDirection eDirection, FocusAction eAction);

private:
    QRectF Size;
    bool IsEditing;
    QString PrevValue;
    int Alpha;
    HighlightState eHighlightState;

    bool HasValueChanged()
    {
        return this->PrevValue != this->toPlainText();
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
};

class CustomGraphicsScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit CustomGraphicsScene(QObject *parent = nullptr) : QGraphicsScene(parent)
    {}
    ~CustomGraphicsScene()
    {}

    void SetScrollBar(QScrollBar* pScrollBar)
    {
        this->ScrollBar = pScrollBar;
    }

public slots:
    void onFocusItemChanged(QGraphicsItem *newFocus, QGraphicsItem *oldFocus, Qt::FocusReason reason);

private:
    QScrollBar* ScrollBar;

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);
};

class CustomLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    CustomLineEdit(QWidget *parent = nullptr) : QLineEdit(parent) {}

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

class MemViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MemViewDialog(QWidget* parent);
    ~MemViewDialog();
    melonDS::NDS* GetNDS();
    int GetAddressFromItem(CustomTextItem* item);
    void* GetRAM(uint32_t address);
    uint32_t GetFocusAddress(QGraphicsItem *focus);
    void SwitchMemRegion(uint32_t address);
    void GoToAddress(const QString &text);

    static MemViewDialog* currentDlg;
    static MemViewDialog* openDlg(QWidget* parent)
   
   {
        if (currentDlg)
       
       {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new MemViewDialog(parent);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
   
   {
        currentDlg = nullptr;
    }

    QGraphicsItem* GetItem(int addrIndex, int index)
    {
        if (addrIndex < 16 && index < 16)
        { 
            return this->RAMTextItems[addrIndex][index];
        }

        return nullptr;
    }

    QGraphicsItem* GetAddressItem(int index)
    { 
        if (index < 16)
        { 
            return this->LeftAddrItems[index];
        }

        return nullptr;
    }

    int GetItemIndex(QGraphicsItem* item)
    {
        if (item != nullptr)
        {
            for (int i = 0; i < 16; i++)
            {
                for (int j = 0; j < 16; j++)
                {
                    if (this->RAMTextItems[i][j] == item)
                    {
                        return (i << 8) | j;
                    }
                }
            }
        }

        return -1;
    }

    QGraphicsItem* GetAsciiItem(int index)
    { 
        if (index < 16)
        { 
            return this->AsciiStrings[index];
        }

        return nullptr;
    }

    QGraphicsTextItem* GetAddrLabel()
    {
        return this->AddrLabel;
    }

    QLineEdit* GetValueAddrLineEdit()
    {
        return this->SetValAddr;
    }

    QCheckBox* GetFocusCheckbox()
    {
        return this->SetValFocus;
    }

    void StripStringHex(QString* str)
    {
        if (str->startsWith("0x"))
        {
            str->remove(0, 2);
        }
    }

    void UpdateViewRegion(uint32_t newStart, uint32_t newEnd) {
        this->ARM9AddrStart = newStart;
        this->ARM9AddrEnd = newEnd;
        this->ScrollBar->setMinimum(newStart);
        this->ScrollBar->setMaximum(newEnd - 0x100);
        this->ScrollBar->setValue(newStart);
    }

private:
    void UpdateText(int addrIndex, int index);
    void UpdateAddress(int index);
    void UpdateDecoded(int index);
    void UpdateScene();

private slots:
    void done(int r);
    void onValueBtnSetPressed();
    void onApplyEditToRAM(uint8_t value, QGraphicsItem *focus);
    void onSwitchFocus(FocusDirection eDirection, FocusAction eAction);
    void onScrollBarValueChanged(int value);
    void onMemRegionIndexChanged(int index);
    void onGoBtnPressed();
    void onUpdateSceneSignal();

public:
    uint32_t ARM9AddrStart;
    uint32_t ARM9AddrEnd;
    bool ForceTextUpdate;
    bool Highlight;

private:
    QGraphicsView* GfxView;
    CustomGraphicsScene* GfxScene;
    MemViewThread* UpdateThread;
    QScrollBar* ScrollBar;
    QPushButton* GoBtn;
    QGraphicsTextItem* AddrLabel;
    QLabel* UpdateRateLabel;
    CustomLineEdit* SearchLineEdit;
    QSpinBox* UpdateRate;
    QComboBox* MemRegionBox;

    // value setter group
    QGroupBox* SetValGroup;
    QComboBox* SetValBits;
    QPushButton* SetValBtn;
    QLineEdit* SetValNumber;
    QLineEdit* SetValAddr;
    QCheckBox* SetValFocus;
    QCheckBox* SetValIsHex;

    // yes I could just use `items()` but good ol' arrays are easier to work with
    CustomTextItem* RAMTextItems[16][16];
    QGraphicsTextItem* LeftAddrItems[16];
    QGraphicsTextItem* AsciiStrings[16];
    QString DecodedStrings[16];

    friend class MemViewThread;
};

// --- update thread ---

class MemViewThread : public QThread
{
    Q_OBJECT

public:
    explicit MemViewThread(MemViewDialog* parent) : Dialog(parent), Running(false), IsPaused(false)
    {}
    ~MemViewThread() override;

    void Start();
    void Stop();
    
    void Pause()
    {
        this->IsPaused = true;
    }

    void Unpause()
    {
        this->IsPaused = false;
    }

    MemViewDialog* Dialog;
    bool Running;
    bool IsPaused;

private:
    virtual void run() override;

signals:
    void updateSceneSignal();
};

#endif // MEMVIEWDIALOG_H
