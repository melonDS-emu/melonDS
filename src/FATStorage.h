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

#ifndef FATSTORAGE_H
#define FATSTORAGE_H

#include <stdio.h>
#include <string>
#include <map>
#include <filesystem>

#include "Platform.h"
#include "types.h"
#include "fatfs/ff.h"


class FATStorage
{
public:
    FATStorage(const std::string& filename, u64 size, bool readonly, const std::string& sourcedir);
    ~FATStorage();

    bool Open();
    void Close();

    bool InjectFile(const std::string& path, u8* data, u32 len);

    u32 ReadSectors(u32 start, u32 num, u8* data);
    u32 WriteSectors(u32 start, u32 num, u8* data);

private:
    std::string FilePath;
    std::string IndexPath;
    std::string SourceDir;
    bool ReadOnly;

    Platform::FileHandle* File;
    u64 FileSize;

    static Platform::FileHandle* FF_File;
    static u64 FF_FileSize;
    static UINT FF_ReadStorage(BYTE* buf, LBA_t sector, UINT num);
    static UINT FF_WriteStorage(const BYTE* buf, LBA_t sector, UINT num);

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
    u64 GetDirectorySize(std::filesystem::path sourcedir);

    bool Load(const std::string& filename, u64 size, const std::string& sourcedir);
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

#endif // FATSTORAGE_H
