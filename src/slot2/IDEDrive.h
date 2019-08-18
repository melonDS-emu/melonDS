/*
    Copyright (c) 2019 Adrian "asie" Siekierka

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

#ifndef IDEDRIVE_H
#define IDEDRIVE_H

#include "../types.h"

#define IDE_REG_DATA 0
#define IDE_REG_ERROR 1
#define IDE_REG_FEATURES 1
#define IDE_REG_SECTORS 2
#define IDE_REG_LBA0 3
#define IDE_REG_LBA1 4
#define IDE_REG_LBA2 5
#define IDE_REG_DRIVE 6
#define IDE_REG_STATUS 7
#define IDE_REG_COMMAND 7
#define IDE_REG_STATUS_ALT (8|6)

class IDEDrive
{
public:
    IDEDrive();
    ~IDEDrive();

    void Reset();

    bool Open(const char *path);
    bool Close();
    bool IsValid();

    u16 Read(u8 addr);
    void Write(u8 addr, u16 val);

private:
    u32 Address;
    u8 Features;
    u8 Drive;
    u8 Status;
    u8 Sectors;
    u8 Error;

    FILE *File;

    u8 CurrentCommand;
    u16 *Buffer;
    u32 BufferPos;
    u32 BufferSize;

    void ResetBuffer();
    u32 CurrentFileOffset();
    void RunCommand(u8 cmd);
};

#endif