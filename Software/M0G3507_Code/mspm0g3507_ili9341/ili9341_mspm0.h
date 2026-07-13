#ifndef ILI9341_MSPM0_H_
#define ILI9341_MSPM0_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"

/*
 * SysConfig symbol mapping.
 * The defaults below assume:
 *   SPI instance name : LCD_SPI
 *   GPIO group name   : LCD_GPIO
 *   GPIO pin names    : CS, DC, RST, BL
 * If SysConfig generates different names, only change these eight macros.
 */
#ifndef ILI9341_SPI_INST
#define ILI9341_SPI_INST              (LCD_SPI_INST)
#endif

#ifndef ILI9341_CS_PORT
#define ILI9341_CS_PORT               (LCD_GPIO_PORT)
#define ILI9341_CS_PIN                (LCD_GPIO_CS_PIN)
#define ILI9341_DC_PORT               (LCD_GPIO_PORT)
#define ILI9341_DC_PIN                (LCD_GPIO_DC_PIN)
#define ILI9341_RST_PORT              (LCD_GPIO_PORT)
#define ILI9341_RST_PIN               (LCD_GPIO_RST_PIN)
#define ILI9341_BL_PORT               (LCD_GPIO_PORT)
#define ILI9341_BL_PIN                (LCD_GPIO_BL_PIN)
#endif

#define ILI9341_WIDTH                 (320U)
#define ILI9341_HEIGHT                (240U)

#define LCD_WHITE                     (0xFFFFU)
#define LCD_BLACK                     (0x0000U)
#define LCD_BLUE                      (0x001FU)
#define LCD_RED                       (0xF800U)
#define LCD_GREEN                     (0x07E0U)
#define LCD_CYAN                      (0x07FFU)
#define LCD_MAGENTA                   (0xF81FU)
#define LCD_YELLOW                    (0xFFE0U)

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t direction;
} LCD_Device;

extern LCD_Device lcddev;
extern uint16_t POINT_COLOR;
extern uint16_t BACK_COLOR;

void LCD_Init(void);
void LCD_DisplayOn(void);
void LCD_DisplayOff(void);
void LCD_Clear(uint16_t color);
void LCD_SetWindows(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_SetCursor(uint16_t x, uint16_t y);
void LCD_DrawPoint(uint16_t x, uint16_t y);
void LCD_DrawPointColor(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawPoint_16Bit(uint16_t color);
void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
              uint16_t color);
void LCD_WritePixels(const uint16_t *pixels, size_t count);

#endif
