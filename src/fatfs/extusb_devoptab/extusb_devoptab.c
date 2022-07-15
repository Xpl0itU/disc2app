#include <sys/iosupport.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../devices.h"
#include "extusb_devoptab.h"

#ifdef __cplusplus
extern "C" {
#endif

static devoptab_t
        __extusb_fs_devoptab =
        {
                .name         = "extusb",
                .structSize   = sizeof(__extusb_fs_file_t),
                .open_r       = __extusb_fs_open,
                .close_r      = __extusb_fs_close,
                .write_r      = __extusb_fs_write,
                .read_r       = __extusb_fs_read,
                .seek_r       = __extusb_fs_seek,
                .fstat_r      = __extusb_fs_fstat,
                .stat_r       = __extusb_fs_stat,
                .link_r       = __extusb_fs_link,
                .unlink_r     = __extusb_fs_unlink,
                .chdir_r      = __extusb_fs_chdir,
                .rename_r     = __extusb_fs_rename,
                .mkdir_r      = __extusb_fs_mkdir,
                .dirStateSize = sizeof(__extusb_fs_dir_t),
                .diropen_r    = __extusb_fs_diropen,
                .dirreset_r   = __extusb_fs_dirreset,
                .dirnext_r    = __extusb_fs_dirnext,
                .dirclose_r   = __extusb_fs_dirclose,
                .statvfs_r    = __extusb_fs_statvfs,
                .ftruncate_r  = __extusb_fs_ftruncate,
                .fsync_r      = __extusb_fs_fsync,
                .deviceData   = NULL,
                .chmod_r      = __extusb_fs_chmod,
                .fchmod_r     = __extusb_fs_fchmod,
                .rmdir_r      = __extusb_fs_rmdir,
        };

static BOOL __extusb_fs_initialised = FALSE;

FRESULT init_extusb_devoptab() {
    FRESULT fr = FR_OK;

    if (__extusb_fs_initialised) {
        return fr;
    }

    __extusb_fs_devoptab.deviceData = memalign(0x20, sizeof(FATFS));
    char mountPath[0x80];
    sprintf(mountPath, "%d:", DEV_USB_EXT);

    int dev = AddDevice(&__extusb_fs_devoptab);
    if (dev != -1) {
        setDefaultDevice(dev);
        __extusb_fs_initialised = TRUE;

        // Mount the external USB drive
        fr = f_mount(__extusb_fs_devoptab.deviceData, mountPath, 1);

        if (fr != FR_OK) {
            free(__extusb_fs_devoptab.deviceData);
            __extusb_fs_devoptab.deviceData = NULL;
            return fr;
        }
        char workDir[0x83];
        // chdir to external USB root for general use
        strcpy(workDir, __extusb_fs_devoptab.name);
        strcat(workDir, "/");
        chdir(workDir);
    } else {
        f_unmount(mountPath);
        free(__extusb_fs_devoptab.deviceData);
        __extusb_fs_devoptab.deviceData = NULL;
        return dev;
    }

    return fr;
}

FRESULT
fini_extusb_devoptab() {
    FRESULT fr = FR_OK;

    if (!__extusb_fs_initialised) {
        return fr;
    }

    RemoveDevice(__extusb_fs_devoptab.name);

    char mountPath[0x80];
    sprintf(mountPath, "%d:", DEV_USB_EXT);
    f_unmount(mountPath);
    free(__extusb_fs_devoptab.deviceData);
    __extusb_fs_devoptab.deviceData = NULL;
    __extusb_fs_initialised = FALSE;

    return fr;
}

#ifdef __cplusplus
}
#endif