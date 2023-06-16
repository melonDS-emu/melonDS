/*
    Copyright 2016-2022 melonDS team

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
#include "HLE.h"

#include "HLE_Retail/IPC.h"


namespace HLE
{

void Reset()
{
    Retail::Reset();
}


void StartScanline(u32 line)
{
    return Retail::StartScanline(line);
}


void OnIPCSync()
{
    // TODO: select retail or homebrew HLE

    return Retail::OnIPCSync();
}

void OnIPCRequest()
{
    // TODO: select retail or homebrew HLE

    return Retail::OnIPCRequest();
}

}
