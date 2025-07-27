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

// --- main window ---

class CustomTextItem : public QGraphicsTextItem {
    Q_OBJECT

public:
    explicit CustomTextItem(const QString &text, QGraphicsItem *parent = nullptr);
    ~CustomTextItem() {}
    QRectF boundingRect() const override;

    void SetSize(QRectF newSize) {
        this->size = newSize;
    }

private:
    QRectF size;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
};

class CustomGraphicsScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit CustomGraphicsScene(QObject *parent = nullptr) : QGraphicsScene(parent) {}
    explicit CustomGraphicsScene(const QRectF &sceneRect, QObject *parent = nullptr) : QGraphicsScene(sceneRect, parent) {}
    explicit CustomGraphicsScene(qreal x, qreal y, qreal width, qreal height, QObject *parent = nullptr) : QGraphicsScene(x, y, width, height, parent) {}
    ~CustomGraphicsScene() {}

    void SetScrollBar(QScrollBar* pScrollBar) {
        this->scrollBar = pScrollBar;
    }

public slots:
    void onFocusItemChanged(QGraphicsItem *newFocus, QGraphicsItem *oldFocus, Qt::FocusReason reason);

private:
    QScrollBar* scrollBar;

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
            return this->ramTextItems[addrIndex][index];
        }

        return nullptr;
    }

    QGraphicsItem* GetAddressItem(int index) { 
        if (index < 16) { 
            return this->leftAddrItems[index];
        }

        return nullptr;
    }

    int GetItemIndex(QGraphicsItem* item) {
        if (item != nullptr) {
            for (int i = 0; i < 16; i++) {
                for (int j = 0; j < 16; j++) {
                    if (this->ramTextItems[i][j] == item) {
                        return j;
                    }
                }
            }
        }

        return -1;
    }

    QGraphicsItem* GetAsciiItem(int index) { 
        if (index < 16) { 
            return this->asciiStrings[index];
        }

        return nullptr;
    }

    QLabel* GetAddrLabel() {
        return this->addrLabel;
    }

    QLineEdit* GetValueAddrLineEdit() {
        return this->setValAddr;
    }

private slots:
    void done(int r);
    void updateText(int addrIndex, int index);
    void updateAddress(int index);
    void updateDecoded(int index);
    void onAddressTextChanged(const QString &text);
    void onValueBtnSetPressed();

public:
    uint32_t arm9AddrStart;
    uint32_t arm9AddrEnd;

private:
    QGraphicsView* gfxView;
    CustomGraphicsScene* gfxScene;
    MemViewThread* updateThread;
    QScrollBar* scrollBar;
    QLabel* addrDescLabel;
    QLabel* addrLabel;
    QLabel* updateRateLabel;
    QLineEdit* searchLineEdit;
    QSpinBox* updateRate;

    // value setter group
    QGroupBox* setValGroup;
    QComboBox* setValBits;
    QPushButton* setValBtn;
    QLineEdit* setValNumber;
    QLineEdit* setValAddr;
    QCheckBox* setValFocus;

    // yes I could just use `items()` but good ol' array are easier to work with
    CustomTextItem* ramTextItems[16][16];
    QGraphicsTextItem* leftAddrItems[16];
    QGraphicsTextItem* asciiStrings[16];
    QString decodedStrings[16];

    friend class MemViewThread;
};

// --- update thread ---

class MemViewThread : public QThread {
    Q_OBJECT

public:
    explicit MemViewThread(MemViewDialog* parent) : dialog(parent), running(false) {}
    ~MemViewThread() override;

    void Start();
    void Stop();

    MemViewDialog* dialog;
    bool running;

private:
    virtual void run() override;

signals:
    void updateTextSignal(int addrIndex, int index);
    void updateAddressSignal(int index);
    void updateDecodedSignal(int index);
};

#endif // MEMVIEWDIALOG_H
