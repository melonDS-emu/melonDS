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

protected:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
};

class MemViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MemViewDialog(QWidget* parent);
    ~MemViewDialog();
    melonDS::NDS* GetNDS();

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
            return this->items[addrIndex][index];
        }

        return nullptr;
    }

    QGraphicsItem* GetAddressItem(int index) { 
        if (index < 16) { 
            return this->addresses[index];
        }

        return nullptr;
    }

    static const uint32_t arm9AddrStart = 0x02000000;
    static const uint32_t arm9AddrEnd = 0x03000000 - 0x100;

private slots:
    void done(int r);
    void updateText(int addrIndex, int index);
    void updateAddress(int index);
    void onAddressTextChanged(const QString &text);

private:
    QGraphicsView* gfxView;
    CustomGraphicsScene* gfxScene;
    MemViewThread* updateThread;
    QScrollBar* scrollBar;
    QLabel* addrLabel;
    QLabel* addrValueLabel;
    QLabel* updateRateLabel;
    QCheckBox* isBigEndian;
    QLineEdit* addrLineEdit;
    QSpinBox* updateRate;

    // yes I could just use `items()` but good ol' arraya are easier to work with
    QGraphicsItem* items[16][16];
    QGraphicsItem* addresses[16];

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
};

#endif // MEMVIEWDIALOG_H
