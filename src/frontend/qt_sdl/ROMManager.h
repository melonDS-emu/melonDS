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

#ifndef ROMMANAGER_H
#define ROMMANAGER_H

#include "types.h"
#include "SaveManager.h"
#include "AREngine.h"
#include "DSi_NAND.h"
#include <QMainWindow>

#include "MemConstants.h"

#include <Args.h>
#include <optional>
#include <string>
#include <memory>
#include <vector>

namespace melonDS
{
class NDS;
class DSi;
class FATStorage;
class FATStorageArgs;
}
class EmuThread;
namespace ROMManager
{

using namespace melonDS;
extern std::unique_ptr<SaveManager> NDSSave;
extern std::unique_ptr<SaveManager> GBASave;
extern std::unique_ptr<SaveManager> FirmwareSave;

QString VerifySetup();
void Reset(EmuThread* thread);

/// Boots the emulated console into its system menu without starting a game.
bool BootToMenu(EmuThread* thread);
void ClearBackupState();

/// Returns the configured ARM9 BIOS loaded from disk,
/// the FreeBIOS if external BIOS is disabled and we're in NDS mode,
/// or nullptr if loading failed.
std::unique_ptr<ARM9BIOSImage> LoadARM9BIOS() noexcept;
std::unique_ptr<ARM7BIOSImage> LoadARM7BIOS() noexcept;
std::unique_ptr<DSiBIOSImage> LoadDSiARM9BIOS() noexcept;
std::unique_ptr<DSiBIOSImage> LoadDSiARM7BIOS() noexcept;
std::optional<FATStorageArgs> GetDSiSDCardArgs() noexcept;
std::optional<FATStorage> LoadDSiSDCard() noexcept;
std::optional<FATStorageArgs> GetDLDISDCardArgs() noexcept;
std::optional<FATStorage> LoadDLDISDCard() noexcept;
void CustomizeFirmware(Firmware& firmware, bool overridesettings) noexcept;
Firmware GenerateFirmware(int type) noexcept;
/// Loads and customizes a firmware image based on the values in Config
std::optional<Firmware> LoadFirmware(int type) noexcept;
/// Loads and customizes a NAND image based on the values in Config
std::optional<DSi_NAND::NANDImage> LoadNAND(const std::array<u8, DSiBIOSSize>& arm7ibios) noexcept;

/// Inserts a ROM into the emulated console.
bool LoadROM(QMainWindow* mainWindow, EmuThread*, QStringList filepath, bool reset);
void EjectCart(NDS& nds);
bool CartInserted();
QString CartLabel();

bool LoadGBAROM(QMainWindow* mainWindow, NDS& nds, QStringList filepath);
void LoadGBAAddon(NDS& nds, int type);
void EjectGBACart(NDS& nds);
bool GBACartInserted();
QString GBACartLabel();

std::string GetSavestateName(int slot);
bool SavestateExists(int slot);
bool LoadState(NDS& nds, const std::string& filename);
bool SaveState(NDS& nds, const std::string& filename);
void UndoStateLoad(NDS& nds);

void EnableCheats(NDS& nds, bool enable);
ARCodeFile* GetCheatFile();

void ROMIcon(const u8 (&data)[512], const u16 (&palette)[16], u32 (&iconRef)[32*32]);
void AnimatedROMIcon(const u8 (&data)[8][512], const u16 (&palette)[8][16],
                     const u16 (&sequence)[64], u32 (&animatedIconRef)[64][32*32],
                     std::vector<int> &animatedSequenceRef);
}

#endif // ROMMANAGER_H
