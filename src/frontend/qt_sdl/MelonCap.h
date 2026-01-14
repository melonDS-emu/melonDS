/*
    Copyright 2016-2022 Arisotura

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

#ifndef MELONCAP_H
#define MELONCAP_H

#include "types.h"

#include <QWidget>
#include <QMainWindow>
#include <QMutex>

class EmuInstance;

class MelonCapWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MelonCapWindow(QWidget* parent = nullptr);
    ~MelonCapWindow();

    static MelonCapWindow* currentWin;
    static MelonCapWindow* openWin(QWidget* parent)
    {
        if (currentWin)
        {
            currentWin->activateWindow();
            return currentWin;
        }

        currentWin = new MelonCapWindow(parent);
        //currentWin->open();
        return currentWin;
    }
    static void closeWin()
    {
        currentWin = nullptr;
    }

    static void update()
    {
        if (currentWin)
            currentWin->drawScreen();
    }

signals:
    void doRepaint();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    EmuInstance* emuInstance;

    QMutex bufferLock;
    bool hasBuffers;
    void* topBuffer;
    void* bottomBuffer;

    QImage screen[2];

    melonDS::u32* capBuffer;
    melonDS::u32* compBuffer;

    QImage capImage;
    QImage compImage;

    void drawScreen();
    void updateCap();
};

#endif // MELONCAP_H
