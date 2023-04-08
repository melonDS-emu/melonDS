/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */


static ff_disk_read_cb ReadCb;
static ff_disk_write_cb WriteCb;
static LBA_t SectorCount;
static DSTATUS Status = STA_NOINIT | STA_NODISK;


void ff_disk_open(ff_disk_read_cb readcb, ff_disk_write_cb writecb, LBA_t seccnt)
{
    if (!readcb) return;

    ReadCb = readcb;
    WriteCb = writecb;
    SectorCount = seccnt;

    Status &= ~STA_NODISK;
    if (!writecb) Status |= STA_PROTECT;
    else          Status &= ~STA_PROTECT;
}

void ff_disk_close(void)
{
    ReadCb = (void*)0;
    WriteCb = (void*)0;
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

