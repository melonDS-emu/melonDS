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
#include <windows.h>
#else
#include <sys/types.h>
#endif // __WIN32__
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>

#include "FATStorage.h"
#include "Platform.h"

namespace fs = std::filesystem;


static int GetDirEntryType(const char* path, struct dirent* entry)
{
#ifdef __WIN32__
    DWORD res = GetFileAttributesA(path);
    if (res == INVALID_FILE_ATTRIBUTES) return -1;
    if (res & FILE_ATTRIBUTE_DIRECTORY) return 1;
    if (res & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_VIRTUAL)) return -1;
    return 0;
#else
    if (entry->d_type == DT_DIR) return 1;
    if (entry->d_type == DT_REG) return 0;
    return -1;
#endif // __WIN32__
}


FATStorage::FATStorage()
{
    printf("FATStorage begin\n");
    bool res = Build("dldi", 0x20000000, "melonDLDI.bin");
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


void FATStorage::LoadIndex()
{
    Index.clear();

    FILE* f = Platform::OpenLocalFile(IndexPath.c_str(), "r");
    if (!f) return;

    char linebuf[1536];
    while (!feof(f))
    {
        if (fgets(linebuf, 1536, f) == nullptr)
            break;

        u64 fsize;
        s64 lastmodified;
        u32 lastmod_internal;
        char fpath[1536] = {0};
        int ret = sscanf(linebuf, "FILE %" PRIu64 " %" PRId64 " %u %[^\t\r\n]", &fsize, &lastmodified, &lastmod_internal, fpath);
        if (ret < 4) continue;

        for (int i = 0; i < 1536 && fpath[i] != '\0'; i++)
        {
            if (fpath[i] == '\\')
                fpath[i] = '/';
        }

        IndexEntry entry;
        entry.Path = fpath;
        entry.Size = fsize;
        entry.LastModified = lastmodified;
        entry.LastModifiedInternal = lastmod_internal;

        Index[entry.Path] = entry;
    }

    fclose(f);
}

void FATStorage::SaveIndex()
{
    FILE* f = Platform::OpenLocalFile(IndexPath.c_str(), "w");
    if (!f) return;

    for (const auto& [key, val] : Index)
    {
        fprintf(f, "FILE %" PRIu64 " %" PRId64 " %u %s\r\n", val.Size, val.LastModified, val.LastModifiedInternal, val.Path.c_str());
    }

    fclose(f);
}


bool FATStorage::CanFitFile(u32 len)
{
    FATFS* fs;
    DWORD freeclusters;
    FRESULT res;

    res = f_getfree("0:", &freeclusters, &fs);
    if (res != FR_OK) return false;

    u32 clustersize = fs->csize * 0x200;
    printf("CHECK IF FILE CAN FIT: len=%d, clus=%d, num=%d, free=%d\n",
           len, clustersize, (len + clustersize - 1) / clustersize, freeclusters);
    len = (len + clustersize - 1) / clustersize;
    return (freeclusters >= len);
}

int FATStorage::CleanupDirectory(std::string path, int level)
{
    if (level >= 32) return 44203;

    fDIR dir;
    FILINFO info;
    FRESULT res;
printf("CHECKING %s (level %d) FOR SHIT\n", path.c_str(), level);
    res = f_opendir(&dir, path.c_str());
    if (res != FR_OK) return 0;

    std::vector<std::string> deletelist;
    std::vector<std::string> subdirlist;
    int survivors = 0;

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK) break;
        if (!info.fname[0]) break;

        std::string fullpath = path + info.fname;
printf("- FOUND: %s\n", fullpath.c_str());
        if (info.fattrib & AM_DIR)
        {
            subdirlist.push_back(fullpath);
        }
        else
        {
            if (Index.count(fullpath) < 1)
                deletelist.push_back(fullpath);
            else
                survivors++;
        }
    }

    f_closedir(&dir);

    for (auto& entry : deletelist)
    {
        std::string fullpath = "0:/" + entry;
        printf("- PURGING file %s\n", fullpath.c_str());
        f_unlink(fullpath.c_str());
    }

    for (auto& entry : subdirlist)
    {
        int res = CleanupDirectory(entry+"/", level+1);
        if (res < 1)
        {
            std::string fullpath = "0:/" + entry;
            printf("- PURGING subdir %s\n", fullpath.c_str());
            f_unlink(fullpath.c_str());
        }
        else
            survivors++;
    }

    return survivors;
}

bool FATStorage::ImportFile(std::string path, std::string in)
{
    FIL file;
    FILE* fin;
    FRESULT res;

    fin = fopen(in.c_str(), "rb");
    if (!fin)
        return false;

    fseek(fin, 0, SEEK_END);
    u32 len = (u32)ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (!CanFitFile(len))
    {
        fclose(fin);
        return false;
    }

    res = f_open(&file, path.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        fclose(fin);
        return false;
    }

    u8 buf[0x1000];
    for (u32 i = 0; i < len; i += 0x1000)
    {
        u32 blocklen;
        if ((i + 0x1000) > len)
            blocklen = len - i;
        else
            blocklen = 0x1000;

        u32 nwrite;
        fread(buf, blocklen, 1, fin);
        f_write(&file, buf, blocklen, &nwrite);
    }

    fclose(fin);
    f_close(&file);

    return true;
}

bool FATStorage::BuildSubdirectory(const char* sourcedir, const char* path, int level)
{
    if (level >= 32)
    {
        printf("FATStorage::BuildSubdirectory: too many subdirectory levels, skipping\n");
        return false;
    }

    if (level == 0)
    {
        // remove whatever isn't in the index, as well as empty directories
        CleanupDirectory("", 0);

        int srclen = strlen(sourcedir);

        // iterate through the host directory:
        // * directories will be added as needed
        // * files will be added if they aren't in the index, or if the size or last-modified-date don't match
        for (auto& entry : fs::recursive_directory_iterator(sourcedir))
        {
            std::string fullpath = entry.path().string();
            std::string innerpath = fullpath.substr(srclen);
            if (innerpath[0] == '/' || innerpath[0] == '\\')
                innerpath = innerpath.substr(1);

            int ilen = innerpath.length();
            for (int i = 0; i < ilen; i++)
            {
                if (innerpath[i] == '\\')
                    innerpath[i] = '/';
            }

            //std::chrono::duration<s64> bourf(lastmodified_raw);
            //printf("DORP: %016llX\n", bourf.count());

            if (entry.is_directory())
            {
                innerpath = "0:/" + innerpath;
                f_mkdir(innerpath.c_str());
            }
            else if (entry.is_regular_file())
            {
                u64 filesize = entry.file_size();

                auto lastmodified = entry.last_write_time();
                s64 lastmodified_raw = std::chrono::duration_cast<std::chrono::seconds>(lastmodified.time_since_epoch()).count();

                bool import = false;
                if (Index.count(innerpath) < 1)
                {
                    import = true;
                }
                else
                {
                    IndexEntry& chk = Index[innerpath];
                    if (chk.Size != filesize) import = true;
                    if (chk.LastModified != lastmodified_raw) import = true;
                }

                if (import)
                {
                    IndexEntry entry;
                    entry.Path = innerpath;
                    entry.Size = filesize;
                    entry.LastModified = lastmodified_raw;

                    innerpath = "0:/" + innerpath;
                    if (ImportFile(innerpath, fullpath))
                    {
                        FILINFO finfo;
                        f_stat(innerpath.c_str(), &finfo);

                        entry.LastModifiedInternal = (finfo.fdate << 16) | finfo.ftime;

                        Index[entry.Path] = entry;

                        printf("IMPORTING FILE %s (FROM %s)\n", innerpath.c_str(), fullpath.c_str());
                    }
                }
            }

        }

        SaveIndex();

        return false;
    }

    char fullpath[1024] = {0};
    snprintf(fullpath, 1023, "%s%s", sourcedir, path);

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

        snprintf(fullpath, 1023, "%s%s/%s", sourcedir, path, entry->d_name);

        int entrytype = GetDirEntryType(fullpath, entry);
        if (entrytype == -1) continue;

        if (entrytype == 1) // directory
        {
            snprintf(fullpath, 1023, "0:%s/%s", path, entry->d_name);
            FRESULT fres = f_mkdir(fullpath);
            if (fres == FR_OK)
            {
                if (!BuildSubdirectory(sourcedir, &fullpath[2], level+1))
                    res = false;
            }
            else
                res = false;
        }
        else // file
        {
            char importpath[1024] = {0};
            snprintf(importpath, 1023, "0:%s/%s", path, entry->d_name);
            printf("importing %s to %s\n", fullpath, importpath);
            if (!ImportFile(importpath, fullpath))
                res = false;
        }
    }

    closedir(dir);

    return res;
}

bool FATStorage::Build(const char* sourcedir, u64 size, const char* filename)
{
    FilePath = filename;
    FileSize = size;

    FF_File = Platform::OpenLocalFile(filename, "r+b");
    if (!FF_File)
    {
        FF_File = Platform::OpenLocalFile(filename, "w+b");
        if (!FF_File)
            return false;
    }

    IndexPath = FilePath + ".idx";
    LoadIndex();

    FF_FileSize = size;
    ff_disk_open(FF_ReadStorage, FF_WriteStorage, (LBA_t)(size>>9));

    FRESULT res;
    FATFS fs;

    res = f_mount(&fs, "0:", 1);
    if (res != FR_OK)
    {
        // TODO: determine proper FAT type!
        // for example: libfat tries to determine the FAT type from the number of clusters
        // which doesn't match the way fatfs handles autodetection
        MKFS_PARM fsopt;
        fsopt.fmt = FM_FAT;// | FM_FAT32;
        fsopt.au_size = 0;
        fsopt.align = 1;
        fsopt.n_fat = 1;
        fsopt.n_root = 512;

        BYTE workbuf[FF_MAX_SS];
        res = f_mkfs("0:", &fsopt, workbuf, sizeof(workbuf));

        if (res == FR_OK)
            res = f_mount(&fs, "0:", 1);
    }

    if (res == FR_OK)
        BuildSubdirectory(sourcedir, "", 0);

    f_unmount("0:");

    ff_disk_close();
    fclose(FF_File);
    FF_File = nullptr;

    return true;
}
