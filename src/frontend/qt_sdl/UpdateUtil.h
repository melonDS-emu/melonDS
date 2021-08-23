/*
    Copyright 2016-2021 Arisotura, WaluigiWare64

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

#ifndef UPDATEUTIL_H
#define UPDATEUTIL_H

#include <QString>

namespace Updater
{

enum
{
    updateChannel_Stable = 0,
    updateChannel_Dev    = 1
};


static QString AzureProject = "melonDS/melonDS";
static QString GitHubRepo = "Arisotura/melonDS";
#if defined(__APPLE__)
    #define CI_PLATFORM_AZURE
    #if defined(__x86_64__)
        static QString PipelineName = "CMake Build (macOS x86-64)";
        static QString StableName = "melonDS_%1_mac64.dmg";
    #elif defined(__aarch64__)
        static QString PipelineName = "CMake Build (macOS ARM64)";
        static QString StableName = "melonDS_%1_macARM.dmg";
    #endif

#elif defined(__linux__)
    #define CI_PLATFORM_GITHUB
    #if defined(__x86_64__)
        static QString PipelineFile = "build-ubuntu.yml";
        static QString StableName = "melonDS_%1_linux64.7z";
    #elif defined(__aarch64__)
        static QString PipelineFile = "build-ubuntu-aarch64.yml";
        static QString StableName = "melonDS_%1_linuxARM.7z";
    #endif

#elif defined(__x86_64__) && defined(_WIN32)
    #define CI_PLATFORM_GITHUB
    static QString PipelineFile = "build-windows.yml";
    static QString StableName = "melonDS_%1_win64.7z";
#endif

#if defined(CI_PLATFORM_GITHUB)
bool CheckForUpdatesDevGitHub(QString& errString, QString& latestVersion, QByteArray token);
#elif defined(CI_PLATFORM_AZURE)
bool CheckForUpdatesDevAzure(QString& errString, QString& latestVersion);
#endif

bool CheckForUpdatesStable(QString& errString, QString& latestVersion);

QString DownloadUpdate();

}

#endif // UPDATEUTIL_H
