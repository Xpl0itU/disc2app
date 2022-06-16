/*
 * Copyright (C) 2016-2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#define _GNU_SOURCE
#include "../payload/wupserver_bin.h"
#include "draw.h"
#include "exploit.h"
#include "fst.h"
#include "log_freetype.h"
#include "structs.h"
#include "tmd.h"
#include <coreinit/cache.h>
#include <coreinit/energysaver.h>
#include <coreinit/ios.h>
#include <coreinit/mcp.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <fat.h>
#include <iosuhax.h>
#include <malloc.h>
#include <padscore/kpad.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sysapp/launch.h>
#include <unistd.h>
#include <vpad/input.h>
#include <whb/proc.h>

#include "crypto.h"

bool usb = false;
int fsaFd = -1;
int oddFd = -1;
FILE *f = NULL;

int vpadError = -1;
VPADStatus vpad;
KPADStatus kpad;

#define ALIGN_FORWARD(x, alignment) (((x) + ((alignment) -1)) & (~((alignment) -1)))

//just to be able to call async
void someFunc(void *arg) {
    (void) arg;
}

static int mcp_hook_fd = -1;
int MCPHookOpen() {
    //take over mcp thread
    mcp_hook_fd = MCP_Open();
    if (mcp_hook_fd < 0)
        return -1;
    IOS_IoctlAsync(mcp_hook_fd, 0x62, (void *) 0, 0, (void *) 0, 0, (void *) someFunc, (void *) 0);
    //let wupserver start up
    sleep(1);
    if (IOSUHAX_Open("/dev/mcp") < 0)
        return -1;
    return 0;
}

void MCPHookClose() {
    if (mcp_hook_fd < 0)
        return;
    //close down wupserver, return control to mcp
    IOSUHAX_Close();
    //wait for mcp to return
    sleep(1);
    MCP_Close(mcp_hook_fd);
    mcp_hook_fd = -1;
}

void println(const char *str, ...) {
    char *tmp = NULL;

    va_list va;
    va_start(va, str);
    if ((vasprintf(&tmp, str, va) >= 0) && (tmp != NULL))
        WHBLogPrint(tmp);

    va_end(va);
    if (tmp != NULL)
        free(tmp);
    WHBLogFreetypeDraw();
}

#define SECTOR_SIZE 2048
#define NUM_SECTORS 1024
#define MAX_SECTORS 0xBA7400

static uint64_t odd_offset = 0;

int fsa_odd_read_sectors(int fsa_fd, int fd, void *buf, unsigned int sector, unsigned int count, int retry) {
    int res;
    if (sector + count > MAX_SECTORS) return -1;
    do {
        res = IOSUHAX_FSA_RawRead(fsa_fd, buf, SECTOR_SIZE, count, sector, fd);
    } while (retry && res < 0);
    // Failed to read
    return res;
}

int fsa_odd_read(int fsa_fd, int fd, char *buf, size_t len, int retry) {
    if (!len) return 0;

    unsigned int sector = odd_offset / SECTOR_SIZE;
    // Read unaligned first sector
    size_t unaligned_length = (-odd_offset) % SECTOR_SIZE;
    if (unaligned_length > len) unaligned_length = len;
    if (unaligned_length) {
        char sector_buf[SECTOR_SIZE];
        if (fsa_odd_read_sectors(fsa_fd, fd, sector_buf, sector, 1, retry) < 0) return -1;
        memcpy(buf, sector_buf + (odd_offset % SECTOR_SIZE), unaligned_length);
        odd_offset += unaligned_length;
        buf += unaligned_length;
        len -= unaligned_length;
        sector += 1;
    }
    if (!len) return 0;
    unsigned int full_sectors = len / SECTOR_SIZE;
    if (full_sectors) {

        if (fsa_odd_read_sectors(fsa_fd, fd, buf, sector, full_sectors, retry) < 0) return -1;
        sector += full_sectors;
        odd_offset += full_sectors * SECTOR_SIZE;
        buf += full_sectors * SECTOR_SIZE;
        len -= full_sectors * SECTOR_SIZE;
    }
    // Read unaligned last sector
    if (len) {
        char sector_buf[SECTOR_SIZE];
        if (fsa_odd_read_sectors(fsa_fd, fd, sector_buf, sector, 1, retry) < 0) return -1;
        memcpy(buf, sector_buf, len);
        odd_offset += len;
    }
    return 0;
}

static void fsa_odd_seek(uint64_t offset) {
    odd_offset = offset;
}

int fsa_write(int fsa_fd, int fd, void *buf, int len) {
    int done = 0;
    uint8_t *buf_uint8_t = (uint8_t *) buf;
    while (done < len) {
        size_t write_size = len - done;
        int result = IOSUHAX_FSA_WriteFile(fsa_fd, buf_uint8_t + done, 0x01, write_size, fd, 0);
        if (result < 0)
            return result;
        else
            done += result;
    }
    return done;
}

static const char *hdrStr = "disc2app WUT Port (based on wudump and wud2app by FIX94)";
void printhdr_noflip() {
    WHBLogPrint(hdrStr);
    WHBLogPrint("");
}

static void dump() {
    WHBLogFreetypeClear();
    int line = 2;
    //will inject our custom mcp code
    println("Doing IOSU Exploit...");
    *(volatile unsigned int *) 0xF5E70000 = wupserver_bin_len;
    memcpy((void *) 0xF5E70020, &wupserver_bin, wupserver_bin_len);
    DCStoreRange((void *) 0xF5E70000, wupserver_bin_len + 0x40);
    IOSUExploit();
    int ret;
    char outDir[64];
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    EVP_MD_CTX *sha1ctx = EVP_MD_CTX_new();

    //done with iosu exploit, take over mcp
    if (MCPHookOpen() < 0) {
        println("MCP hook could not be opened!");
        return;
    }
    memset((void *) 0xF5E10C00, 0, 0x20);
    DCFlushRange((void *) 0xF5E10C00, 0x20);
    println("Done!");

    //mount with full permissions
    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        println("FSA could not be opened!");
        return;
    }
    fatMountSimple("sd", &IOSUHAX_sdio_disc_interface);
    fatMountSimple("usb", &IOSUHAX_usb_disc_interface);

    println("Please insert the disc you want to dump now to begin.");
    //wait for disc key to be written
    while (1) {
        DCInvalidateRange((void *) 0xF5E10C00, 0x20);
        if (*(volatile unsigned int *) 0xF5E10C00 != 0)
            break;
        VPADRead(0, &vpad, 1, &vpadError);
        if (vpadError == 0) {
            if (vpad.trigger & VPAD_BUTTON_HOME)
                return;
        }

        for (int i = 0; i < 4; i++) {
            uint32_t controllerType;
            // check if the controller is connected
            if (WPADProbe(i, &controllerType) != 0)
                continue;

            KPADRead(i, &kpad, 1);

            switch (controllerType) {
                case WPAD_EXT_CORE:
                    if (kpad.trigger & WPAD_BUTTON_HOME)
                        return;
                    break;
                case WPAD_EXT_CLASSIC:
                    if (kpad.classic.trigger & WPAD_CLASSIC_BUTTON_HOME)
                        return;
                    break;
                case WPAD_EXT_PRO_CONTROLLER:
                    if (kpad.pro.trigger & WPAD_PRO_BUTTON_HOME)
                        return;
                    break;
            }
        }
        usleep(50000);
    }

    //opening raw odd might take a bit
    int retry = 10;
    ret = -1;
    while (ret < 0) {
        ret = IOSUHAX_FSA_RawOpen(fsaFd, "/dev/odd01", &oddFd);
        retry--;
        if (retry < 0)
            break;
        sleep(1);
    }
    if (ret < 0) {
        println("Failed to open Raw ODD!");
        return;
    }

    //get disc name for folder
    char discId[11];
    discId[10] = '\0';
    fsa_odd_seek(0);
    if (fsa_odd_read(fsaFd, oddFd, discId, 10, 0)) {
        println("Failed to read first disc sector!");
        return;
    }
    char discStr[64];
    sprintf(discStr, "Inserted %s", discId);
    println(discStr);

    // make install dir we will write to
    char *device = (usb == false) ? "sd:" : "usb:";
    sprintf(outDir, "%s/install", device);
    mkdir(outDir, 0x600);
    sprintf(outDir, "%s/install/%s", device, discId);
    mkdir(outDir, 0x600);

    // Read common key
    uint8_t cKey[0x10];
    IOSUHAX_read_otp(cKey, (0x38 * 4) + 16);

    // Read disc key
    uint8_t discKey[0x10];
    IOSUHAX_ODM_GetDiscKey(discKey);

    uint32_t apd_enabled = 0;
    IMIsAPDEnabled(&apd_enabled);
    if (apd_enabled)
        if (IMDisableAPD() == 0)
            println("Disabled Auto Power-Down.");

    sprintf(discStr, "Converting %s to app...", discId);

    println("Reading Disc FST from WUD");
    //read out and decrypt partition table
    uint8_t *partTblEnc = aligned_alloc(0x100, 0x8000);
    fsa_odd_seek(0x18000);
    fsa_odd_read(fsaFd, oddFd, partTblEnc, 0x8000, 1);
    uint8_t iv[16];
    memset(iv, 0, 16);
    uint8_t *partTbl = aligned_alloc(0x100, 0x8000);
    decrypt_aes(partTblEnc, 0x8000, discKey, iv, partTbl);
    free(partTblEnc);

    if (*(uint32_t *) partTbl != 0xCCA6E67B) {
        println("Invalid FST!");
        sleep(5);
        return;
    }

    //make sure TOC is actually valid
    unsigned int expectedHash[5];
    expectedHash[0] = *(uint32_t *) (partTbl + 8);
    expectedHash[1] = *(uint32_t *) (partTbl + 12);
    expectedHash[2] = *(uint32_t *) (partTbl + 16);
    expectedHash[3] = *(uint32_t *) (partTbl + 20);
    expectedHash[4] = *(uint32_t *) (partTbl + 24);

    EVP_DigestInit_ex(sha1ctx, EVP_sha1(), NULL);
    EVP_DigestUpdate(sha1ctx, partTbl + 0x800, 0x7800);
    unsigned int sha1[5];
    unsigned int sha1size;

    EVP_DigestFinal_ex(sha1ctx, sha1, &sha1size);
    EVP_MD_CTX_free(sha1ctx);

    if (memcmp(sha1, expectedHash, 0x14) != 0) {
        println("Invalid TOC SHA1!");
        return;
    }

    int numPartitions = *(uint32_t *) (partTbl + 0x1C);
    int siPart;
    toc_t *tbl = (toc_t *) (partTbl + 0x800);
    void *tmdBuf = NULL;
    bool certFound = false, tikFound = false, tmdFound = false;
    uint8_t tikKey[16];

    println("Searching for SI Partition");
    //start by getting cert, tik and tmd
    for (siPart = 0; siPart < numPartitions; siPart++) {
        if (strncasecmp(tbl[siPart].name, "SI", 3) == 0)
            break;
    }
    if (strncasecmp(tbl[siPart].name, "SI", 3) != 0) {
        println("No SI Partition found!");
        return;
    }

    //dont care about first header but only about data
    uint64_t offset = ((uint64_t) tbl[siPart].offsetBE) * 0x8000;
    offset += 0x8000;
    //read out FST
    println("Reading SI FST from WUD");
    void *fstEnc = aligned_alloc(0x100, 0x8000);
    fsa_odd_seek(offset);
    fsa_odd_read(fsaFd, oddFd, fstEnc, 0x8000, 1);
    void *fstDec = aligned_alloc(0x100, 0x8000);
    memset(iv, 0, 16);
    decrypt_aes(fstEnc, 0x8000, discKey, iv, fstDec);
    free(fstEnc);
    uint32_t EntryCount = (*(uint32_t *) (fstDec + 8) << 5);
    uint32_t Entries = *(uint32_t *) (fstDec + 0x20 + EntryCount + 8);
    uint32_t NameOff = 0x20 + EntryCount + (Entries << 4);
    FEntry *fe = (FEntry *) (fstDec + 0x20 + EntryCount);

    //increase offset past fst for actual files
    offset += 0x8000;
    uint32_t entry;
    for (entry = 1; entry < Entries; ++entry) {
        if (certFound && tikFound && tmdFound)
            break;
        uint32_t cNameOffset = fe[entry].NameOffset;
        const char *name = (const char *) (fstDec + NameOff + cNameOffset);
        if (strncasecmp(name, "title.", 6) != 0)
            continue;
        uint32_t CNTSize = fe[entry].FileLength;
        uint64_t CNTOff = ((uint64_t) fe[entry].FileOffset) << 5;
        uint64_t CNT_IV = CNTOff >> 16;
        void *titleF = aligned_alloc(0x100, ALIGN_FORWARD(CNTSize, 16));
        fsa_odd_seek(offset + CNTOff);
        fsa_odd_read(fsaFd, oddFd, titleF, ALIGN_FORWARD(CNTSize, 16), 1);
        uint8_t *titleDec = aligned_alloc(0x100, ALIGN_FORWARD(CNTSize, 16));
        memset(iv, 0, 16);
        memcpy(iv + 8, &CNT_IV, 8);
        decrypt_aes(titleF, ALIGN_FORWARD(CNTSize, 16), discKey, iv, titleDec);
        free(titleF);
        char outF[64];
        sprintf(outF, "%s/%s", outDir, name);
        //just write the first found cert, they're all the same anyways
        if (strncasecmp(name, "title.cert", 11) == 0 && !certFound) {
            println("Writing title.cert");
            FILE *t = fopen(outF, "wb");
            if (t == NULL) {
                println("Failed to create file");
                return;
            }
            fwrite(titleDec, 1, CNTSize, t);
            fclose(t);
            certFound = true;
        } else if (strncasecmp(name, "title.tik", 10) == 0 && !tikFound) {
            uint32_t tidHigh = *(uint32_t *) (titleDec + 0x1DC);
            if (tidHigh == 0x00050000) {
                println("Writing title.tik");
                FILE *t = fopen(outF, "wb");
                if (t == NULL) {
                    println("Failed to create file");
                    return;
                }
                fwrite(titleDec, 1, CNTSize, t);
                fclose(t);
                tikFound = true;
                uint8_t *title_id = titleDec + 0x1DC;
                int k;
                for (k = 0; k < 8; k++) {
                    iv[k] = title_id[k];
                    iv[k + 8] = 0x00;
                }
                uint8_t *tikKeyEnc = titleDec + 0x1BF;
                decrypt_aes(tikKeyEnc, 16, cKey, iv, tikKey);
            }
        } else if (strncasecmp(name, "title.tmd", 10) == 0 && !tmdFound) {
            uint32_t tidHigh = *(uint32_t *) (titleDec + 0x18C);
            if (tidHigh == 0x00050000) {
                println("Writing title.tmd");
                FILE *t = fopen(outF, "wb");
                if (t == NULL) {
                    println("Failed to create file");
                    return;
                }
                fwrite(titleDec, 1, CNTSize, t);
                fclose(t);
                tmdFound = true;
                tmdBuf = aligned_alloc(0x100, CNTSize);
                memcpy(tmdBuf, titleDec, CNTSize);
            }
        }
        free(titleDec);
    }
    WHBLogFreetypeClear();
    free(fstDec);

    if (!tikFound || !tmdFound) {
        println("tik or tmd not found!");
        return;
    }
    TitleMetaData *tmd = (TitleMetaData *) tmdBuf;
    char gmChar[19];
    char gmmsg[64];
    uint64_t fullTid = tmd->TitleID;
    sprintf(gmChar, "GM%016" PRIx64, fullTid);
    sprintf(gmmsg, "Searching for %s Partition", gmChar);
    println(gmmsg);
    uint32_t appBufLen = SECTOR_SIZE * NUM_SECTORS;
    void *appBuf = aligned_alloc(0x100, appBufLen);
    //write game .app data next
    int gmPart;
    for (gmPart = 0; gmPart < numPartitions; gmPart++) {
        if (strncasecmp(tbl[gmPart].name, gmChar, 18) == 0)
            break;
    }
    if (strncasecmp(tbl[gmPart].name, gmChar, 18) != 0) {
        println("No GM Partition found!");
        return;
    }
    println("Reading GM Header from WUD");
    offset = ((uint64_t) tbl[gmPart].offsetBE) * 0x8000;
    uint8_t *fHdr = aligned_alloc(0x100, 0x8000);
    fsa_odd_seek(offset);
    fsa_odd_read(fsaFd, oddFd, fHdr, 0x8000, 1);
    uint32_t fHdrCnt = *(uint32_t *) (fHdr + 0x10);
    uint8_t *hashPos = fHdr + 0x40 + (fHdrCnt * 4);

    //grab FST first
    println("Reading GM FST from WUD");
    uint64_t fstSize = tmd->Contents[0].Size;
    fstEnc = aligned_alloc(0x100, ALIGN_FORWARD(fstSize, 16));
    fsa_odd_seek(offset + 0x8000);
    fsa_odd_read(fsaFd, oddFd, fstEnc, ALIGN_FORWARD(fstSize, 16), 1);
    //write FST to file
    uint32_t fstContentCid = tmd->Contents[0].ID;
    char outF[64];
    char outbuf[64];
    sprintf(outF, "%s/%08x.app", outDir, fstContentCid);
    sprintf(outbuf, "Writing %08x.app", fstContentCid);
    println(outbuf);
    FILE *t = fopen(outF, "wb");
    if (t == NULL) {
        println("Failed to create file");
        return;
    }
    fwrite(fstEnc, 1, ALIGN_FORWARD(fstSize, 16), t);
    fclose(t);
    //decrypt FST to use now
    memset(iv, 0, 16);
    uint16_t content_index = tmd->Contents[0].Index;
    memcpy(iv, &content_index, 2);
    fstDec = aligned_alloc(0x100, ALIGN_FORWARD(fstSize, 16));
    decrypt_aes(fstEnc, ALIGN_FORWARD(fstSize, 16), tikKey, iv, fstDec);
    free(fstEnc);
    app_tbl_t *appTbl = (app_tbl_t *) (fstDec + 0x20);

    //write in files
    uint16_t titleCnt = tmd->ContentCount;
    uint16_t curCont;
    char progress[64];
    for (curCont = 1; curCont < titleCnt; curCont++) {
        WHBLogFreetypeClear();
        uint64_t appOffset = ((uint64_t) appTbl[curCont].offsetBE) * 0x8000;
        uint64_t totalAppOffset = offset + appOffset;
        fsa_odd_seek(totalAppOffset);
        uint64_t tSize = tmd->Contents[curCont].Size;
        uint32_t curContentCid = tmd->Contents[curCont].ID;
        char outF[64];
        char outbuf[64];
        char titlesmsg[64];
        sprintf(titlesmsg, "Dumping title %d/%d", curCont, titleCnt - 1);
        sprintf(outF, "%s/%08x.app", outDir, curContentCid);
        sprintf(outbuf, "Writing %08x.app", curContentCid);
        printhdr_noflip();
        WHBLogPrint(discStr);
        WHBLogPrint(titlesmsg);
        WHBLogPrint(outbuf);
        WHBLogFreetypeDraw();
        line = 5;
        FILE *t = fopen(outF, "wb");
        if (t == NULL) {
            println("Failed to create file");
            return;
        }
        uint64_t total = tSize;
        while (total > 0) {
            uint32_t toWrite = ((total > (uint64_t) appBufLen) ? (appBufLen) : (uint32_t)(total));
            sprintf(progress, "0x%08X/0x%08X (%i% %)", (uint32_t)(tSize - total), (uint32_t) tSize, (uint32_t)((tSize - total) * 100 / tSize));
            fsa_odd_read(fsaFd, oddFd, appBuf, toWrite, 1);
            fwrite(appBuf, 1, toWrite, t);
            total -= toWrite;
            WHBLogFreetypeClear();
            printhdr_noflip();
            WHBLogPrint(discStr);
            WHBLogPrint(titlesmsg);
            WHBLogPrint(outbuf);
            WHBLogPrint(progress);
            WHBLogFreetypeDraw();

            if (vpad.trigger & VPAD_BUTTON_B) {
                fclose(t);
                free(fstDec);
                free(appBuf);
                return;
            }
        }
        line = 6;
        fclose(t);
        uint16_t type = tmd->Contents[curCont].Type;
        if (type & 2) //h3 hashes used
        {
            char outF[64];
            char outbuf[64];
            sprintf(outF, "%s/%08x.h3", outDir, curContentCid);
            sprintf(outbuf, "Writing %08x.h3", curContentCid);
            println(outbuf);
            t = fopen(outF, "wb");
            if (t == NULL) {
                println("Failed to create file");
                return;
            }
            uint32_t hashNum = (uint32_t)((tSize / 0x10000000ULL) + 1);
            fwrite(hashPos, 1, (0x14 * hashNum), t);
            fclose(t);
            hashPos += (0x14 * hashNum);
        }
    }
    free(fstDec);
    free(appBuf);
    free(tmdBuf);

    WHBLogPrint("Done!");

    if (apd_enabled) {
        if (IMEnableAPD() == 0)
            WHBLogPrint("Re-Enabled Auto Power-Down.");
    }
    WHBLogFreetypeDraw();
}

int main() {
    VPADInit();
    KPADInit();

    WHBProcInit();

    // Init screen
    WHBLogFreetypeInit();

    printhdr_noflip();
    WHBLogPrint("Please make sure to take out any currently inserted disc.");
    WHBLogPrint("Also make sure you have at least 23.3GB free on your device.");
    WHBLogPrint("");
    WHBLogPrint("Press A to continue with a FAT32 SD Card as destination.");
    WHBLogPrint("Press B to continue with a FAT32 USB Device as destination.");
    WHBLogPrint("");
    WHBLogPrint("Press HOME to return to the Homebrew Launcher.");

    WHBLogFreetypeDraw();

    // set everything to 0 because some vars will stay uninitialized on first read
    memset(&kpad, 0, sizeof(kpad));

    bool exitMainLoop = false;
    while (WHBProcIsRunning()) {
        VPADRead(0, &vpad, 1, &vpadError);
        if (vpadError == 0) {
            if (vpad.trigger & VPAD_BUTTON_A) {
                dump();
                break;
            }

            else if (vpad.trigger & VPAD_BUTTON_B) {
                usb = true;
                dump();
                break;
            }
        }

        for (int i = 0; i < 4; i++) {
            uint32_t controllerType;
            // check if the controller is connected
            if (WPADProbe(i, &controllerType) != 0)
                continue;

            KPADRead(i, &kpad, 1);

            switch (controllerType) {
                case WPAD_EXT_CORE:
                    if (kpad.trigger & WPAD_BUTTON_A) {
                        exitMainLoop = true;
                        dump();
                    }

                    else if (kpad.trigger & WPAD_BUTTON_B) {
                        usb = true;
                        exitMainLoop = true;
                        dump();
                    }
                    break;
                case WPAD_EXT_CLASSIC:
                    if (kpad.classic.trigger & WPAD_CLASSIC_BUTTON_A) {
                        exitMainLoop = true;
                        dump();
                    }

                    else if (kpad.classic.trigger & WPAD_CLASSIC_BUTTON_B) {
                        usb = true;
                        exitMainLoop = true;
                        dump();
                    }
                    break;
                case WPAD_EXT_PRO_CONTROLLER:
                    if (kpad.pro.trigger & WPAD_PRO_BUTTON_A) {
                        exitMainLoop = true;
                        dump();
                    } else if (kpad.pro.trigger & WPAD_PRO_BUTTON_B) {
                        usb = true;
                        exitMainLoop = true;
                        dump();
                    }
                    break;
            }
        }

        if (exitMainLoop)
            break;

        usleep(50000);
    }

    if (fsaFd >= 0) {
        if (f != NULL)
            fclose(f);
        fatUnmount("sd");
        fatUnmount("usb");
        if (oddFd >= 0)
            IOSUHAX_FSA_RawClose(fsaFd, oddFd);
        IOSUHAX_FSA_Close(fsaFd);
    }
    //close out old mcp instance
    MCPHookClose();
    WHBLogFreetypeFree();
    WHBProcShutdown();
    return 1;
}
