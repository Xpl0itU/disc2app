#pragma once

#include <coreinit/filesystem.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __FSAShimSend                ((FSError(*)(FSAShimBuffer *, uint32_t))(0x101C400 + 0x042d90))

FSError FSA_Ioctl(FSClient *client, int ioctl, void *inBuf, uint32_t inLen, void *outBuf, uint32_t outLen);
FSError FSA_IoctlEx(int clientHandle, int ioctl, void *inBuf, uint32_t inLen, void *outBuf, uint32_t outLen);
FSError FSA_Ioctlv(FSClient *client, uint32_t request, uint32_t vectorCountIn, uint32_t vectorCountOut, IOSVec *vector);
FSError FSA_IoctlvEx(int clientHandle, uint32_t request, uint32_t vectorCountIn, uint32_t vectorCountOut, IOSVec *vector);
int FSAEx_FlushVolume(FSClient *client, const char *volume_path);
int FSAEx_GetDeviceInfo(FSClient *client, const char *device_path, int type, void *out_data);

FSClient *initFs();
int cleanupFs();

#ifdef __cplusplus
}
#endif