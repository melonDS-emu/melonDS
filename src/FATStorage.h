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

#ifndef FATSTORAGE_H
#define FATSTORAGE_H

#include <stdio.h>
#include <string>
#include <map>
#include <optional>
#include <filesystem>

#include "Platform.h"
#include "types.h"
#include "fatfs/ff.h"
#include "FATIO.h"

namespace melonDS
{
/// Contains information necessary to load an SD card image.
/// The intended use case is for loading homebrew NDS ROMs;
/// you won't know that a ROM is homebrew until you parse it,
/// so if you load the SD card before the ROM
/// then you might end up discarding it.
struct FATStorageArgs
{
    std::string Filename;

    /// Size of the desired SD card in bytes, or 0 for auto-detect.
    u64 Size;
    bool ReadOnly;
    std::optional<std::string> SourceDir;
};

class FATStorage
{
public:
    FATStorage(const std::string& filename, u64 size, bool readonly, const std::optional<std::string>& sourcedir = std::nullopt);
    explicit FATStorage(const FATStorageArgs& args) noexcept;
    explicit FATStorage(FATStorageArgs&& args) noexcept;
    FATStorage(FATStorage&& other) noexcept;
    FATStorage(const FATStorage& other) = delete;
    FATStorage& operator=(const FATStorage& other) = delete;
    FATStorage& operator=(FATStorage&& other) noexcept;
    ~FATStorage();

    bool InjectFile(const std::string& path, u8* data, u32 len);
    u32 ReadFile(const std::string& path, u32 start, u32 len, u8* data);

    u32 ReadSectors(u32 start, u32 num, u8* data) const;
    u32 WriteSectors(u32 start, u32 num, const u8* data);

    [[nodiscard]] bool IsReadOnly() const noexcept { return ReadOnly; }
    u64 GetSectorCount() const;

private:
    std::string FilePath;
    std::string IndexPath;
    std::optional<std::string> SourceDir;
    bool ReadOnly;

    Platform::FileHandle* File;
    u64 FileSize;

    [[nodiscard]] ff_disk_read_cb FF_ReadStorage() const noexcept;
    [[nodiscard]] ff_disk_write_cb FF_WriteStorage() const noexcept;

    static u32 ReadSectorsInternal(Platform::FileHandle* file, u64 filelen, u32 start, u32 num, u8* data);
    static u32 WriteSectorsInternal(Platform::FileHandle* file, u64 filelen, u32 start, u32 num, const u8* data);

    void LoadIndex();
    void SaveIndex();

    bool ExportFile(const std::string& path, std::filesystem::path out);
    void ExportDirectory(const std::string& path, const std::string& outbase, int level);
    bool DeleteHostDirectory(const std::string& path, const std::string& outbase, int level);
    void ExportChanges(const std::string& outbase);

    bool CanFitFile(u32 len);
    bool DeleteDirectory(const std::string& path, int level);
    void CleanupDirectory(const std::string& sourcedir, const std::string& path, int level);
    bool ImportFile(const std::string& path, std::filesystem::path in);
    bool ImportDirectory(const std::string& sourcedir);
    u64 GetDirectorySize(std::filesystem::path sourcedir) const;

    bool Load(const std::string& filename, u64 size, const std::optional<std::string>& sourcedir);
    bool Save();

    typedef struct
    {
        std::string Path;
        bool IsReadOnly;

    } DirIndexEntry;

    typedef struct
    {
        std::string Path;
        bool IsReadOnly;
        u64 Size;
        s64 LastModified;
        u32 LastModifiedInternal;

    } FileIndexEntry;

    std::map<std::string, DirIndexEntry> DirIndex;
    std::map<std::string, FileIndexEntry> FileIndex;
};

}
#endif // FATSTORAGE_H
