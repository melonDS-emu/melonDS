/*
    Copyright 2016-2025 melonDS team

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

    if (!Load())
        Error = true;
}

std::vector<ARCode> ARCodeFile::GetCodes() const noexcept
{
    if (Error)
        return {};

    std::vector<ARCode> codes;

    for (const ARCodeCat& cat : Categories)
    {
        for (const ARCode& code : cat.Codes)
        {
            codes.push_back(code);
        }
    }

    return codes;
}

bool ARCodeFile::Load()
{
    FileHandle* f = OpenFile(Filename, FileMode::ReadText);
    if (!f) return true;

    Categories.clear();

    // every code that isn't part of a category will be added to a null 'root' category
    ARCodeCat nullcat = {.IsRoot = true, .OnlyOneCodeEnabled = false};

    bool isincat = false;
    ARCodeCat curcat;

    bool isincode = false;
    ARCode curcode;

    int lastentry = 0;

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

        if (!strncasecmp(start, "ROOT", 4))
        {
            if (isincode) curcat.Codes.push_back(curcode);
            isincode = false;

            if (isincat) Categories.push_back(curcat);
            isincat = true;

            curcat = nullcat;
            lastentry = 0;
        }
        else if (!strncasecmp(start, "CAT", 3))
        {
            char catname[128];
            int ret, retchk;
            int onlyone;
            if (start[3] == ' ' && (start[4] == '0' || start[4] == '1') && start[5] == ' ')
            {
                retchk = 2;
                ret = sscanf(start, "CAT %d %127[^\r\n]", &onlyone, catname);
            }
            else
            {
                // backwards compatibility
                onlyone = 0;
                retchk = 1;
                ret = sscanf(start, "CAT %127[^\r\n]", catname);
            }
            catname[127] = '\0';

            if (ret < retchk)
            {
                Log(LogLevel::Error, "AR: malformed CAT line: %s\n", start);
                CloseFile(f);
                return false;
            }

            if (isincode) curcat.Codes.push_back(curcode);
            isincode = false;

            if (isincat) Categories.push_back(curcat);
            isincat = true;

            curcat.IsRoot = false;
            curcat.Name = catname;
            curcat.Description = "";
            curcat.OnlyOneCodeEnabled = onlyone!=0;
            curcat.Codes.clear();

            lastentry = 1;
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
            curcode.Description = "";
            curcode.Enabled = enable!=0;
            curcode.Code.clear();

            lastentry = 2;
        }
        else if (!strncasecmp(start, "DESC", 4))
        {
            char desc[256];
            int ret = sscanf(start, "DESC %255[^\r\n]", desc);
            desc[255] = '\0';

            if (ret < 1)
                continue;

            if (lastentry == 2)
                curcode.Description = desc;
            else if (lastentry == 1)
                curcat.Description = desc;
            else
            {
                Log(LogLevel::Error, "AR: encountered DESC line not part of anything\n");
                CloseFile(f);
                return false;
            }
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

    for (auto& cat : Categories)
    {
        for (auto& code : cat.Codes)
        {
            code.Parent = &cat;
        }

        // for categories that only allow one code to be enabled:
        // make sure we don't have multiple ones enabled
        if (cat.OnlyOneCodeEnabled)
        {
            bool foundone = false;
            for (auto& code : cat.Codes)
            {
                if (!code.Enabled) continue;
                if (foundone)
                    code.Enabled = false;
                else
                    foundone = true;
            }
        }
    }

    CloseFile(f);
    return true;
}

bool ARCodeFile::Save()
{
    FileHandle* f = Platform::OpenFile(Filename, FileMode::WriteText);
    if (!f) return false;

    for (auto it = Categories.begin(); it != Categories.end(); it++)
    {
        ARCodeCat& cat = *it;

        if (it != Categories.begin()) FileWriteFormatted(f, "\n");

        if (cat.IsRoot)
            FileWriteFormatted(f, "ROOT\n\n");
        else
        {
            FileWriteFormatted(f, "CAT %d %s\n", cat.OnlyOneCodeEnabled, cat.Name.c_str());
            if (!cat.Description.empty())
                FileWriteFormatted(f, "DESC %s\n", cat.Description.c_str());
            FileWriteFormatted(f, "\n");
        }

        for (auto jt = cat.Codes.begin(); jt != cat.Codes.end(); jt++)
        {
            ARCode& code = *jt;
            FileWriteFormatted(f, "CODE %d %s\n", code.Enabled, code.Name.c_str());
            if (!code.Description.empty())
                FileWriteFormatted(f, "DESC %s\n", code.Description.c_str());

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