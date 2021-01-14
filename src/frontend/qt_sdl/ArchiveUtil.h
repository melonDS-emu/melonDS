#ifndef ARCHIVEUTIL_H
#define ARCHIVEUTIL_H

#include <stdio.h>

#include <string>
#include <memory>

#include <QList>
#include <QDir>

#include <archive.h>
#include <archive_entry.h>

#include "types.h"

namespace Archive
{
    
QList<QString> ListArchive(const char* path);
QList<QString> ExtractFileFromArchive(const char* path, const char* wantedFile);

}

#endif // ARCHIVEUTIL_H
