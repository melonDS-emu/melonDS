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
    
QVector<QString> ListArchive(const char* path);
QVector<QString> ExtractFileFromArchive(const char* path, const char* wantedFile);

}

#endif // ARCHIVEUTIL_H
