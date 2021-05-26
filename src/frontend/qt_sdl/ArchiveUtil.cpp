/*
    Copyright 2016-2021 Arisotura, WaluigiWare64

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

namespace Archive
{

QVector<QString> ListArchive(const char* path)
{
    struct archive *a;
    struct archive_entry *entry;
    int r;

    QVector<QString> fileList = {"OK"};
    
    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    r = archive_read_open_filename(a, path, 10240);
    if (r != ARCHIVE_OK)
    {
        return QVector<QString> {"Err"};
    }
    
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) 
    {
      fileList.push_back(archive_entry_pathname(entry));
      archive_read_data_skip(a);
    }
    archive_read_close(a);
    archive_read_free(a);  
    if (r != ARCHIVE_OK)
    {
        return QVector<QString> {"Err"};
    }
    
    return fileList;
}

QVector<QString> ExtractFileFromArchive(const char* path, const char* wantedFile, QByteArray *romBuffer)
{
    struct archive *a = archive_read_new();
    struct archive_entry *entry;
    int r;

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    
    r = archive_read_open_filename(a, path, 10240);
    if (r != ARCHIVE_OK)
    {
        return QVector<QString> {"Err"};
    }

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        if (strcmp(wantedFile, archive_entry_pathname(entry)) == 0)
        {
            break;
        }
    }

    size_t bytesToWrite = archive_entry_size(entry);
    romBuffer->fill(0, bytesToWrite);
    ssize_t bytesRead = archive_read_data(a, romBuffer->data(), bytesToWrite);

    if (bytesRead < 0)
    {
        printf("Error whilst reading archive: %s", archive_error_string(a));
        return QVector<QString> {"Err", archive_error_string(a)};
    }

    archive_read_close(a);
    archive_read_free(a);
    return QVector<QString> {wantedFile};

}

u32 ExtractFileFromArchive(const char* path, const char* wantedFile, u8 **romdata)
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
}

}
