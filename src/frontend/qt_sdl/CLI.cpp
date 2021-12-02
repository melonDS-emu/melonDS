/*
    Copyright 2016-2021 patataofcourse

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

#include "../../dolphin/CommonFuncs.h"

#include "CLI.h"

namespace CLI
{

//TODO: don't make it a function, add it into the ManageArgs function
bool IsFlag (int arg)
{
    switch (arg) {
        case cliArg_GBARomPath:
            return false;
        case cliArg_UseFreeBios:
            return true;
        case cliArg_FirmwareLanguage:
            return false;
        case cliArg_Renderer:
            return false;
    }
    return false;
}

void ManageArgs (int argc, char** argv)
{
    // easter egg - not worth checking other cases for something so dumb
    if (strcasecmp(argv[0], "derpDS") || strcasecmp(argv[0], "./derpDS"))
        printf("did you just call me a derp???\n");

    for (int i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        printf("%s\n", arg);
    }
}

}