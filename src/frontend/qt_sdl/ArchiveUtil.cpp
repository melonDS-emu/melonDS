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

#include "ArchiveUtil.h"
#include "Platform.h"

using namespace melonDS;
using Platform::Log;
using Platform::LogLevel;

namespace Archive
{

#ifdef __WIN32__
#define melon_archive_open(a, f, b) archive_read_open_filename_w(a, (const wchar_t*)f.utf16(), b)
#else
#define melon_archive_open(a, f, b) archive_read_open_filename(a, f.toUtf8().constData(), b)
#endif // __WIN32__

bool compareCI(const QString& s1, const QString& s2)
{
    return s1.toLower() < s2.toLower();
}

QVector<QString> ListArchive(QString path)
{
    struct archive *a;
    struct archive_entry *entry;
    int r;

    QVector<QString> fileList;

    a = archive_read_new();

    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    //r = archive_read_open_filename(a, path, 10240);
    r = melon_archive_open(a, path, 10240);
    if (r != ARCHIVE_OK)
    {
        return QVector<QString> {"Err"};
    }

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;

        fileList.push_back(archive_entry_pathname_utf8(entry));
        archive_read_data_skip(a);
    }

    archive_read_close(a);
    archive_read_free(a);

    if (r != ARCHIVE_OK)
    {
        return QVector<QString> {"Err"};
    }

    std::stable_sort(fileList.begin(), fileList.end(), compareCI);
    fileList.prepend("OK");

    return fileList;
}

QVector<QString> ExtractFileFromArchive(QString path, QString wantedFile, QByteArray *romBuffer)
{
    struct archive *a = archive_read_new();
    struct archive_entry *entry;
    int r;

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    //r = archive_read_open_filename(a, path, 10240);
    r = melon_archive_open(a, path, 10240);
    if (r != ARCHIVE_OK)
    {
        return QVector<QString> {"Err"};
    }

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        if (strcmp(wantedFile.toUtf8().constData(), archive_entry_pathname_utf8(entry)) == 0)
        {
            break;
        }
    }

    size_t bytesToWrite = archive_entry_size(entry);
    romBuffer->fill(0, bytesToWrite);
    ssize_t bytesRead = archive_read_data(a, romBuffer->data(), bytesToWrite);

    if (bytesRead < 0)
    {
        Log(LogLevel::Error, "Error whilst reading archive: %s", archive_error_string(a));
        return QVector<QString> {"Err", archive_error_string(a)};
    }

    archive_read_close(a);
    archive_read_free(a);
    return QVector<QString> {wantedFile};

}

s32 ExtractFileFromArchive(QString path, QString wantedFile, std::unique_ptr<u8[]>& filedata, u32* filesize)
{
    struct archive *a = archive_read_new();
    struct archive_entry *entry;
    int r;


    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    //r = archive_read_open_filename(a, path, 10240);
    r = melon_archive_open(a, path, 10240);
    if (r != ARCHIVE_OK)
    {
        return -1;
    }

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        if (strcmp(wantedFile.toUtf8().constData(), archive_entry_pathname_utf8(entry)) == 0)
        {
            break;
        }
    }

    size_t bytesToRead = archive_entry_size(entry);
    if (filesize) *filesize = bytesToRead;
    filedata = std::make_unique<u8[]>(bytesToRead);
    ssize_t bytesRead = archive_read_data(a, filedata.get(), bytesToRead);

    archive_read_close(a);
    archive_read_free(a);

    return (u32)bytesRead;

}

/*u32 ExtractFileFromArchive(const char* path, const char* wantedFile, u8 **romdata)
{
    QByteArray romBuffer;
    QVector<QString> extractResult = ExtractFileFromArchive(path, wantedFile, &romBuffer);

    if(extractResult[0] == "Err")
    {
        return 0;
    }

    u32 len = romBuffer.size();
    *romdata = new u8[romBuffer.size()];
    memcpy(*romdata, romBuffer.data(), len);

    return len;
}*/

}
