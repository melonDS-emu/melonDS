/*
    Copyright 2021 patataofcourse

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

#include <QStringList>

#include "../FrontendUtil.h"

#include "CLI.h"

namespace CLI
{

QStringList DSRomPath = {};
QStringList GBARomPath = {};
bool StartOnFullscreen = false;

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

    // don't think this will ever happen but i don't wanna risk a random segfault
    if (argc == 0)
        return;

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
                //TODO: QT options
                printf(
                   "usage: melonDS [options] ... [dspath] [gbapath]\n"
                   "Options:\n"
                   "    -h / --help         display this help message and quit\n"
                   "    -v / --verbose      toggle verbose mode\n"
                   "    -f / --fullscreen   opens melonDS on fullscreen\n"
                   "Arguments:\n"
                   "    dspath              path to a DS ROM you want to run\n"
                   "    gbapath             path to a GBA ROM you want to load in the emulated Slot 2\n"
                );
                exit(0);
            }
            else if (!strcasecmp(arg, "-f") || !strcasecmp(arg, "--fullscreen"))
            {
                StartOnFullscreen = true;
            }
            else
            {
                printf("Unrecognized option %s - run '%s --help' for more details \n", arg, argv[0]);
                exit(1);
            }
        } 
        else
        {
            switch (posArgCount)
            {
                case 0:
                    DSRomPath = QString(arg).split("|");
                    printf("DS ROM path: %s\n", arg);
                    break;
                case 1:
                    GBARomPath = QString(arg).split("|");
                    printf("GBA ROM path: %s\n", arg);
                    break;
                default:
                    printf("Too many arguments, arg '%s' will be ignored\n", arg);
            }
            posArgCount++;
        }
    }
}

}