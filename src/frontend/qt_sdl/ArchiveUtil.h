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

#ifndef ARCHIVEUTIL_H
#define ARCHIVEUTIL_H

#include <stdio.h>

#include <string>
#include <memory>

#include <QVector>
#include <QDir>

#include <archive.h>
#include <archive_entry.h>

#include "types.h"

namespace Archive
{

using namespace melonDS;
QVector<QString> ListArchive(QString path);
s32 ExtractFileFromArchive(QString path, QString wantedFile, std::unique_ptr<u8[]>& filedata, u32* filesize);
//QVector<QString> ExtractFileFromArchive(QString path, QString wantedFile, QByteArray *romBuffer);
//u32 ExtractFileFromArchive(const char* path, const char* wantedFile, u8 **romdata);

}

#endif // ARCHIVEUTIL_H
