/*
    Copyright 2016-2020 Arisotura

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

#ifndef MAIN_H
#define MAIN_H

#include <QThread>
#include <QWidget>
#include <QMainWindow>


class EmuThread : public QThread
{
    Q_OBJECT
    void run() override;

public:
    explicit EmuThread(QObject* parent = nullptr);

    // to be called from the UI thread
    void emuRun();
    void emuPause(bool refresh);
    void emuUnpause();
    void emuStop();

private:
    volatile int EmuStatus;
    int PrevEmuStatus;
    int EmuRunning;
};


class MainWindowPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindowPanel(QWidget* parent);
    ~MainWindowPanel();

protected:
    void paintEvent(QPaintEvent* event) override;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();

private:
    MainWindowPanel* panel;
};

#endif // MAIN_H
