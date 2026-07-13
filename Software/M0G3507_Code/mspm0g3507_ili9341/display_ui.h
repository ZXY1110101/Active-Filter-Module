#ifndef DISPLAY_UI_H_
#define DISPLAY_UI_H_

#include <stdint.h>
#include <stddef.h>
#include "ili9341_mspm0.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DISPLAY_UI_WIDTH
#define DISPLAY_UI_WIDTH  320U
#endif

#ifndef DISPLAY_UI_HEIGHT
#define DISPLAY_UI_HEIGHT 240U
#endif

/*
 * Porting hook:
 * If your LCD driver uses a different pixel function name, override this macro
 * before including display_ui.h, or edit it here.
 */
#ifndef DISPLAY_UI_DRAW_PIXEL
#define DISPLAY_UI_DRAW_PIXEL(x, y, color) LCD_DrawPointColor((x), (y), (color))
#endif

#define DISPLAY_UI_BLACK   0x0000U
#define DISPLAY_UI_WHITE   0xFFFFU
#define DISPLAY_UI_RED     0xF800U
#define DISPLAY_UI_GREEN   0x07E0U
#define DISPLAY_UI_BLUE    0x001FU
#define DISPLAY_UI_CYAN    0x07FFU
#define DISPLAY_UI_MAGENTA 0xF81FU
#define DISPLAY_UI_YELLOW  0xFFE0U
#define DISPLAY_UI_GRAY    0x8410U
#define DISPLAY_UI_DARK    0x1082U
#define DISPLAY_UI_ORANGE  0xFD20U

typedef struct {
    int32_t vpp_mv;
    int32_t vmax_mv;
    int32_t vmin_mv;
    int32_t vavg_mv;
    uint32_t freq_cHz;
    uint16_t duty_x100;
} DisplayUI_Measurements;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} DisplayUI_Rect;

void DisplayUI_Clear(uint16_t color);
void DisplayUI_DrawStaticFrame(void);
void DisplayUI_UpdateMeasurements(const DisplayUI_Measurements *m);
void DisplayUI_DrawWaveformI16(const int16_t *samples,
                               size_t count,
                               int16_t min_value,
                               int16_t max_value);

void DisplayUI_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void DisplayUI_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void DisplayUI_DrawLineBresenham(int x0, int y0, int x1, int y1, uint16_t color);
void DisplayUI_DrawLine(int x0, int y0, int x1, int y1, uint16_t color);
void DisplayUI_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t bg, uint8_t scale);

#ifdef __cplusplus
}
#endif

#endif
