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

#include "FATIO.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"

using namespace melonDS;

static ff_disk_read_cb ReadCb;
static ff_disk_write_cb WriteCb;
static LBA_t SectorCount;
static DSTATUS Status = STA_NOINIT | STA_NODISK;


void melonDS::ff_disk_open(const ff_disk_read_cb& readcb, const ff_disk_write_cb& writecb, LBA_t seccnt)
{
    if (!readcb) return;

    ReadCb = readcb;
    WriteCb = writecb;
    SectorCount = seccnt;

    Status &= ~STA_NODISK;
    if (!writecb) Status |= STA_PROTECT;
    else          Status &= ~STA_PROTECT;
}

void melonDS::ff_disk_close()
{
    ReadCb = nullptr;
    WriteCb = nullptr;
    SectorCount = 0;

    Status &= ~STA_PROTECT;
    Status |= STA_NODISK;
}


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	return Status;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    Status &= ~STA_NOINIT;
	return Status;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (Status & (STA_NOINIT | STA_NODISK)) return RES_NOTRDY;
	if (!ReadCb) return RES_ERROR;

	UINT res = ReadCb(buff, sector, count);
	if (res != count) return RES_ERROR;
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (Status & (STA_NOINIT | STA_NODISK)) return RES_NOTRDY;
	if (Status & STA_PROTECT) return RES_WRPRT;
	if (!WriteCb) return RES_ERROR;

	UINT res = WriteCb(buff, sector, count);
	if (res != count) return RES_ERROR;
	return RES_OK;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
    switch (cmd)
    {
    case CTRL_SYNC:
        // TODO: fflush?
        return RES_OK;

    case GET_SECTOR_COUNT:
        *(LBA_t*)buff = SectorCount;
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD*)buff = 0x200;
        return RES_OK;

    case GET_BLOCK_SIZE:
        *(DWORD*)buff = 1;
        return RES_OK;

    case CTRL_TRIM:
        // TODO??
        return RES_OK;
    }

	//printf("FatFS: unknown disk_ioctl(%02X, %02X, %p)\n", pdrv, cmd, buff);
	return RES_PARERR;
}

