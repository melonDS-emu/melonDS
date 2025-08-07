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

#include "NDS.h"
#include "DSi.h"
#include "Mic.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

// Microphone control hub
// -
// There are three different ways to access the microphone:
// * DS: through the TSC (reading the AUX input returns the current mic level)
// * DSi: through the mic interface at 0x04004600
// * DSi: through the DSP (via BTDMP 0)
//
// This module serves to centralize microphone control functions and keep the sound buffer.


Mic::Mic(melonDS::NDS& nds) : NDS(nds)
{
    //
}

Mic::~Mic()
{
    //
}

void Mic::Reset()
{
    //
}

void Mic::DoSavestate(melonDS::Savestate *file)
{
    //
}


}
