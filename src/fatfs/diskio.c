/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various existing       */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"            /* Obtains integer types */
#include "diskio.h"        /* Declarations of disk functions */
#include <mocha/fsa.h>
#include <coreinit/filesystem.h>
#include <coreinit/time.h>
#include <whb/log_console.h>
#include <whb/log.h>
#include "../ios_fs.h"
#include "devices.h"

static FSClient *fsClient;
static uint8_t staticBuf[512];


static DSTATUS tryMountingFatDrive(int pdrv, const char *path) {
    int fd = -1;
    int res = FSAEx_RawOpen(fsClient, path, &fd);
    if (res < 0 || fd < 0) return STA_NODISK;

    res = FSAEx_RawRead(fsClient, staticBuf, 512, 1, 0, fd);
    if (res < 0 || fd < 0) {
        goto unmount;
    }

    if (staticBuf[0x1FE] == 0x55 && (staticBuf[0x1FF] == 0xAA || staticBuf[0x1FF] == 0xAB)) {
        deviceFds[pdrv] = fd;
        devicePaths[pdrv] = path;
        return 0;
    }

    unmount:
    FSAEx_RawClose(fsClient, fd);
    return STA_NOINIT;
}


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status(
        BYTE pdrv        /* Physical drive number to identify the drive */
) {
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
    if (deviceFds[pdrv] < 0) {
        return STA_NOINIT;
    }
    return 0;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(
        BYTE pdrv                /* Physical drive number to identify the drive */
) {
    fsClient = initFs();
    if (fsClient == NULL) {
        return STA_NOINIT;
    }

    DSTATUS success;

    switch (pdrv) {
        case DEV_SD:
            return tryMountingFatDrive(pdrv, SD_PATH);
        case DEV_USB_EXT:
            success = tryMountingFatDrive(pdrv, USB_EXT1_PATH);
            if (success == 0) return success;
            return tryMountingFatDrive(pdrv, USB_EXT2_PATH);
        default:
            return STA_NODISK;
    }
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(
        BYTE pdrv,        /* Physical drive number to identify the drive */
        BYTE *buff,        /* Data buffer to store read data */
        LBA_t sector,    /* Start sector in LBA */
        UINT count        /* Number of sectors to read */
) {
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
    // sector size 512 bytes
    if (deviceFds[pdrv] < 0) return RES_NOTRDY;
    int res = FSAEx_RawRead(fsClient, buff, 512, count, sector, deviceFds[pdrv]);
    if (res < 0) return RES_ERROR;

    // Make stealthed drives accessible
    if (sector == 0 && buff[0x1FF] == 0xAB) {
        buff[0x1FF] = 0xAA;
    }

    return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write(
        BYTE pdrv,            /* Physical drive nmuber to identify the drive */
        const BYTE *buff,    /* Data to be written */
        LBA_t sector,        /* Start sector in LBA */
        UINT count            /* Number of sectors to write */
) {
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
    if (deviceFds[pdrv] < 0) return RES_NOTRDY;
    int res = FSAEx_RawWrite(fsClient, (const void *) buff, 512, count, sector, deviceFds[pdrv]);
    if (res < 0) return RES_ERROR;

    return RES_OK;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(
        BYTE pdrv,        /* Physical drive nmuber (0..) */
        BYTE cmd,        /* Control code */
        void *buff        /* Buffer to send/receive control data */
) {
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
    int res;
    uint8_t ioctl_buf[0x28];

    if (deviceFds[pdrv] < 0) return RES_NOTRDY;
    const char *devicePath = devicePaths[pdrv];

    switch (cmd) {
        case CTRL_SYNC:
            res = FSAEx_FlushVolume(fsClient, devicePath);
            if (res) return RES_ERROR;
            return RES_OK;
        case GET_SECTOR_COUNT:
            res = FSAEx_GetDeviceInfo(fsClient, devicePath, 0x4, ioctl_buf);
            if (res) return RES_ERROR;
            *(LBA_t *) buff = *(uint64_t *) &ioctl_buf[0x08];
            return RES_OK;
        case GET_SECTOR_SIZE:
            res = FSAEx_GetDeviceInfo(fsClient, devicePath, 0x4, ioctl_buf);
            if (res) return RES_ERROR;
            *(WORD *) buff = *(uint32_t *) &ioctl_buf[0x10];
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(WORD *) buff = 1;
            return RES_OK;
        case CTRL_TRIM:
            return RES_OK;
    }

    return RES_PARERR;
}

DWORD get_fattime() {
    OSCalendarTime output;
    OSTicksToCalendarTime(OSGetTime(), &output);
    return (DWORD) (output.tm_year - 1980) << 25 |
           (DWORD) (output.tm_mon + 1) << 21 |
           (DWORD) output.tm_mday << 16 |
           (DWORD) output.tm_hour << 11 |
           (DWORD) output.tm_min << 5 |
           (DWORD) output.tm_sec >> 1;
}