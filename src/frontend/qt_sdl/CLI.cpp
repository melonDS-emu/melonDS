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
#include <QDebug>
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

    parser.addPositionalArgument("arg1", "First argument");
    parser.addPositionalArgument("arg2", "Second argument");

    parser.addOption(QCommandLineOption("b", "Whether to boot firmware on startup. Defaults to \"auto\" (boot if NDS rom given)", "auto/always/never", "auto"));
    parser.addOption(QCommandLineOption("f", "Start melonDS in fullscreen mode"));
    parser.addOption(QCommandLineOption("xt", "Extract the title of specified rom in specified language. This is split into the MainTitle;Subtitle;Publisher. Supported language codes: ja, en, fr, ge, it, es, ko, zh. If no language code is given, it defaults to english"));
    parser.addOption(QCommandLineOption("xgt", "Extract the game title of specified rom"));
    parser.addOption(QCommandLineOption("xgc", "Extract the game code of specified rom"));
    parser.addOption(QCommandLineOption("xi", "Extract the icon of specified rom"));
    parser.addOption(QCommandLineOption("xai", "Extract the animated icon of specified rom"));

#ifdef ARCHIVE_SUPPORT_ENABLED
    parser.addOption(QCommandLineOption({"a", "archive-file"}, "Specify file to load inside an archive given (NDS)", "rom"));
    parser.addOption(QCommandLineOption({"A", "archive-file-gba"}, "Specify file to load inside an archive given (GBA)", "rom"));
#endif

    parser.process(melon);

    CommandLineOptions* options = new CommandLineOptions;
    QStringList posargs = parser.positionalArguments();

    options->fullscreen = parser.isSet("fullscreen");
    options->headless = false;
    if (parser.isSet("xt")){
        if (posargs.size() == 2){
            return options;
        } else if (posargs.size() == 1){
            return options;
        } else {
            Log(LogLevel::Error, "Error: Incorrect amount of arguments\n");
            exit(1);
        }
    } else if (parser.isSet("xgt")){
        if (posargs.size() == 1){
            return options;
        } else {
            Log(LogLevel::Error, "Error: Incorrect amount of arguments\n");
            exit(1);
        }
    } else if (parser.isSet("xgc")){
        if (posargs.size() == 1){
            melonDS::Platform::setMuteLogs(true);
            qDebug() << "muteLogs = " << melonDS::Platform::getMuteLogs();
            qDebug() << "Entered xgc";
            options->headless = true;
            options->dsRomPath = posargs[0];
            return options;
        } else {
            Log(LogLevel::Error, "Error: Incorrect amount of arguments\n");
            exit(1);
        }
    } else if (parser.isSet("xi")){
        if (posargs.size() == 1){
            return options;
        } else {
            Log(LogLevel::Error, "Error: Incorrect amount of arguments\n");
            exit(1);
        }
    } else if (parser.isSet("xai")){
        if (posargs.size() == 1){
            return options;
        } else {
            Log(LogLevel::Error, "Error: Incorrect amount of arguments\n");
            exit(1);
        }
    }
    options->boot = false;

    switch (posargs.size())
    {
        default:
            Log(LogLevel::Warn, "Too many positional arguments; ignoring 3 onwards\n");
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
        Log(LogLevel::Error, "ERROR: -b/--boot only accepts auto/always/never as arguments\n");
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
            Log(LogLevel::Error, "Option -a/--archive-file given, but no archive specified!");
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
            Log(LogLevel::Error, "Option -A/--archive-file-gba given, but no archive specified!");
        }
    }
#endif

    return options;
}

}
