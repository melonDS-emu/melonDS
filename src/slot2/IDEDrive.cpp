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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Platform.h"
#include "IDEDrive.h"

#define IDE_DRIVE_LBA 0x40

#define IDE_STATUS_ERR 0x01
#define IDE_STATUS_DRQ 0x08
#define IDE_STATUS_DSC 0x10 /* disk seek complete */
#define IDE_STATUS_DF  0x20
#define IDE_STATUS_RDY 0x40 /* ready */
#define IDE_STATUS_BSY 0x80

#define IDE_COMMAND_READ 0x20
#define IDE_COMMAND_WRITE 0x30

IDEDrive::IDEDrive() {
    this->Buffer = NULL;
    this->File = NULL;
    this->Reset();
}

IDEDrive::~IDEDrive() {
    this->ResetBuffer();
    this->Close();
}

void IDEDrive::ResetBuffer() {
    if (this->Buffer != NULL) {
        delete this->Buffer;
        this->Buffer = NULL;
    }
}

void IDEDrive::Reset() {
    this->Drive = 0xA0 | IDE_DRIVE_LBA;
    this->Status = IDE_STATUS_DSC | IDE_STATUS_RDY;
    this->CurrentCommand = 0x00;

    this->ResetBuffer();    
}

bool IDEDrive::Open(const char *path) {
    this->Close();

    this->File = fopen(path, "r+b");
    if (this->File == NULL) return false;

    return true;
}

bool IDEDrive::Close() {
    if (this->File != NULL) {
        if (!fclose(this->File)) return false;
        this->File = NULL;
    }
    return true;
}

u32 IDEDrive::CurrentFileOffset() {
        if (this->Drive & IDE_DRIVE_LBA) {
            return (((this->Drive & 0x0F) << 24) | (this->Address & 0xFFFFFF)) * 512;
        } else {
            printf("IDEDrive: CHS mode not yet implemented\n");
            return 0;
        }
}


void IDEDrive::RunCommand(u8 cmd) {
    this->CurrentCommand = cmd;
    this->ResetBuffer();
    this->Status &= ~IDE_STATUS_DRQ;

    switch (cmd) {
        case IDE_COMMAND_READ: {
            u32 file_pos = this->CurrentFileOffset();
            this->BufferPos = 0;
            this->BufferSize = (this->Sectors == 0 ? 256 : this->Sectors) * 0x100;
            this->Buffer = new u16[this->BufferSize];
            printf("IDEDrive: command: read %d words @ %d\n", this->BufferSize, file_pos);
            if (this->File != NULL) {
                fseek(this->File, file_pos, 0);
                for (u32 i = 0; i < this->BufferSize; i++) {
                    u8 b0 = (u8) fgetc(this->File);
                    u8 b1 = (u8) fgetc(this->File);
                    this->Buffer[i] = b0 | (b1 << 8);
                }
            }
            this->Status |= IDE_STATUS_DRQ;
            break;
        }
        case IDE_COMMAND_WRITE: {
            this->BufferPos = 0;
            this->BufferSize = (this->Sectors == 0 ? 256 : this->Sectors) * 0x100;
            this->Buffer = new u16[this->BufferSize];
            this->Status |= IDE_STATUS_DRQ;
            break;
        }
        default:
            printf("IDEDrive: unsupported CF command 0x%02X\n", cmd);
            break;
    }
}

u16 IDEDrive::Read(u8 addr) {
    switch (addr) {
        case IDE_REG_DATA: {
            if (this->BufferPos >= this->BufferSize) {
                this->ResetBuffer();
            }

            if (this->Buffer == NULL) {
                this->CurrentCommand = 0x00;
                this->Status &= ~IDE_STATUS_DRQ;
                return 0x0000;
            }

            switch (this->CurrentCommand) {
                case IDE_COMMAND_READ: {
                    return this->Buffer[this->BufferPos++];
                }
                default:
                    return 0x0000;
            }
            break;
        }
        case IDE_REG_ERROR:
            return this->Error;
        case IDE_REG_SECTORS:
            return this->Sectors;
        case IDE_REG_LBA0:
            return (this->Address >> 0) & 0xFF;
        case IDE_REG_LBA1:
            return (this->Address >> 8) & 0xFF;
        case IDE_REG_LBA2:
            return (this->Address >> 16) & 0xFF;
        case IDE_REG_DRIVE:
            return this->Drive;
        case IDE_REG_STATUS:
        case IDE_REG_STATUS_ALT:
            return this->Status;
        default:
            return 0xFFFF;
    }
}

void IDEDrive::Write(u8 addr, u16 value) {
    switch (addr) {
        case IDE_REG_DATA:
            switch (this->CurrentCommand) {
                case IDE_COMMAND_WRITE:
                    if (this->Buffer == NULL) {
                        return;
                    }
                    
                    if (this->BufferPos < this->BufferSize)
                        this->Buffer[this->BufferPos++] = value;

                    if (this->BufferPos >= this->BufferSize) {
                        if (this->File != NULL) {
                            u32 file_pos = this->CurrentFileOffset();
                            fseek(this->File, file_pos, 0);
                            for (u32 i = 0; i < this->BufferSize; i++) {
                                fputc(this->Buffer[i] & 0xFF, this->File);
                                fputc(this->Buffer[i] >> 8, this->File);
                            }
                            printf("IDEDrive: command: write %d words @ %d\n", this->BufferSize, file_pos);
                        }
                        this->ResetBuffer();
                        this->Status &= ~IDE_STATUS_DRQ;
                    }
                    return;
                default:
                    return;
            }
            break;
        case IDE_REG_FEATURES:
            this->Features = value & 0xFF;
            break;
        case IDE_REG_SECTORS:
            this->Sectors = value & 0xFF;
            break;
        case IDE_REG_LBA0:
            this->Address = (this->Address & ~(0x0000FF)) | ((value & 0xFF) << 0);
            break;
        case IDE_REG_LBA1:
            this->Address = (this->Address & ~(0x00FF00)) | ((value & 0xFF) << 8);
            break;
        case IDE_REG_LBA2:
            this->Address = (this->Address & ~(0xFF0000)) | ((value & 0xFF) << 16);
            break;
        case IDE_REG_DRIVE:
            this->Drive = value & 0xFF;
            break;
        case IDE_REG_COMMAND:
            this->RunCommand(value & 0xFF);
            break;
    }
}