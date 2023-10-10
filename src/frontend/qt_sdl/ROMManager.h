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

#ifndef ROMMANAGER_H
#define ROMMANAGER_H

#include "types.h"
#include "SaveManager.h"
#include "AREngine.h"
#include "DSi_NAND.h"

#include <string>
#include <memory>
#include <vector>

namespace ROMManager
{

extern SaveManager* NDSSave;
extern SaveManager* GBASave;
extern std::unique_ptr<SaveManager> FirmwareSave;

QString VerifySetup();
void Reset();
bool LoadBIOS();
void ClearBackupState();

bool InstallFirmware();
bool InstallNAND(const u8* es_keyY);
bool LoadROM(QStringList filepath, bool reset);
void EjectCart();
bool CartInserted();
QString CartLabel();

bool LoadGBAROM(QStringList filepath);
void LoadGBAAddon(int type);
void EjectGBACart();
bool GBACartInserted();
QString GBACartLabel();

std::string GetSavestateName(int slot);
bool SavestateExists(int slot);
bool LoadState(const std::string& filename);
bool SaveState(const std::string& filename);
void UndoStateLoad();

void EnableCheats(bool enable);
ARCodeFile* GetCheatFile();

void ROMIcon(const u8 (&data)[512], const u16 (&palette)[16], u32* iconRef);
void AnimatedROMIcon(const u8 (&data)[8][512], const u16 (&palette)[8][16],
                     const u16 (&sequence)[64], u32 (&animatedTexRef)[32 * 32 * 64],
                     std::vector<int> &animatedSequenceRef);

}

#endif // ROMMANAGER_H
