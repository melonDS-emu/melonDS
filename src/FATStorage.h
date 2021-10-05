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

#ifndef FATSTORAGE_H
#define FATSTORAGE_H

#include <stdio.h>
#include <string>
#include <map>
#include <filesystem>

#include "types.h"
#include "fatfs/ff.h"


class FATStorage
{
public:
    FATStorage();
    ~FATStorage();

private:
    std::string FilePath;
    std::string IndexPath;

    FILE* File;
    u64 FileSize;

    static FILE* FF_File;
    static u64 FF_FileSize;
    static UINT FF_ReadStorage(BYTE* buf, LBA_t sector, UINT num);
    static UINT FF_WriteStorage(BYTE* buf, LBA_t sector, UINT num);

    void LoadIndex();
    void SaveIndex();

    bool ExportFile(std::string path, std::string out, std::filesystem::file_time_type& modtime);
    void ExportDirectory(std::string path, std::string outbase, int level);
    bool DeleteHostDirectory(std::string path, std::string outbase, int level);
    void ExportChanges(std::string outbase);

    bool CanFitFile(u32 len);
    bool DeleteDirectory(std::string path, int level);
    void CleanupDirectory(std::string path, int level);
    bool ImportFile(std::string path, std::string in);
    bool BuildSubdirectory(const char* sourcedir, const char* path, int level);

    bool Build(const char* sourcedir, u64 size, const char* filename);
    bool Save(std::string sourcedir);

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
