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
#include <stdlib.h>
#include <string.h>

#include "../FrontendUtil.h"

#include "CLI.h"

namespace CLI
{

char* DSRomPath = new char[128];

char* GBARomPath = new char[128];

char* GetNextArg (int argc, char** argv, int argp)
{
    // returns argv[argp] if it exists, nullptr if it doesn't
    if (argp >= argc)
        return nullptr;
    else
        return argv[argp];
}

void ManageArgs (int argc, char** argv)
{
    // easter egg - not worth checking other cases for something so dumb
    if (!strcasecmp(argv[0], "derpDS") || !strcasecmp(argv[0], "./derpDS"))
        printf("did you just call me a derp???\n");

    int posArgCount = 0;
    
    for (int i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        bool isFlag;
        if (arg[0] == '-')
        {
            if (!strcasecmp(arg, "-h") || !strcasecmp(arg, "--help"))
            {
                //TODO: QT arguments
                printf(
                    "usage: melonDS [options] ... [dspath] [gbapath]\n"
                    "Options:\n"
                    "  -h / --help  display this help message and quit"
                    "Arguments:\n"
                    "  dspath       path to a DS ROM you want to run\n"
                    "  gbapath      path to a GBA ROM you want to load in the emulated Slot 2\n"
                );
                exit(0);
            }
            else
            {
                printf("Unrecognized command line option %s - aborting.\n", arg);
                exit(1);
            }
        } 
        else
        {
            switch (posArgCount)
            {
                case 0:
                    {
                        DSRomPath = arg;
                        break;
                    }
                case 1:
                    GBARomPath = arg;
                    break;
                default:
                    printf("Too many arguments, arg '%s' will be ignored\n", arg);
            }
            posArgCount++;
        }
    }
}

}