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
#include <string.h>
#include "ARM.h"
#include "Platform.h"
#include "GBACart.h"

#define GBA_ROM_MASK 0x01FFFFFFF

#define GBAMP_MASK       0x008E0000
#define GBAMP_OFFSET     0x00000000
#define GBAMP_OFFSET_ALT 0x00800000
#define GBAMP_CF_OFFSET(i) ((i) * 0x20000)

#define GBAMP_CF_DATA (GBAMP_OFFSET + GBAMP_CF_OFFSET(0))
#define GBAMP_CF_ERROR (GBAMP_OFFSET + GBAMP_CF_OFFSET(1))
#define GBAMP_CF_FEATURES (GBAMP_OFFSET + GBAMP_CF_OFFSET(1))
#define GBAMP_CF_SECTORS (GBAMP_OFFSET + GBAMP_CF_OFFSET(2))
#define GBAMP_CF_LBA0 (GBAMP_OFFSET + GBAMP_CF_OFFSET(3))
#define GBAMP_CF_LBA1 (GBAMP_OFFSET + GBAMP_CF_OFFSET(4))
#define GBAMP_CF_LBA2 (GBAMP_OFFSET + GBAMP_CF_OFFSET(5))
#define GBAMP_CF_DRIVE (GBAMP_OFFSET + GBAMP_CF_OFFSET(6))
#define GBAMP_CF_STATUS (GBAMP_OFFSET + GBAMP_CF_OFFSET(7))
#define GBAMP_CF_COMMAND (GBAMP_OFFSET + GBAMP_CF_OFFSET(7))
#define GBAMP_CF_STATUS_ALT (GBAMP_OFFSET_ALT + GBAMP_CF_OFFSET(6))

#define CF_DRIVE_LBA 0x40

#define CF_STATUS_ERR 0x01
#define CF_STATUS_DRQ 0x08
#define CF_STATUS_DSC 0x10 /* disk seek complete */
#define CF_STATUS_DF  0x20
#define CF_STATUS_RDY 0x40 /* ready */
#define CF_STATUS_BSY 0x80

#define CF_COMMAND_READ 0x20
#define CF_COMMAND_WRITE 0x30

// TODO: Does the addressing wrap around over any bits?
namespace GBACart_GBAMP
{
    // IDE registers
    u32 lba;
    u8 features, drive, status, sectors, error;

    // drive state
    FILE *image;
    bool initialized;

    // command state
    u8 command;
    u16 *data_buffer;
    u32 data_buffer_pos, data_buffer_size;

    void FreeBuffers() {
        if (data_buffer != NULL) {
            free(data_buffer);
            data_buffer = NULL;
        }
    }

    void Unload() {
        if (image != NULL) {
            fclose(image);
        }
    }

    bool Load(const char *path) {
        Unload();

        image = fopen(path, "rb");
        if (image == NULL) {
            return false;
        }

        return true;
    }

    void DeInit() {
        FreeBuffers();
    }

    bool Init() {
        if (initialized) {
            DeInit();
        }

        drive = 0xA0 | CF_DRIVE_LBA;
        status = CF_STATUS_DSC | CF_STATUS_RDY;
    ;
        data_buffer = NULL;

        initialized = true;
        return true;
    }

    u32 GetSector() {
        if (drive & CF_DRIVE_LBA) {
            return ((drive & 0x0F) << 24) | (lba & 0xFFFFFF);
        } else {
            printf("GBACart_GBAMP: CHS mode not yet implemented\n");
            return 0;
        }
    }

    void RunCommand(u8 cmd) {
        command = cmd;

        FreeBuffers();
        status &= ~CF_STATUS_DRQ;

        switch (cmd) {
            case CF_COMMAND_READ: {
                u32 file_pos = GetSector() * 512;
                data_buffer_pos = 0;
                data_buffer_size = (sectors == 0 ? 256 : sectors) * 0x100;
                data_buffer = (u16*) malloc(data_buffer_size * sizeof(u16));
                printf("GBACart_GBAMP: command: read %d bytes @ %d\n", data_buffer_size, file_pos);
                if (image != NULL) {
                    fseek(image, file_pos, 0);
                    for (u32 i = 0; i < data_buffer_size; i++) {
                        u8 b0 = (u8) fgetc(image);
                        u8 b1 = (u8) fgetc(image);
                        data_buffer[i] = b0 | (b1 << 8); // TODO: verify endianness
                    }
                }
                status |= CF_STATUS_DRQ;
                break;
            }
            case CF_COMMAND_WRITE: {
                data_buffer_pos = 0;
                data_buffer_size = (sectors == 0 ? 256 : sectors) * 0x100;
                data_buffer = (u16*) malloc(data_buffer_size * sizeof(u16));
                printf("GBACart_GBAMP: command: write %d bytes\n", data_buffer_size);
                status |= CF_STATUS_DRQ;
                break;
            }
            default:
                printf("GBACart_GBAMP: unsupported CF command 0x%02X\n", cmd);
                break;
        }
    }

    u16 Read16(u32 addr) {
        switch (addr & GBAMP_MASK) {
            case GBAMP_CF_DATA: {
                if (data_buffer == NULL || data_buffer_pos >= data_buffer_size) {
                    command = 0x00;
                    status &= ~CF_STATUS_DRQ;
                    return 0;
                }

                switch (command) {
                    case CF_COMMAND_READ: {
                        u16 value = data_buffer[data_buffer_pos];
                        data_buffer_pos++;
                        return value;
                    }
                    default:
                        return 0x00;
                }
                break;
            }
            case GBAMP_CF_ERROR:
                return error;
            case GBAMP_CF_SECTORS:
                return sectors;
            case GBAMP_CF_LBA0:
                return (lba >> 0) & 0xFF;
            case GBAMP_CF_LBA1:
                return (lba >> 8) & 0xFF;
            case GBAMP_CF_LBA2:
                return (lba >> 16) & 0xFF;
            case GBAMP_CF_DRIVE:
                return drive;
            case GBAMP_CF_STATUS:
            case GBAMP_CF_STATUS_ALT:
                return status;
            default:
                return 0xFFFF;
        }
    }

    void Write16(u32 addr, u16 value) {
        switch (addr & GBAMP_MASK) {
            case GBAMP_CF_DATA:
                switch (command) {
                    case CF_COMMAND_WRITE:
                        if (data_buffer == NULL) {
                            return;
                        }
                        
                        if (data_buffer_pos >= data_buffer_size) {
                            if (image != NULL) {
                                u32 file_pos = GetSector() * 512;
                                fseek(image, file_pos, 0);
                                for (u32 i = 0; i < data_buffer_size; i++) {
                                    // TODO: verify endianness
                                    fputc(data_buffer[i] & 0xFF, image);
                                    fputc(data_buffer[i] >> 8, image);
                                }
                            }
                            status &= ~CF_STATUS_DRQ;
                        } else {
                            data_buffer[data_buffer_pos++] = value;
                        }
                        return;
                    default:
                        return;
                }
                break;
            case GBAMP_CF_FEATURES:
                features = value & 0xFF;
                break;
            case GBAMP_CF_SECTORS:
                sectors = value & 0xFF;
                break;
            case GBAMP_CF_LBA0:
                lba = (lba & ~(0x0000FF)) | ((value & 0xFF) << 0);
                break;
            case GBAMP_CF_LBA1:
                lba = (lba & ~(0x00FF00)) | ((value & 0xFF) << 8);
                break;
            case GBAMP_CF_LBA2:
                lba = (lba & ~(0xFF0000)) | ((value & 0xFF) << 16);
                break;
            case GBAMP_CF_DRIVE:
                drive = value & 0xFF;
                break;
            case GBAMP_CF_COMMAND:
                RunCommand(value & 0xFF);
                break;
        }
    }
}

namespace GBACart {

bool gbamp_enabled = true;

bool Init() {
    if (!GBACart_GBAMP::Init()) return false;
    GBACart_GBAMP::Load("./cf.img");
    return true;
}

void DeInit() {
    GBACart_GBAMP::DeInit();
}

u8 Read8(u32 addr) {
    addr &= GBA_ROM_MASK;
    printf("GBACart: Read8 %08X\n", addr);
    if (gbamp_enabled) {
        return (u8) GBACart_GBAMP::Read16(addr);
    }
    // TODO
    return 0xFF;
}
u16 Read16(u32 addr) {
    addr &= GBA_ROM_MASK;
    printf("GBACart: Read16 %08X\n", addr);
    if (gbamp_enabled) {
        return GBACart_GBAMP::Read16(addr);
    }
    // TODO
    return 0xFFFF;
}
u32 Read32(u32 addr) {
    addr &= GBA_ROM_MASK;
    printf("GBACart: Read32 %08X\n", addr);
    // TODO
    return 0xFFFFFFFF;
}

void Write8(u32 addr, u8 val) {
    addr &= GBA_ROM_MASK;
    if (gbamp_enabled) {
        GBACart_GBAMP::Write16(addr, val);
    }
    printf("GBACart: Write8 %08X %02X\n", addr, val);
    // TODO
}
void Write16(u32 addr, u16 val) {
    addr &= GBA_ROM_MASK;
    if (gbamp_enabled) {
        GBACart_GBAMP::Write16(addr, val);
    }
    printf("GBACart: Write16 %08X %04X\n", addr, val);
    // TODO
}
void Write32(u32 addr, u32 val) {
    // TODO
    printf("GBACart: Write32 %08X %08X\n", addr, val);
}

}