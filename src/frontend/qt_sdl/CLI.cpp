/*
    Copyright 2021-2022 melonDS team

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

#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>

#include "CLI.h"
#include "main.h"

namespace CLI
{

CommandLineOptions* ManageArgs(QApplication& melon)
{
    QCommandLineParser parser;
    parser.addHelpOption();

    parser.addPositionalArgument("nds", "Nintendo DS ROM (or an archive file which contains it) to load into Slot-1");
    parser.addPositionalArgument("gba", "GBA ROM (or an archive file which contains it) to load into Slot-2");

    parser.addOption(QCommandLineOption({"b", "boot"}, "Whether to boot firmware on startup. Defaults to auto (boot if ", "auto/always/never", "auto"));
    parser.addOption(QCommandLineOption({"f", "fullscreen"}, "Start melonDS on fullscreen mode"));

    parser.process(melon);

    CommandLineOptions* options = new CommandLineOptions{{},{}, parser.isSet("fullscreen")};
    
    //TODO: aRcHiVeS
    QStringList posargs = parser.positionalArguments();
    switch (posargs.size())
    {
        default:
            printf("Too many positional arguments; ignoring 3 onwards\n");
        case 2:
            options->gbaRomPath = QStringList(posargs[1]);
        case 1:
            options->dsRomPath = QStringList(posargs[0]);
        case 0:
            break;
    }

    QString bootMode = parser.value("boot");
    if (bootMode == "auto")
    {
        options->boot = posargs.size() > 0;
    } 
    else if (bootMode == "always")
    {
        options->boot = true;
    }
    else if (bootMode == "never")
    {
        options->boot = false;
    }
    else {
        printf("ERROR: -b/--boot only accepts auto/always/never as arguments\n");
        exit(1);
    }

    return options;
}

}