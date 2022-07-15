#include "ios_fs.h"
#include <coreinit/cache.h>
#include <coreinit/filesystem.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <mocha/mocha.h>

#ifdef __cplusplus
extern "C" {
#endif

static FSClient fsClient;
static bool initialized = false;

FSClient *initFs() {
    if (initialized) {
        return &fsClient;
    }

    WHBLogPrint("FSInit()");
    WHBLogConsoleDraw();
    FSInit();
    WHBLogPrint("FSAddClient()");
    WHBLogConsoleDraw();
    if (FSAddClient(&fsClient, FS_ERROR_FLAG_ALL) != FS_STATUS_OK) {
        WHBLogPrint("FSAddClient failed! Press any key to exit");
        WHBLogConsoleDraw();
        return NULL;
    }
    WHBLogPrint("Mocha_UnlockFSClient()");
    WHBLogConsoleDraw();
    int returncode = Mocha_UnlockFSClient(&fsClient);
    if (returncode < 0) {
        WHBLogPrintf("UnlockFSClient failed %d! Press any key to exit", returncode);
        WHBLogConsoleDraw();
        return NULL;
    }

    initialized = true;
    return &fsClient;
}

int cleanupFs() {
    FSDelClient(&fsClient, FS_ERROR_FLAG_ALL);
    initialized = false;
    return 0;
}


FSError FSA_Ioctl(FSClient *client, int ioctl, void *in_buf, uint32_t in_len, void *out_buf, uint32_t out_len) {
    if (!client) {
        return FS_ERROR_INVALID_CLIENTHANDLE;
    }

    int handle = FSGetClientBody(client)->clientHandle;
    return FSA_IoctlEx(handle, ioctl, in_buf, in_len, out_buf, out_len);
}

FSError FSA_IoctlEx(int clientHandle, int ioctl, void *in_buf, uint32_t in_len, void *out_buf, uint32_t out_len) {
    WHBLogPrintf("ioctl %d", ioctl);
    WHBLogPrintf("clientHandle %d", clientHandle);
    WHBLogPrintf("in_buf %p", in_buf);
    WHBLogPrintf("in_len %u", in_len);
    WHBLogPrintf("out_buf %p", out_buf);
    WHBLogPrintf("out_len %u", out_len);
    WHBLogConsoleDraw();
    int ret = IOS_Ioctl(clientHandle, ioctl, in_buf, in_len, out_buf, out_len);
    WHBLogPrintf("ret %d", ret);
    WHBLogConsoleDraw();
    return (FSError) ret;
}


FSError FSA_Ioctlv(FSClient *client, uint32_t request, uint32_t vectorCountIn, uint32_t vectorCountOut, IOSVec *vector) {
    if (!client) {
        return FS_ERROR_INVALID_CLIENTHANDLE;
    }

    int handle = FSGetClientBody(client)->clientHandle;
    return FSA_IoctlvEx(handle, request, vectorCountIn, vectorCountOut, vector);
}


FSError FSA_IoctlvEx(int clientHandle, uint32_t request, uint32_t vectorCountIn, uint32_t vectorCountOut, IOSVec *vector) {
    WHBLogPrintf("ioctlv %d", request);
    WHBLogConsoleDraw();
    int ret = IOS_Ioctlv(clientHandle, request, vectorCountIn, vectorCountOut, vector);
    WHBLogPrintf("ret %d", ret);
    WHBLogConsoleDraw();
    return (FSError) ret;
}


int FSAEx_FlushVolume(FSClient *client, const char *volume_path) {
    /*
        if (!outHandle) {
        return FS_ERROR_INVALID_BUFFER;
    }
    if (!device_path) {
        return FS_ERROR_INVALID_PATH;
    }
    auto *shim = (FSAShimBuffer *) memalign(0x40, sizeof(FSAShimBuffer));
    if (!shim) {
        return FS_ERROR_INVALID_BUFFER;
    }

    shim->clientHandle   = clientHandle;
    shim->command        = FSA_COMMAND_RAW_OPEN;
    shim->ipcReqType     = FSA_IPC_REQUEST_IOCTL;
    shim->response.word0 = 0xFFFFFFFF;

    FSARequestRawOpen *requestBuffer = &shim->request.rawOpen;

    strncpy(requestBuffer->path, device_path, 0x27F);

    auto res = __FSAShimSend(shim, 0);
    if (res >= 0) {
        *outHandle = shim->response.rawOpen.handle;
    }
    free(shim);
    return res;
    */
    if (!volume_path) return FS_ERROR_INVALID_PATH;

    FSAShimBuffer *shim = (FSAShimBuffer *) memalign(0x40, sizeof(FSAShimBuffer));
    if (!shim) return FS_ERROR_INVALID_BUFFER;

    shim->clientHandle = FSGetClientBody(client)->clientHandle;
    shim->command = FSA_COMMAND_FLUSH_VOLUME;
    shim->ipcReqType = FSA_IPC_REQUEST_IOCTL;
    shim->response.word0 = 0xFFFFFFFF;

    int ret = __FSAShimSend(shim, 0);
    free(shim);
    return ret;
}


// type 4 :
// 		0x08 : device size in sectors (u64)
// 		0x10 : device sector size (u32)
int FSAEx_GetDeviceInfo(FSClient *client, const char *device_path, int type, void *out_data) {
    if (!device_path) return FS_ERROR_INVALID_PATH;
    if (!out_data) return FS_ERROR_INVALID_BUFFER;

    FSAShimBuffer *shim = (FSAShimBuffer *) memalign(0x40, sizeof(FSAShimBuffer));
    if (!shim) return FS_ERROR_INVALID_BUFFER;

    shim->clientHandle = FSGetClientBody(client)->clientHandle;
    shim->command = FSA_COMMAND_GET_INFO_BY_QUERY;
    shim->ipcReqType = FSA_IPC_REQUEST_IOCTL;
    shim->response.word0 = 0xFFFFFFFF;

    char *buf = (char*) &shim->request.rawOpen;
    strncpy(buf, device_path, 0x27F);
    *(int32_t*) &buf[0x280] = type;
/*
    uint8_t *iobuf = alloc_iobuf();
    uint32_t *in_buf  = (uint32_t *) iobuf;
    uint32_t *out_buf = (uint32_t *) &iobuf[0x520];

    strncpy((char *) &in_buf[0x01], device_path, 0x27F);
    in_buf[0x284 / 4] = type;

    int ret = FSA_Ioctl(client, 0x18, in_buf, 0x520, out_buf, 0x293);
*/
    int size = 0;

    switch (type) {
        case 0:
        case 1:
        case 7:
            size = 0x8;
            break;
        case 2:
            size = 0x4;
            break;
        case 3:
            size = 0x1E;
            break;
        case 4:
            size = 0x28;
            break;
        case 5:
            size = 0x64;
            break;
        case 6:
        case 8:
            size = 0x14;
            break;
    }

    /*
    memcpy(out_data, &out_buf[1], size);

    free(iobuf);
    return ret;
    */
    memcpy(out_data, (uint8_t*)&shim->response.rawOpen, size);

    int ret = __FSAShimSend(shim, 0);
    free(shim);
    return ret;
}


#ifdef __cplusplus
}
#endif