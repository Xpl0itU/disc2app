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

void clearBuffers();

void clearBuffersEx();

void fillScreen(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void drawPixel32(int x, int y, RGBAColor color);

void drawLine(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void drawRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void drawFillRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void drawCircle(int xCen, int yCen, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void drawFillCircle(int xCen, int yCen, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void drawCircleCircum(int cx, int cy, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void drawPic(int x, int y, uint32_t w, uint32_t h, float scale, uint32_t *pixels);

void drawBackgroundDRC(uint32_t w, uint32_t h, uint8_t *out);

void drawBackgroundTV(uint32_t w, uint32_t h, uint8_t *out);

int ttfPrintString(int x, int y, char *string, bool wWrap, bool ceroX);

int ttfStringWidth(char *string, int8_t part);

void draw_bitmap(FT_Bitmap *bitmap, FT_Int x, FT_Int y);

#ifdef __cplusplus
}
#endif