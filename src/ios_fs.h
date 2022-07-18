#pragma once

#include <coreinit/filesystem.h>
#include <coreinit/filesystem_fsa.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __FSAShimSetupRequestMount   ((FSError(*)(FSAShimBuffer *, uint32_t, const char *, const char *, uint32_t, void *, uint32_t))(0x101C400 + 0x042f88))
#define __FSAShimSetupRequestUnmount ((FSError(*)(FSAShimBuffer *, uint32_t, const char *, uint32_t))(0x101C400 + 0x43130))
#define __FSAShimSend                ((FSError(*)(FSAShimBuffer *, uint32_t))(0x101C400 + 0x042d90))

FSError FSA_Ioctl(FSClient *client, int ioctl, void *inBuf, uint32_t inLen, void *outBuf, uint32_t outLen);
FSError FSA_IoctlEx(int clientHandle, int ioctl, void *inBuf, uint32_t inLen, void *outBuf, uint32_t outLen);
FSError FSA_Ioctlv(FSClient *client, uint32_t request, uint32_t vectorCountIn, uint32_t vectorCountOut, IOSVec *vector);
FSError FSA_IoctlvEx(int clientHandle, uint32_t request, uint32_t vectorCountIn, uint32_t vectorCountOut, IOSVec *vector);
FSError FSAEx_FlushVolume(FSClient *client, const char *volume_path);
FSError FSAEx_GetDeviceInfo(FSClient *client, const char *device_path, int type, void *out_data);
FSError FSAEx_Mount(FSClient *client, const char *source, const char *target, FSAMountFlags flags, void *arg_buf, uint32_t arg_len);
FSError FSAEx_Unmount(FSClient *client, const char *mountedTarget, FSAUnmountFlags flags);

FSClient *initFs();
int cleanupFs();

#ifdef __cplusplus
}
#endif