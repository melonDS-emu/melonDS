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

#include <stdio.h>
#include <string.h>
#include "ARCodeFile.h"
#include "Platform.h"

namespace melonDS
{
using namespace Platform;

// TODO: import codes from other sources (usrcheat.dat, ...)
// TODO: more user-friendly error reporting


ARCodeFile::ARCodeFile(const std::string& filename)
{
    Filename = filename;

    Error = false;

    Categories.clear();

    if (!Load())
        Error = true;
}

ARCodeFile::~ARCodeFile()
{
    Categories.clear();
}

bool ARCodeFile::Load()
{
    FileHandle* f = OpenFile(Filename, FileMode::ReadText);
    if (!f) return true;

    Categories.clear();

    bool isincat = false;
    ARCodeCat curcat;

    bool isincode = false;
    ARCode curcode;

    char linebuf[1024];
    while (!IsEndOfFile(f))
    {
        if (!FileReadLine(linebuf, 1024, f))
            break;

        linebuf[1023] = '\0';

        char* start = linebuf;
        while (start[0]==' ' || start[0]=='\t')
            start++;

        if (start[0]=='#' || start[0]=='\r' || start[0]=='\n' || start[0]=='\0')
            continue;

        if (!strncasecmp(start, "CAT", 3))
        {
            char catname[128];
            int ret = sscanf(start, "CAT %127[^\r\n]", catname);
            catname[127] = '\0';

            if (ret < 1)
            {
                Log(LogLevel::Error, "AR: malformed CAT line: %s\n", start);
                CloseFile(f);
                return false;
            }

            if (isincode) curcat.Codes.push_back(curcode);
            isincode = false;

            if (isincat) Categories.push_back(curcat);
            isincat = true;

            curcat.Name = catname;
            curcat.Codes.clear();
        }
        else if (!strncasecmp(start, "CODE", 4))
        {
            int enable;
            char codename[128];
            int ret = sscanf(start, "CODE %d %127[^\r\n]", &enable, codename);
            codename[127] = '\0';

            if (ret < 2)
            {
                Log(LogLevel::Error, "AR: malformed CODE line: %s\n", start);
                CloseFile(f);
                return false;
            }

            if (!isincat)
            {
                Log(LogLevel::Error, "AR: encountered CODE line with no category started\n");
                CloseFile(f);
                return false;
            }

            if (isincode) curcat.Codes.push_back(curcode);
            isincode = true;

            curcode.Name = codename;
            curcode.Enabled = enable!=0;
            curcode.Code.clear();
        }
        else
        {
            u32 c0, c1;
            int ret = sscanf(start, "%08X %08X", &c0, &c1);

            if (ret < 2)
            {
                Log(LogLevel::Error, "AR: malformed data line: %s\n", start);
                CloseFile(f);
                return false;
            }

            if (!isincode)
            {
                Log(LogLevel::Error, "AR: encountered data line with no code started\n");
                CloseFile(f);
                return false;
            }

            curcode.Code.push_back(c0);
            curcode.Code.push_back(c1);
        }
    }

    if (isincode) curcat.Codes.push_back(curcode);
    if (isincat) Categories.push_back(curcat);

    CloseFile(f);
    return true;
}

bool ARCodeFile::Save()
{
    FileHandle* f = Platform::OpenFile(Filename, FileMode::WriteText);
    if (!f) return false;

    for (ARCodeCatList::iterator it = Categories.begin(); it != Categories.end(); it++)
    {
        ARCodeCat& cat = *it;

        if (it != Categories.begin()) FileWriteFormatted(f, "\n");
        FileWriteFormatted(f, "CAT %s\n\n", cat.Name.c_str());

        for (ARCodeList::iterator jt = cat.Codes.begin(); jt != cat.Codes.end(); jt++)
        {
            ARCode& code = *jt;
            FileWriteFormatted(f, "CODE %d %s\n", code.Enabled, code.Name.c_str());

            for (size_t i = 0; i < code.Code.size(); i+=2)
            {
                FileWriteFormatted(f, "%08X %08X\n", code.Code[i], code.Code[i + 1]);
            }

            FileWriteFormatted(f, "\n");
        }
    }

    CloseFile(f);
    return true;
}

}