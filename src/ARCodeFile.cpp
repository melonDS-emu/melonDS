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

#include <stdio.h>
#include <string.h>
#include "ARCodeFile.h"
#include "Platform.h"


// TODO: import codes from other sources (usrcheat.dat, ...)
// TODO: more user-friendly error reporting


ARCodeFile::ARCodeFile(std::string filename)
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
    FILE* f = Platform::OpenFile(Filename, "r");
    if (!f) return true;

    Categories.clear();

    bool isincat = false;
    ARCodeCat curcat;

    bool isincode = false;
    ARCode curcode;

    char linebuf[1024];
    while (!feof(f))
    {
        if (fgets(linebuf, 1024, f) == nullptr)
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
                printf("AR: malformed CAT line: %s\n", start);
                fclose(f);
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
                printf("AR: malformed CODE line: %s\n", start);
                fclose(f);
                return false;
            }

            if (!isincat)
            {
                printf("AR: encountered CODE line with no category started\n");
                fclose(f);
                return false;
            }

            if (isincode) curcat.Codes.push_back(curcode);
            isincode = true;

            curcode.Name = codename;
            curcode.Enabled = enable!=0;
            curcode.CodeLen = 0;
        }
        else
        {
            u32 c0, c1;
            int ret = sscanf(start, "%08X %08X", &c0, &c1);

            if (ret < 2)
            {
                printf("AR: malformed data line: %s\n", start);
                fclose(f);
                return false;
            }

            if (!isincode)
            {
                printf("AR: encountered data line with no code started\n");
                fclose(f);
                return false;
            }

            if (curcode.CodeLen >= 2*64)
            {
                printf("AR: code too long!\n");
                fclose(f);
                return false;
            }

            u32 idx = curcode.CodeLen;
            curcode.Code[idx+0] = c0;
            curcode.Code[idx+1] = c1;
            curcode.CodeLen += 2;
        }
    }

    if (isincode) curcat.Codes.push_back(curcode);
    if (isincat) Categories.push_back(curcat);

    fclose(f);
    return true;
}

bool ARCodeFile::Save()
{
    FILE* f = Platform::OpenFile(Filename, "w");
    if (!f) return false;

    for (ARCodeCatList::iterator it = Categories.begin(); it != Categories.end(); it++)
    {
        ARCodeCat& cat = *it;

        if (it != Categories.begin()) fprintf(f, "\r\n");
        fprintf(f, "CAT %s\r\n\r\n", cat.Name.c_str());

        for (ARCodeList::iterator jt = cat.Codes.begin(); jt != cat.Codes.end(); jt++)
        {
            ARCode& code = *jt;
            fprintf(f, "CODE %d %s\r\n", code.Enabled, code.Name.c_str());

            for (u32 i = 0; i < code.CodeLen; i+=2)
            {
                fprintf(f, "%08X %08X\r\n", code.Code[i], code.Code[i+1]);
            }

            fprintf(f, "\r\n");
        }
    }

    fclose(f);
    return true;
}
