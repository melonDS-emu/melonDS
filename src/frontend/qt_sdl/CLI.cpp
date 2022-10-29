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

namespace CLI
{

CommandLineOptions* ManageArgs(QApplication& melon)
{
    QCommandLineParser parser;
    parser.addHelpOption();

    parser.addPositionalArgument("nds", "加载到Slot-1的Nintendo DS ROM（或包含它的文档）");
    parser.addPositionalArgument("gba", "加载到Slot-2的GBA ROM（或包含它的文档）");

    parser.addOption(QCommandLineOption({"b", "boot"}, "是否在开机时启动固件。默认为\"自动\" (有加载ndsROM就启动)", "auto/always/never", "auto"));
    parser.addOption(QCommandLineOption({"f", "fullscreen"}, "全屏模式启动melonDS"));
    
#ifdef ARCHIVE_SUPPORT_ENABLED
    parser.addOption(QCommandLineOption({"a", "archive-file"}, "包含NDS的文档", "rom"));
    parser.addOption(QCommandLineOption({"A", "archive-file-gba"}, "包含GBA的文档", "rom"));
#endif

    parser.process(melon);

    CommandLineOptions* options = new CommandLineOptions;

    options->fullscreen = parser.isSet("fullscreen");
    
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
    else
    {
        printf("错误：-b/--boot 仅接受 auto/always/never 作为指令\n");
        exit(1);
    }

#ifdef ARCHIVE_SUPPORT_ENABLED
    if (parser.isSet("archive-file"))
    {
        if (options->dsRomPath.isEmpty())
        {
            options->errorsToDisplay += "Option -a/--archive-file given, but no archive specified!";
        }
        else
        {
            options->dsRomPath += parser.value("archive-file");
        }
    } 
    else if (!options->dsRomPath.isEmpty())
    {
        //TODO-CLI: try to automatically find ROM
        QStringList paths = options->dsRomPath[0].split("|");
        if (paths.size() >= 2)
        {
            printf("Warning: use the a.zip|b.nds format at your own risk!\n");
            options->dsRomPath = paths;
        }
    }

    if (parser.isSet("archive-file-gba"))
    {
        if (options->gbaRomPath.isEmpty())
        {
            options->errorsToDisplay += "Option -A/--archive-file-gba given, but no archive specified!";
        }
        else
        {
            options->gbaRomPath += parser.value("archive-file-gba");
        }
    }
    else if (!options->gbaRomPath.isEmpty())
    {
        //TODO-CLI: try to automatically find ROM
        QStringList paths = options->gbaRomPath[0].split("|");
        if (paths.size() >= 2)
        {
            printf("Warning: use the a.zip|b.gba format at your own risk!\n");
            options->gbaRomPath = paths;
        }
    }
#endif

    return options;
}

}