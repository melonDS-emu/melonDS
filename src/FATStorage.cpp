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

#ifdef __WIN32__
#include <shlwapi.h>
#else
#include <sys/types.h>
#endif // __WIN32__
#include <dirent.h>
#include <errno.h>

#include "FATStorage.h"
#include "Platform.h"


static int GetDirEntryType(struct dirent* entry)
{
#ifdef __WIN32__
    BOOL res = PathIsDirectoryA(entry->d_name);
    return res ? 1:0;
#else
    if (entry->d_type == DT_DIR) return 1;
    if (entry->d_type == DT_REG) return 0;
    return -1;
#endif // __WIN32__
}


FATStorage::FATStorage()
{
    printf("FATStorage begin\n");
    bool res = Build(".", 0x40000000, "melonDLDI.bin");
    printf("FATStorage result: %d\n", res);
}

FATStorage::~FATStorage()
{
    //
}


FILE* FATStorage::FF_File;
u64 FATStorage::FF_FileSize;

UINT FATStorage::FF_ReadStorage(BYTE* buf, LBA_t sector, UINT num)
{
    if (!FF_File) return 0;

    u64 addr = sector * 0x200ULL;
    u32 len = num * 0x200;

    if ((addr+len) > FF_FileSize)
    {
        if (addr >= FF_FileSize) return 0;
        len = FF_FileSize - addr;
        num = len >> 9;
    }

    fseek(FF_File, addr, SEEK_SET);

    u32 res = fread(buf, 0x200, num, FF_File);
    if (res < num)
    {
        if (feof(FF_File))
        {
            memset(&buf[0x200*res], 0, 0x200*(num-res));
            return num;
        }
    }

    return res;
}

UINT FATStorage::FF_WriteStorage(BYTE* buf, LBA_t sector, UINT num)
{
    if (!FF_File) return 0;

    u64 addr = sector * 0x200ULL;
    u32 len = num * 0x200;

    if ((addr+len) > FF_FileSize)
    {
        if (addr >= FF_FileSize) return 0;
        len = FF_FileSize - addr;
        num = len >> 9;
    }

    fseek(FF_File, addr, SEEK_SET);

    u32 res = fwrite(buf, 0x200, num, FF_File);
    return res;
}


bool FATStorage::BuildSubdirectory(const char* sourcedir, const char* path, int level)
{
    if (level >= 32)
    {
        printf("FATStorage::BuildSubdirectory: too many subdirectory levels, skipping\n");
        return false;
    }

    char fullpath[1024] = {0};
    snprintf(fullpath, 1023, "%s/%s", sourcedir, path);

    DIR* dir = opendir(fullpath);
    if (!dir) return false;

    bool res = true;
    for (;;)
    {
        errno = 0;
        struct dirent* entry = readdir(dir);
        if (!entry)
        {
            if (errno != 0) res = false;
            break;
        }

        if (entry->d_name[0] == '.')
        {
            if (entry->d_name[1] == '\0') continue;
            if (entry->d_name[1] == '.' && entry->d_name[2] == '\0') continue;
        }

        int entrytype = GetDirEntryType(entry);
        if (entrytype == -1) continue;

        if (entrytype == 1) // directory
        {
            snprintf(fullpath, 1023, "%s/%s", path, entry->d_name);

            printf("DIR: %s/%s\n", sourcedir, fullpath);

            if (!BuildSubdirectory(sourcedir, fullpath, level+1))
                res = false;
        }
        else // file
        {
            snprintf(fullpath, 1023, "%s/%s/%s", sourcedir, path, entry->d_name);

            printf("FILE: %s\n", fullpath);
        }
    }

    closedir(dir);

    return res;
}

bool FATStorage::Build(const char* sourcedir, u64 size, const char* filename)
{
    filesize = size;

    FF_File = Platform::OpenLocalFile(filename, "wb");
    if (!FF_File)
        return false;

    FF_FileSize = size;
    ff_disk_open(FF_ReadStorage, FF_WriteStorage, (LBA_t)(size>>9));

    FRESULT res;

    MKFS_PARM fsopt;
    fsopt.fmt = FM_FAT | FM_FAT32;
    fsopt.au_size = 0;
    fsopt.align = 1;
    fsopt.n_fat = 1;
    fsopt.n_root = 512;

    BYTE workbuf[FF_MAX_SS];
    res = f_mkfs("0:", &fsopt, workbuf, sizeof(workbuf));
    printf("MKFS RES %d\n", res);

    BuildSubdirectory(sourcedir, "", 0);

    ff_disk_close();
    fclose(FF_File);
    FF_File = nullptr;

    return true;
}
