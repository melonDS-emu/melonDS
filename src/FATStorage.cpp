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

#include <string.h>
#include <dirent.h>
#include <inttypes.h>
#include <vector>

#include "FATIO.h"
#include "FATStorage.h"
#include "Platform.h"

namespace fs = std::filesystem;
using namespace Platform;

FATStorage::FATStorage(const std::string& filename, u64 size, bool readonly, const std::string& sourcedir)
{
    ReadOnly = readonly;
    Load(filename, size, sourcedir);

    File = nullptr;
}

FATStorage::~FATStorage()
{
    if (!ReadOnly) Save();
}


bool FATStorage::Open()
{
    File = Platform::OpenLocalFile(FilePath, FileMode::ReadWriteExisting);
    if (!File)
    {
        return false;
    }

    return true;
}

void FATStorage::Close()
{
    if (File) CloseFile(File);
    File = nullptr;
}


bool FATStorage::InjectFile(const std::string& path, u8* data, u32 len)
{
    if (!File) return false;
    if (FF_File) return false;

    FF_File = File;
    FF_FileSize = FileSize;
    ff_disk_open(FF_ReadStorage, FF_WriteStorage, (LBA_t)(FileSize>>9));

    FRESULT res;
    FATFS fs;

    res = f_mount(&fs, "0:", 1);
    if (res != FR_OK)
    {
        ff_disk_close();
        FF_File = nullptr;
        return false;
    }

    std::string prefixedPath("0:/");
    prefixedPath += path;
    FF_FIL file;
    res = f_open(&file, prefixedPath.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        f_unmount("0:");
        ff_disk_close();
        FF_File = nullptr;
        return false;
    }

    u32 nwrite;
    f_write(&file, data, len, &nwrite);
    f_close(&file);

    f_unmount("0:");
    ff_disk_close();
    FF_File = nullptr;
    return nwrite==len;
}


u32 FATStorage::ReadSectors(u32 start, u32 num, u8* data)
{
    return ReadSectorsInternal(File, FileSize, start, num, data);
}

u32 FATStorage::WriteSectors(u32 start, u32 num, u8* data)
{
    if (ReadOnly) return 0;
    return WriteSectorsInternal(File, FileSize, start, num, data);
}


FileHandle* FATStorage::FF_File;
u64 FATStorage::FF_FileSize;

UINT FATStorage::FF_ReadStorage(BYTE* buf, LBA_t sector, UINT num)
{
    return ReadSectorsInternal(FF_File, FF_FileSize, sector, num, buf);
}

UINT FATStorage::FF_WriteStorage(const BYTE* buf, LBA_t sector, UINT num)
{
    return WriteSectorsInternal(FF_File, FF_FileSize, sector, num, buf);
}


u32 FATStorage::ReadSectorsInternal(FileHandle* file, u64 filelen, u32 start, u32 num, u8* data)
{
    if (!file) return 0;

    u64 addr = start * 0x200ULL;
    u32 len = num * 0x200;

    if ((addr+len) > filelen)
    {
        if (addr >= filelen) return 0;
        len = filelen - addr;
        num = len >> 9;
    }

    FileSeek(file, addr, FileSeekOrigin::Start);

    u32 res = FileRead(data, 0x200, num, file);
    if (res < num)
    {
        if (IsEndOfFile(file))
        {
            memset(&data[0x200*res], 0, 0x200*(num-res));
            return num;
        }
    }

    return res;
}

u32 FATStorage::WriteSectorsInternal(FileHandle* file, u64 filelen, u32 start, u32 num, const u8* data)
{
    if (!file) return 0;

    u64 addr = start * 0x200ULL;
    u32 len = num * 0x200;

    if ((addr+len) > filelen)
    {
        if (addr >= filelen) return 0;
        len = filelen - addr;
        num = len >> 9;
    }

    FileSeek(file, addr, FileSeekOrigin::Start);

    u32 res = Platform::FileWrite(data, 0x200, num, file);
    return res;
}


void FATStorage::LoadIndex()
{
    DirIndex.clear();
    FileIndex.clear();

    FileHandle* f = OpenLocalFile(IndexPath, FileMode::ReadText);
    if (!f) return;

    char linebuf[1536];
    while (!IsEndOfFile(f))
    {
        if (!FileReadLine(linebuf, 1536, f))
            break;

        if (linebuf[0] == 'S')
        {
            u64 fsize;
            int ret = sscanf(linebuf, "SIZE %" PRIu64, &fsize);
            if (ret < 1) continue;

            FileSize = fsize;
        }
        else if (linebuf[0] == 'D')
        {
            u32 readonly;
            char fpath[1536] = {0};
            int ret = sscanf(linebuf, "DIR %u %[^\t\r\n]",
                             &readonly, fpath);
            if (ret < 2) continue;

            for (int i = 0; i < 1536 && fpath[i] != '\0'; i++)
            {
                if (fpath[i] == '\\')
                    fpath[i] = '/';
            }

            DirIndexEntry entry;
            entry.Path = fpath;
            entry.IsReadOnly = readonly!=0;

            DirIndex[entry.Path] = entry;
        }
        else if (linebuf[0] == 'F')
        {
            u32 readonly;
            u64 fsize;
            s64 lastmodified;
            u32 lastmod_internal;
            char fpath[1536] = {0};
            int ret = sscanf(linebuf, "FILE %u %" PRIu64 " %" PRId64 " %u %[^\t\r\n]",
                             &readonly, &fsize, &lastmodified, &lastmod_internal, fpath);
            if (ret < 5) continue;

            for (int i = 0; i < 1536 && fpath[i] != '\0'; i++)
            {
                if (fpath[i] == '\\')
                    fpath[i] = '/';
            }

            FileIndexEntry entry;
            entry.Path = fpath;
            entry.IsReadOnly = readonly!=0;
            entry.Size = fsize;
            entry.LastModified = lastmodified;
            entry.LastModifiedInternal = lastmod_internal;

            FileIndex[entry.Path] = entry;
        }
    }

    CloseFile(f);

    // ensure the indexes are sane

    std::vector<std::string> removelist;

    for (const auto& [key, val] : DirIndex)
    {
        std::string path = val.Path;

        if ((path.find("/./") != std::string::npos) ||
            (path.find("/../") != std::string::npos) ||
            (path.substr(0,2) == "./") ||
            (path.substr(0,3) == "../"))
        {
            removelist.push_back(key);
            continue;
        }

        int sep = path.rfind('/');
        if (sep == std::string::npos) continue;

        path = path.substr(0, sep);
        if (DirIndex.count(path) < 1)
        {
            removelist.push_back(key);
        }
    }

    for (const auto& key : removelist)
    {
        DirIndex.erase(key);
    }

    removelist.clear();

    for (const auto& [key, val] : FileIndex)
    {
        std::string path = val.Path;

        if ((path.find("/./") != std::string::npos) ||
            (path.find("/../") != std::string::npos) ||
            (path.substr(0,2) == "./") ||
            (path.substr(0,3) == "../"))
        {
            removelist.push_back(key);
            continue;
        }

        int sep = path.rfind('/');
        if (sep == std::string::npos) continue;

        path = path.substr(0, sep);
        if (DirIndex.count(path) < 1)
        {
            removelist.push_back(key);
        }
    }

    for (const auto& key : removelist)
    {
        FileIndex.erase(key);
    }
}

void FATStorage::SaveIndex()
{
    FileHandle* f = OpenLocalFile(IndexPath, FileMode::WriteText);
    if (!f) return;

    FileWriteFormatted(f, "SIZE %" PRIu64 "\r\n", FileSize);

    for (const auto& [key, val] : DirIndex)
    {
        FileWriteFormatted(f, "DIR %u %s\r\n",
                val.IsReadOnly?1:0, val.Path.c_str());
    }

    for (const auto& [key, val] : FileIndex)
    {
        FileWriteFormatted(f, "FILE %u %" PRIu64 " %" PRId64 " %u %s\r\n",
                val.IsReadOnly?1:0, val.Size, val.LastModified, val.LastModifiedInternal, val.Path.c_str());
    }

    CloseFile(f);
}


bool FATStorage::ExportFile(const std::string& path, fs::path out)
{
    FF_FIL file;
    FileHandle* fout;
    FRESULT res;

    res = f_open(&file, path.c_str(), FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
        return false;

    u32 len = f_size(&file);

    if (fs::exists(out))
    {
        std::error_code err;
        fs::permissions(out,
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::add,
                        err);
    }

    fout = OpenFile(out.u8string(), FileMode::Write);
    if (!fout)
    {
        f_close(&file);
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

        u32 nread;
        f_read(&file, buf, blocklen, &nread);
        FileWrite(buf, blocklen, 1, fout);
    }

    CloseFile(fout);
    f_close(&file);

    return true;
}

void FATStorage::ExportDirectory(const std::string& path, const std::string& outbase, int level)
{
    if (level >= 32) return;

    FF_DIR dir;
    FF_FILINFO info;
    FRESULT res;

    std::string fullpath = "0:/" + path;
    res = f_opendir(&dir, fullpath.c_str());
    if (res != FR_OK) return;

    std::vector<std::string> subdirlist;

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK) break;
        if (!info.fname[0]) break;

        std::string fullpath = path + info.fname;
        fs::path outpath = fs::u8path(outbase + "/" + fullpath);

        if (info.fattrib & AM_DIR)
        {
            if (DirIndex.count(fullpath) < 1)
            {
                std::error_code err;
                fs::create_directory(outpath, err);

                DirIndexEntry entry;
                entry.Path = fullpath;
                entry.IsReadOnly = (info.fattrib & AM_RDO) != 0;

                DirIndex[entry.Path] = entry;
            }

            subdirlist.push_back(fullpath);
        }
        else
        {
            bool doexport = false;

            if (FileIndex.count(fullpath) < 1)
            {
                doexport = true;

                FileIndexEntry entry;
                entry.Path = fullpath;
                entry.IsReadOnly = (info.fattrib & AM_RDO) != 0;
                entry.Size = info.fsize;
                entry.LastModifiedInternal = (info.fdate << 16) | info.ftime;

                FileIndex[entry.Path] = entry;
            }
            else
            {
                u32 lastmod = (info.fdate << 16) | info.ftime;

                FileIndexEntry& entry = FileIndex[fullpath];
                if ((info.fsize != entry.Size) || (lastmod != entry.LastModifiedInternal))
                    doexport = true;

                entry.Size = info.fsize;
                entry.LastModifiedInternal = lastmod;
            }

            if (doexport)
            {
                if (ExportFile("0:/"+fullpath, outpath))
                {
                    fs::file_time_type modtime = fs::last_write_time(outpath);
                    s64 modtime_raw = std::chrono::duration_cast<std::chrono::seconds>(modtime.time_since_epoch()).count();

                    FileIndexEntry& entry = FileIndex[fullpath];
                    entry.LastModified = modtime_raw;
                }
                else
                {
                    // ??????
                }
            }
        }

        std::error_code err;
        fs::permissions(outpath,
                        fs::perms::owner_read | fs::perms::owner_write,
                        (info.fattrib & AM_RDO) ? fs::perm_options::remove : fs::perm_options::add,
                        err);
    }

    f_closedir(&dir);

    for (auto& entry : subdirlist)
    {
        ExportDirectory(entry+"/", outbase, level+1);
    }
}

bool FATStorage::DeleteHostDirectory(const std::string& path, const std::string& outbase, int level)
{
    if (level >= 32) return false;

    fs::path dirpath = fs::u8path(outbase + "/" + path);
    if (!fs::is_directory(dirpath))
        return true; // already deleted? oh well

    std::vector<std::string> filedeletelist;
    std::vector<std::string> dirdeletelist;

    int outlen = outbase.length();
    for (auto& entry : fs::directory_iterator(dirpath))
    {
        std::string fullpath = entry.path().string();
        std::string innerpath = fullpath.substr(outlen);
        if (innerpath[0] == '/' || innerpath[0] == '\\')
            innerpath = innerpath.substr(1);

        int ilen = innerpath.length();
        for (int i = 0; i < ilen; i++)
        {
            if (innerpath[i] == '\\')
                innerpath[i] = '/';
        }

        if (entry.is_directory())
        {
            dirdeletelist.push_back(innerpath);
        }
        else
        {
            filedeletelist.push_back(innerpath);
        }
    }

    for (const auto& key : filedeletelist)
    {
        fs::path fullpath = fs::u8path(outbase + "/" + key);
        std::error_code err;
        fs::permissions(fullpath,
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::add,
                        err);
        if (!fs::remove(fullpath))
            return false;

        FileIndex.erase(key);
    }

    for (const auto& key : dirdeletelist)
    {
        if (!DeleteHostDirectory(key, outbase, level+1))
            return false;
    }

    {
        fs::path fullpath = fs::u8path(outbase + "/" + path);

        std::error_code err;
        fs::permissions(fullpath,
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::add,
                        err);
        if (!fs::remove(fullpath))
            return false;

        DirIndex.erase(path);
    }

    return true;
}

void FATStorage::ExportChanges(const std::string& outbase)
{
    // reflect changes in the FAT volume to the host filesystem
    // * delete directories and files that exist in the index but not in the volume
    // * copy files to the host FS if they exist within the index and their size or
    //   internal last-modified time is different
    // * index and copy directories and files that exist in the volume but not in
    //   the index

    std::vector<std::string> deletelist;

    for (const auto& [key, val] : FileIndex)
    {
        std::string innerpath = "0:/" + val.Path;
        FF_FILINFO finfo;
        FRESULT res = f_stat(innerpath.c_str(), &finfo);
        if (res == FR_OK)
        {
            if (finfo.fattrib & AM_DIR)
            {
                deletelist.push_back(key);
            }
        }
        else if (res == FR_NO_FILE || res == FR_NO_PATH)
        {
            deletelist.push_back(key);
        }
    }

    for (const auto& key : deletelist)
    {
        fs::path fullpath = fs::u8path(outbase + "/" + key);

        std::error_code err;
        fs::permissions(fullpath,
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::add,
                        err);
        fs::remove(fullpath);

        FileIndex.erase(key);
    }

    deletelist.clear();

    for (const auto& [key, val] : DirIndex)
    {
        std::string innerpath = "0:/" + val.Path;
        FF_FILINFO finfo;
        FRESULT res = f_stat(innerpath.c_str(), &finfo);
        if (res == FR_OK)
        {
            if (!(finfo.fattrib & AM_DIR))
            {
                deletelist.push_back(key);
            }
        }
        else if (res == FR_NO_FILE || res == FR_NO_PATH)
        {
            deletelist.push_back(key);
        }
    }

    for (const auto& key : deletelist)
    {
        DeleteHostDirectory(key, outbase, 0);
    }

    ExportDirectory("", outbase, 0);
}


bool FATStorage::CanFitFile(u32 len)
{
    FATFS* fs;
    DWORD freeclusters;
    FRESULT res;

    res = f_getfree("0:", &freeclusters, &fs);
    if (res != FR_OK) return false;

    u32 clustersize = fs->csize * 0x200;

    len = (len + clustersize - 1) / clustersize;
    return (freeclusters >= len);
}

bool FATStorage::DeleteDirectory(const std::string& path, int level)
{
    if (level >= 32) return false;
    if (path.length() < 1) return false;

    FF_DIR dir;
    FF_FILINFO info;
    FRESULT res;

    std::string fullpath = "0:/" + path;
    f_chmod(fullpath.c_str(), 0, AM_RDO);
    res = f_opendir(&dir, fullpath.c_str());
    if (res != FR_OK) return false;

    std::vector<std::string> deletelist;
    std::vector<std::string> subdirlist;
    int survivors = 0;

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK) break;
        if (!info.fname[0]) break;

        std::string fullpath = path + info.fname;

        if (info.fattrib & AM_DIR)
        {
            subdirlist.push_back(fullpath);
        }
        else
        {
            deletelist.push_back(fullpath);
        }
    }

    f_closedir(&dir);

    for (auto& entry : deletelist)
    {
        std::string fullpath = "0:/" + entry;
        f_chmod(fullpath.c_str(), 0, AM_RDO);
        res = f_unlink(fullpath.c_str());
        if (res != FR_OK) return false;
    }

    for (auto& entry : subdirlist)
    {
        if (!DeleteDirectory(entry+"/", level+1))
            return false;
    }

    res = f_unlink(fullpath.c_str());
    if (res != FR_OK) return false;

    return true;
}

void FATStorage::CleanupDirectory(const std::string& sourcedir, const std::string& path, int level)
{
    if (level >= 32) return;

    FF_DIR dir;
    FF_FILINFO info;
    FRESULT res;

    std::string fullpath = "0:/" + path;
    res = f_opendir(&dir, fullpath.c_str());
    if (res != FR_OK) return;

    std::vector<std::string> filedeletelist;
    std::vector<std::string> dirdeletelist;
    std::vector<std::string> subdirlist;

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK) break;
        if (!info.fname[0]) break;

        std::string fullpath = path + info.fname;

        if (info.fattrib & AM_DIR)
        {
            if (DirIndex.count(fullpath) < 1)
                dirdeletelist.push_back(fullpath);
            else if (!fs::is_directory(fs::u8path(sourcedir+"/"+fullpath)))
            {
                DirIndex.erase(fullpath);
                dirdeletelist.push_back(fullpath);
            }
            else
                subdirlist.push_back(fullpath);
        }
        else
        {
            if (FileIndex.count(fullpath) < 1)
                filedeletelist.push_back(fullpath);
            else if (!fs::is_regular_file(fs::u8path(sourcedir+"/"+fullpath)))
            {
                FileIndex.erase(fullpath);
                filedeletelist.push_back(fullpath);
            }
        }
    }

    f_closedir(&dir);

    for (auto& entry : filedeletelist)
    {
        std::string fullpath = "0:/" + entry;
        f_chmod(fullpath.c_str(), 0, AM_RDO);
        f_unlink(fullpath.c_str());
    }

    for (auto& entry : dirdeletelist)
    {
        DeleteDirectory(entry+"/", level+1);
    }

    for (auto& entry : subdirlist)
    {
        CleanupDirectory(sourcedir, entry+"/", level+1);
    }
}

bool FATStorage::ImportFile(const std::string& path, fs::path in)
{
    FF_FIL file;
    FileHandle* fin;
    FRESULT res;

    fin = Platform::OpenFile(in.u8string(), FileMode::Read);
    if (!fin)
        return false;

    u32 len = FileLength(fin);

    if (!CanFitFile(len))
    {
        CloseFile(fin);
        return false;
    }

    res = f_open(&file, path.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        CloseFile(fin);
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
        FileRead(buf, blocklen, 1, fin);
        f_write(&file, buf, blocklen, &nwrite);
    }

    CloseFile(fin);
    f_close(&file);

    return true;
}

bool FATStorage::ImportDirectory(const std::string& sourcedir)
{
    // remove whatever isn't in the index
    CleanupDirectory(sourcedir, "", 0);

    int srclen = sourcedir.length();

    // iterate through the host directory:
    // * directories will be added if they aren't in the index
    // * files will be added if they aren't in the index, or if the size or last-modified-date don't match
    for (auto& entry : fs::recursive_directory_iterator(fs::u8path(sourcedir)))
    {
        std::string fullpath = entry.path().u8string();
        std::string innerpath = fullpath.substr(srclen);
        if (innerpath[0] == '/' || innerpath[0] == '\\')
            innerpath = innerpath.substr(1);

        int ilen = innerpath.length();
        for (int i = 0; i < ilen; i++)
        {
            if (innerpath[i] == '\\')
                innerpath[i] = '/';
        }

        bool readonly = (entry.status().permissions() & fs::perms::owner_write) == fs::perms::none;

        if (entry.is_directory())
        {
            if (DirIndex.count(innerpath) < 1)
            {
                DirIndexEntry ientry;
                ientry.Path = innerpath;
                ientry.IsReadOnly = readonly;

                innerpath = "0:/" + innerpath;
                FRESULT res = f_mkdir(innerpath.c_str());
                if (res == FR_OK)
                {
                    DirIndex[ientry.Path] = ientry;
                }
            }
        }
        else if (entry.is_regular_file())
        {
            u64 filesize = entry.file_size();

            auto lastmodified = entry.last_write_time();
            s64 lastmodified_raw = std::chrono::duration_cast<std::chrono::seconds>(lastmodified.time_since_epoch()).count();

            bool import = false;
            if (FileIndex.count(innerpath) < 1)
            {
                import = true;
            }
            else
            {
                FileIndexEntry& chk = FileIndex[innerpath];
                if (chk.Size != filesize) import = true;
                if (chk.LastModified != lastmodified_raw) import = true;
            }

            if (import)
            {
                FileIndexEntry ientry;
                ientry.Path = innerpath;
                ientry.IsReadOnly = readonly;
                ientry.Size = filesize;
                ientry.LastModified = lastmodified_raw;

                innerpath = "0:/" + innerpath;
                if (ImportFile(innerpath, entry.path()))
                {
                    FF_FILINFO finfo;
                    f_stat(innerpath.c_str(), &finfo);

                    ientry.LastModifiedInternal = (finfo.fdate << 16) | finfo.ftime;

                    FileIndex[ientry.Path] = ientry;
                }
            }
        }

        f_chmod(innerpath.c_str(), readonly?AM_RDO:0, AM_RDO);
    }

    SaveIndex();

    return true;
}

u64 FATStorage::GetDirectorySize(fs::path sourcedir)
{
    u64 ret = 0;
    u32 csize = 0x1000; // this is an estimate

    for (auto& entry : fs::recursive_directory_iterator(sourcedir))
    {
        if (entry.is_directory())
        {
            ret += csize;
        }
        else if (entry.is_regular_file())
        {
            u64 filesize = entry.file_size();

            filesize = (filesize + (csize-1)) & ~(csize-1);
            ret += filesize;
        }
    }

    return ret;
}

bool FATStorage::Load(const std::string& filename, u64 size, const std::string& sourcedir)
{
    FilePath = filename;
    FileSize = size;
    SourceDir = sourcedir;

    bool hasdir = !sourcedir.empty();
    if (hasdir)
    {
        if (!fs::is_directory(fs::u8path(sourcedir)))
        {
            hasdir = false;
            SourceDir = "";
        }
    }

    // 'auto' size management: (size=0)
    // * if an index exists: the size from the index is used
    // * if no index, and an image file exists: the file size is used
    // * if no image: if sourcing from a directory, size is calculated from that
    //   with a minimum 128MB extra, otherwise size is defaulted to 512MB

    bool isnew = !Platform::LocalFileExists(filename);
    FF_File = Platform::OpenLocalFile(filename, static_cast<FileMode>(FileMode::ReadWrite | FileMode::Preserve));
    if (!FF_File)
        return false;

    IndexPath = FilePath + ".idx";
    if (isnew)
    {
        DirIndex.clear();
        FileIndex.clear();
        SaveIndex();
    }
    else
    {
        LoadIndex();

        if (FileSize == 0)
        {
            FileSize = FileLength(FF_File);
        }
    }

    bool needformat = false;
    FATFS fs;
    FRESULT res;

    if (FileSize == 0)
    {
        needformat = true;
    }
    else
    {
        FF_FileSize = FileSize;
        ff_disk_open(FF_ReadStorage, FF_WriteStorage, (LBA_t)(FF_FileSize>>9));

        res = f_mount(&fs, "0:", 1);
        if (res != FR_OK)
        {
            needformat = true;
        }
        else if (size > 0 && size != FileSize)
        {
            needformat = true;
        }
    }

    if (needformat)
    {
        FileSize = size;
        if (FileSize == 0)
        {
            if (hasdir)
            {
                FileSize = GetDirectorySize(fs::u8path(sourcedir));
                FileSize += 0x8000000ULL; // 128MB leeway

                // make it a power of two
                FileSize |= (FileSize >> 1);
                FileSize |= (FileSize >> 2);
                FileSize |= (FileSize >> 4);
                FileSize |= (FileSize >> 8);
                FileSize |= (FileSize >> 16);
                FileSize |= (FileSize >> 32);
                FileSize++;
            }
            else
                FileSize = 0x20000000ULL; // 512MB
        }

        FF_FileSize = FileSize;
        ff_disk_close();
        ff_disk_open(FF_ReadStorage, FF_WriteStorage, (LBA_t)(FF_FileSize>>9));

        DirIndex.clear();
        FileIndex.clear();
        SaveIndex();

        FF_MKFS_PARM fsopt;

        // FAT type: we force it to FAT32 for any volume that is 1GB or more
        // libfat attempts to determine the FAT type from the volume size and other parameters
        // which can lead to it trying to interpret a FAT16 volume as FAT32
        if (FileSize >= 0x40000000ULL)
            fsopt.fmt = FM_FAT32;
        else
            fsopt.fmt = FM_FAT;

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
    {
        if (hasdir)
            ImportDirectory(sourcedir);
    }

    f_unmount("0:");

    ff_disk_close();
    CloseFile(FF_File);
    FF_File = nullptr;

    return true;
}

bool FATStorage::Save()
{
    if (SourceDir.empty())
    {
        return true;
    }

    FF_File = Platform::OpenLocalFile(FilePath, FileMode::ReadWriteExisting);
    if (!FF_File)
    {
        return false;
    }

    FF_FileSize = FileSize;
    ff_disk_open(FF_ReadStorage, FF_WriteStorage, (LBA_t)(FileSize>>9));

    FRESULT res;
    FATFS fs;

    res = f_mount(&fs, "0:", 1);
    if (res != FR_OK)
    {
        ff_disk_close();
        CloseFile(FF_File);
        FF_File = nullptr;
        return false;
    }

    ExportChanges(SourceDir);

    SaveIndex();

    f_unmount("0:");

    ff_disk_close();
    CloseFile(FF_File);
    FF_File = nullptr;

    return true;
}
