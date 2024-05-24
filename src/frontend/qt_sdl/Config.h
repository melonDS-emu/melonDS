/*
    Copyright 2016-2023 melonDS team

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

#ifndef PLATFORMCONFIG_H
#define PLATFORMCONFIG_H

#include <variant>
#include <string>
#include <QString>
#include <unordered_map>
#include <tuple>

//#define TOML_HEADER_ONLY 0
//#include "toml/toml.hpp"

#ifndef TOML11_VALUE_HPP
namespace toml
{
class value;
}
#endif

namespace Config
{

struct LegacyEntry
{
    char Name[32];
    int Type;           // 0=int 1=bool 2=string 3=64bit int
    char TOMLPath[64];
    bool InstanceUnique; // whether the setting can exist individually for each instance in multiplayer
};

struct ConfigEntry
{
    char Name[32];
    int Type;           // 0=int 1=bool 2=string 3=64bit int
    void* Value;        // pointer to the value variable
    std::variant<int, bool, std::string, int64_t> Default;
    bool InstanceUnique; // whether the setting can exist individually for each instance in multiplayer
};

struct CameraConfig
{
    int InputType; // 0=blank 1=image 2=camera
    std::string ImagePath;
    std::string CamDeviceName;
    bool XFlip;
};

template<typename T>
using DefaultList = std::unordered_map<std::string, T>;

using RangeList = std::unordered_map<std::string, std::tuple<int, int>>;

class Table;

class Array
{
public:
    Array(toml::value& data);
    ~Array() {}

    size_t Size();

    void Clear();

    Array GetArray(const int id);

    int GetInt(const int id);
    int64_t GetInt64(const int id);
    bool GetBool(const int id);
    std::string GetString(const int id);

    void SetInt(const int id, int val);
    void SetInt64(const int id, int64_t val);
    void SetBool(const int id, bool val);
    void SetString(const int id, const std::string& val);

    // convenience

    QString GetQString(const int id)
    {
        return QString::fromStdString(GetString(id));
    }

    void SetQString(const int id, const QString& val)
    {
        return SetString(id, val.toStdString());
    }

private:
    toml::value& Data;
};

class Table
{
public:
    //Table();
    Table(toml::value& data, const std::string& path);
    ~Table() {}

    Table& operator=(const Table& b);

    Array GetArray(const std::string& path);
    Table GetTable(const std::string& path, const std::string& defpath = "");

    int GetInt(const std::string& path);
    int64_t GetInt64(const std::string& path);
    bool GetBool(const std::string& path);
    std::string GetString(const std::string& path);

    void SetInt(const std::string& path, int val);
    void SetInt64(const std::string& path, int64_t val);
    void SetBool(const std::string& path, bool val);
    void SetString(const std::string& path, const std::string& val);

    // convenience

    QString GetQString(const std::string& path)
    {
        return QString::fromStdString(GetString(path));
    }

    void SetQString(const std::string& path, const QString& val)
    {
        return SetString(path, val.toStdString());
    }

private:
    toml::value& Data;
    std::string PathPrefix;

    toml::value& ResolvePath(const std::string& path);
    template<typename T> T FindDefault(const std::string& path, T def, DefaultList<T> list);
};


extern int WindowWidth;
extern int WindowHeight;
extern bool WindowMaximized;

extern int ScreenRotation;
extern int ScreenGap;
extern int ScreenLayout;
extern bool ScreenSwap;
extern int ScreenSizing;
extern int ScreenAspectTop;
extern int ScreenAspectBot;
extern bool IntegerScaling;
extern bool ScreenFilter;

extern bool ScreenUseGL;
extern bool ScreenVSync;
extern int ScreenVSyncInterval;

extern int _3DRenderer;
extern bool Threaded3D;

extern int GL_ScaleFactor;
extern bool GL_BetterPolygons;
extern bool GL_HiresCoordinates;

extern std::string LANDevice;
extern bool DirectLAN;

extern CameraConfig Camera[2];


bool Load();
void Save();

Table GetLocalTable(int instance);
inline Table GetGlobalTable() { return GetLocalTable(-1); }

}

#endif // PLATFORMCONFIG_H
