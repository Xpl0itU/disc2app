#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <coreinit/cache.h>
#include <coreinit/memfrmheap.h>
#include <coreinit/memheap.h>
#include <coreinit/memory.h>
#include <coreinit/screen.h>
#include <proc_ui/procui.h>

#include <whb/log.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialization functions
bool WHBLogFreetypeInit();
void WHBLogFreetypeFree();

// Logging
void WHBLogFreetypePrintf(const char *fmt, ...);
void WHBLogFreetypePrint(const char *line);
void WHBLogFreetypeDraw();
void WHBLogFreetypeClear();

// Menu
void WHBLogFreetypeStartScreen();
void WHBLogFreetypePrintfAtPosition(uint32_t position, const char *fmt, ...);
void WHBLogFreetypePrintAtPosition(uint32_t position, const char *line);
void WHBLogFreetypeScreenPrintBottom(const char *line);
void WHBLogFreetypeScreenPrintfBottom(const char *fmt, ...);
uint32_t WHBLogFreetypeScreenSize();
uint32_t WHBLogFreetypeGetScreenPosition();
void WHBLogFreetypeDrawScreen();

// Rendering options
void WHBLogFreetypeSetFontColor(uint32_t color);
void WHBLogFreetypeSetBackgroundColor(uint32_t color);
bool WHBLogFreetypeSetFontSize(uint8_t width, uint8_t height);

void println(const char *str, ...);

#ifdef __cplusplus
}
#endif