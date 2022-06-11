#include "draw.h"
#include "log_freetype.h"

size_t tvBufferSize = 0;
size_t drcBufferSize = 0;

void *tvBuffer;
void *drcBuffer;

void flipBuffers() {
    DCFlushRange(tvBuffer, tvBufferSize);
    DCFlushRange(drcBuffer, drcBufferSize);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void clearBuffersEx() {
    OSScreenClearBufferEx(SCREEN_TV, 0x00000000);
    OSScreenClearBufferEx(SCREEN_DRC, 0x00000000);
}