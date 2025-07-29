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

typedef enum FocusDirection {
    focusDirection_Up,
    focusDirection_Down,
    focusDirection_Left,
    focusDirection_Right,
} FocusDirection;

// --- main window ---

class CustomTextItem : public QGraphicsTextItem {
    Q_OBJECT

public:
    explicit CustomTextItem(const QString &text, QGraphicsItem *parent = nullptr);
    ~CustomTextItem() {}
    QRectF boundingRect() const override;
    bool IsKeyValid(int key);

    void SetSize(QRectF newSize) {
        this->Size = newSize;
    }

    void SetEditionFlags() {
        this->IsEditing = true;
        this->setTextInteractionFlags(Qt::TextInteractionFlag::TextEditorInteraction);
    }

    void SetSelectionFlags() {
        this->IsEditing = false;
        this->setTextInteractionFlags(Qt::TextInteractionFlag::TextSelectableByMouse);
    }

    void SetTextSelection(bool selected) {
        QTextCursor cursor = this->textCursor();

        if (selected) {
            cursor.select(QTextCursor::Document);
        } else {
            cursor.clearSelection();
        }

        this->setTextCursor(cursor);
    }

signals:
    void applyEditToRAM(uint8_t value, QGraphicsItem *focus);
    void switchFocus(FocusDirection eDirection);

private:
    QRectF Size;
    bool IsEditing;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
};

class CustomGraphicsScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit CustomGraphicsScene(QObject *parent = nullptr) : QGraphicsScene(parent) {}
    explicit CustomGraphicsScene(const QRectF &sceneRect, QObject *parent = nullptr) : QGraphicsScene(sceneRect, parent) {}
    explicit CustomGraphicsScene(qreal x, qreal y, qreal width, qreal height, QObject *parent = nullptr) : QGraphicsScene(x, y, width, height, parent) {}
    ~CustomGraphicsScene() {}

    void SetScrollBar(QScrollBar* pScrollBar) {
        this->ScrollBar = pScrollBar;
    }

public slots:
    void onFocusItemChanged(QGraphicsItem *newFocus, QGraphicsItem *oldFocus, Qt::FocusReason reason);

private:
    QScrollBar* ScrollBar;

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);
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

    QGraphicsItem* GetItem(int addrIndex, int index) { 
        if (addrIndex < 16 && index < 16) { 
            return this->RAMTextItems[addrIndex][index];
        }

        return nullptr;
    }

    QGraphicsItem* GetAddressItem(int index) { 
        if (index < 16) { 
            return this->LeftAddrItems[index];
        }

        return nullptr;
    }

    int GetItemIndex(QGraphicsItem* item) {
        if (item != nullptr) {
            for (int i = 0; i < 16; i++) {
                for (int j = 0; j < 16; j++) {
                    if (this->RAMTextItems[i][j] == item) {
                        return (i << 8) | j;
                    }
                }
            }
        }

        return -1;
    }

    QGraphicsItem* GetAsciiItem(int index) { 
        if (index < 16) { 
            return this->AsciiStrings[index];
        }

        return nullptr;
    }

    QLabel* GetAddrLabel() {
        return this->AddrLabel;
    }

    QLineEdit* GetValueAddrLineEdit() {
        return this->SetValAddr;
    }

    QCheckBox* GetFocusCheckbox() {
        return this->SetValFocus;
    }

    void StripStringHex(QString* str) {
        if (str->startsWith("0x")) {
            str->remove(0, 2);
        }
    }

private slots:
    void done(int r);
    void UpdateText(int addrIndex, int index);
    void UpdateAddress(int index);
    void UpdateDecoded(int index);
    void onAddressTextChanged(const QString &text);
    void onValueBtnSetPressed();
    void onApplyEditToRAM(uint8_t value, QGraphicsItem *focus);
    void onSwitchFocus(FocusDirection eDirection);

public:
    uint32_t ARM9AddrStart;
    uint32_t ARM9AddrEnd;

private:
    bool ForceTextUpdate;

    QGraphicsView* GfxView;
    CustomGraphicsScene* GfxScene;
    MemViewThread* UpdateThread;
    QScrollBar* ScrollBar;
    QLabel* AddrDescLabel;
    QLabel* AddrLabel;
    QLabel* UpdateRateLabel;
    QLineEdit* SearchLineEdit;
    QSpinBox* UpdateRate;

    // value setter group
    QGroupBox* SetValGroup;
    QComboBox* SetValBits;
    QPushButton* SetValBtn;
    QLineEdit* SetValNumber;
    QLineEdit* SetValAddr;
    QCheckBox* SetValFocus;

    // yes I could just use `items()` but good ol' array are easier to work with
    CustomTextItem* RAMTextItems[16][16];
    QGraphicsTextItem* LeftAddrItems[16];
    QGraphicsTextItem* AsciiStrings[16];
    QString DecodedStrings[16];

    friend class MemViewThread;
};

// --- update thread ---

class MemViewThread : public QThread {
    Q_OBJECT

public:
    explicit MemViewThread(MemViewDialog* parent) : Dialog(parent), Running(false) {}
    ~MemViewThread() override;

    void Start();
    void Stop();

    MemViewDialog* Dialog;
    bool Running;

private:
    virtual void run() override;

signals:
    void updateTextSignal(int addrIndex, int index);
    void updateAddressSignal(int index);
    void updateDecodedSignal(int index);
};

#endif // MEMVIEWDIALOG_H
