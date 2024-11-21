/*
    Copyright 2021-2023 melonDS team

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
#include "Platform.h"

using melonDS::Platform::Log;
using melonDS::Platform::LogLevel;

namespace CLI
{

CommandLineOptions* ManageArgs(QApplication& melon)
{
    QCommandLineParser parser;
    parser.addHelpOption();

    parser.addPositionalArgument("nds", "加载Slot-1的Nintendo DS ROM（或包含它的压缩包）");
    parser.addPositionalArgument("gba", "加载Slot-2的GBA ROM（或包含它的压缩包）");

    parser.addOption(QCommandLineOption({"b", "boot"}, "是否在开机时启动固件。默认为 \"auto\" (NDS文件被加载时启动)", "auto/always/never", "auto"));
    parser.addOption(QCommandLineOption({"f", "fullscreen"}, "以全屏模式启动melonDS"));

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
            Log(LogLevel::Warn, "参数过多; 将忽略第3及之后的参数\n");
        case 2:
            options->gbaRomPath = posargs[1];
        case 1:
            options->dsRomPath = posargs[0];
        case 0:
            break;
    }

    QString bootMode = parser.value("boot");
    if (bootMode == "auto")
    {
        options->boot = !posargs.empty();
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
        Log(LogLevel::Error, "错误: -b/--boot 仅接受 auto/always/never 作为指令\n");
        exit(1);
    }

#ifdef ARCHIVE_SUPPORT_ENABLED
    if (parser.isSet("archive-file"))
    {
        if (options->dsRomPath.has_value())
        {
            options->dsRomArchivePath = parser.value("archive-file");
        }
        else
        {
            Log(LogLevel::Error, "输入了 -a/--archive-file 指令, 但未输入具体压缩包文件!");
        }
    }

    if (parser.isSet("archive-file-gba"))
    {
        if (options->gbaRomPath.has_value())
        {
            options->gbaRomArchivePath = parser.value("archive-file-gba");
        }
        else
        {
            Log(LogLevel::Error, "输入了 -A/--archive-file-gba 指令, 但未输入具体压缩包文件!");
        }
    }
#endif

    return options;
}

}
