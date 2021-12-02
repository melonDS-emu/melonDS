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

#include "../FrontendUtil.h"

#include "CLI.h"

namespace CLI
{

char* GetNextArg (int argc, char** argv, int argp)
{
    // returns argv[argp] if it exists, nullptr if it doesn't
    if (argp >= argc)
        return nullptr;
    else
        return argv[argp];
}

bool ManageArgs (int argc, char** argv)
{
    // returns true if a ROM was successfully loaded

    // easter egg - not worth checking other cases for something so dumb
    if (strcasecmp(argv[0], "derpDS") || strcasecmp(argv[0], "./derpDS"))
        //TODO: seems to always be doing this, research strcasecmp
        printf("did you just call me a derp???\n");

    int posArgCount = 0;
    bool romLoaded = false;
    
    for (int i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        bool isFlag;
        if (arg[0] == '-')
        {
            // manage options(?), so arguments like these: -h --help
        } 
        else
        {
            switch (posArgCount)
            {
                case 0:
                    {
                        int res = Frontend::LoadROM(arg, Frontend::ROMSlot_NDS);
                        if (res == Frontend::Load_OK)
                            romLoaded = true;
                        break;
                    }
                case 1:
                    //TODO: check if it's possible to load a GBA ROM without a DS ROM loaded
                    Frontend::LoadROM(arg, Frontend::ROMSlot_GBA);
                    break;
                default:
                    printf("Too many arguments, arg '%s' will be ignored\n", arg);
            }
            posArgCount++;
        }
    }

    return romLoaded;
}

}