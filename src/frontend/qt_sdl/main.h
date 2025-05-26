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

#ifndef MAIN_H
#define MAIN_H

#include "glad/glad.h"

#include <QApplication>
#include <QEvent>
#include <QElapsedTimer>

#include "EmuInstance.h"
#include "Window.h"
#include "EmuThread.h"
#include "ScreenLayout.h"
#include "MPInterface.h"

enum
{
    InstCmd_Pause,
    InstCmd_Unpause,

    InstCmd_UpdateRecentFiles,
    //InstCmd_UpdateVideoSettings,
};

class MelonApplication : public QApplication
{
    Q_OBJECT

public:
    MelonApplication(int &argc, char** argv);
    bool event(QEvent* event) override;
};

extern QString* systemThemeName;
extern QString emuDirectory;

extern QElapsedTimer sysTimer;

bool createEmuInstance();
void deleteEmuInstance(int id);
void deleteAllEmuInstances(int first = 0);
int numEmuInstances();

void broadcastInstanceCommand(int cmd, QVariant& param, int sourceinst);

void setMPInterface(melonDS::MPInterfaceType type);

#endif // MAIN_H
