/*
    Copyright 2016-2021 Arisotura

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

#ifndef SHAREDCONFIG_H
#define SHAREDCONFIG_H

namespace Config
{

extern int ConsoleType;
extern int DirectBoot;
extern int SavestateRelocSRAM;

extern int ExternalBIOSEnable;

extern char BIOS9Path[1024];
extern char BIOS7Path[1024];
extern char FirmwarePath[1024];

extern char DSiBIOS9Path[1024];
extern char DSiBIOS7Path[1024];
extern char DSiFirmwarePath[1024];
extern char DSiNANDPath[1024];

}

#endif
