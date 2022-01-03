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

#ifndef ROMMANAGER_H
#define ROMMANAGER_H

#include "types.h"
#include "SaveManager.h"

#include <string>
#include <vector>

namespace ROMManager
{

extern SaveManager* NDSSave;
extern SaveManager* GBASave;

QString VerifySetup();
void Reset();
bool LoadBIOS();

bool LoadROM(QStringList filepath, bool reset);
void EjectCart();
QString CartLabel();

bool LoadGBAROM(QStringList filepath, bool reset);
void LoadGBAAddon(int type);
void EjectGBACart();
QString GBACartLabel();

enum
{
    ROMSlot_NDS = 0,
    ROMSlot_GBA,

    ROMSlot_MAX
};

enum
{
    Load_OK = 0,

    Load_BIOS9Missing,
    Load_BIOS9Bad,

    Load_BIOS7Missing,
    Load_BIOS7Bad,

    Load_FirmwareMissing,
    Load_FirmwareBad,
    Load_FirmwareNotBootable,

    Load_DSiBIOS9Missing,
    Load_DSiBIOS9Bad,

    Load_DSiBIOS7Missing,
    Load_DSiBIOS7Bad,

    Load_DSiNANDMissing,
    Load_DSiNANDBad,

    // TODO: more precise errors for ROM loading
    Load_ROMLoadError,
};





}

#endif // ROMMANAGER_H
