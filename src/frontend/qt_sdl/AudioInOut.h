/*
    Copyright 2016-2023 melonDS team

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

#ifndef AUDIO_INOUT_H
#define AUDIO_INOUT_H

#include "types.h"

#include <QMainWindow>

class EmuThread;
namespace melonDS
{
class NDS;
}
namespace AudioInOut
{

void Init(EmuThread* thread);
void DeInit();

void MicProcess(melonDS::NDS& nds);
void AudioMute(QMainWindow* mainWindow);

void AudioSync(melonDS::NDS& nds);

void UpdateSettings(melonDS::NDS& nds);

void Enable();
void Disable();

}

#endif
