#pragma once

#include <coreinit/screen.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef union _RGBAColor {
    uint32_t c;
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };
} RGBAColor;

//Function declarations for my graphics library
void flipBuffers();

void clearBuffersEx();

#ifdef __cplusplus
}
#endif