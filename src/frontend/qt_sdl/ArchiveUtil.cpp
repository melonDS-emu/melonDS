/*
    Copyright 2016-2020 Arisotura, WaluigiWare64

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

#ifdef _WIN32
    #include <direct.h>
    #define mkdir(dir, mode) _mkdir(dir)
#endif

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

QVector<QString> ExtractFileFromArchive(const char* path, const char* wantedFile)
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
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (wantedFile == nullptr)
        {
            break;
        }
        if (strcmp(wantedFile, archive_entry_pathname(entry)) == 0)
        {
            break;
        }
    }
    size_t bytesToWrite = archive_entry_size(entry);
    auto archiveBuffer = std::make_unique<u8[]>(bytesToWrite);
    ssize_t bytesRead = archive_read_data(a, archiveBuffer.get(), bytesToWrite);
    if (bytesRead < 0)
    {
        printf(archive_error_string(a));
        archiveBuffer.reset(nullptr);
        return QVector<QString> {"Err", archive_error_string(a)};
    }
    QString nameToWrite = QFileInfo(path).absolutePath() + "/" + QFileInfo(path).baseName() + "/" + archive_entry_pathname(entry);

    mkdir(QFileInfo(path).baseName().toUtf8().constData(), 600); // Create directory otherwise fopen will not open the file
    FILE* fileToWrite = fopen(nameToWrite.toUtf8().constData(), "wb");
    fwrite((char*)archiveBuffer.get(), bytesToWrite, 1, fileToWrite);
    fclose(fileToWrite);

    archiveBuffer.reset(nullptr);
    archive_read_close(a);
    archive_read_free(a);
    return QVector<QString> {nameToWrite};

}


}
