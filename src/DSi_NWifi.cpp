/*
    Copyright 2016-2019 Arisotura

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

#include <string.h>
#include <stdio.h>
#include "DSi.h"
#include "DSi_NWifi.h"


DSi_NWifi::DSi_NWifi(DSi_SDHost* host) : DSi_SDDevice(host)
{
    //
}

DSi_NWifi::~DSi_NWifi()
{
    //
}


void DSi_NWifi::SendCMD(u8 cmd, u32 param)
{
    printf("NWIFI: unknown CMD %d %08X\n", cmd, param);
}

void DSi_NWifi::SendACMD(u8 cmd, u32 param)
{
    printf("NWIFI: unknown ACMD %d %08X\n", cmd, param);
}

void DSi_NWifi::ContinueTransfer()
{
    //
}
