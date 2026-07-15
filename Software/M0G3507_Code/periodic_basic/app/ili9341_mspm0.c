#include "ili9341_mspm0.h"

LCD_Device lcddev = {ILI9341_WIDTH, ILI9341_HEIGHT, 1U};
uint16_t POINT_COLOR = LCD_BLACK;
uint16_t BACK_COLOR = LCD_WHITE;

static inline void cs_low(void)
{
    DL_GPIO_clearPins(ILI9341_CS_PORT, ILI9341_CS_PIN);
}

static inline void cs_high(void)
{
    DL_GPIO_setPins(ILI9341_CS_PORT, ILI9341_CS_PIN);
}

static inline void dc_command(void)
{
    DL_GPIO_clearPins(ILI9341_DC_PORT, ILI9341_DC_PIN);
}

static inline void dc_data(void)
{
    DL_GPIO_setPins(ILI9341_DC_PORT, ILI9341_DC_PIN);
}

static void delay_ms(uint32_t ms)
{
    while (ms-- != 0U) {
        DL_Common_delayCycles(CPUCLK_FREQ / 1000U);
    }
}

static inline void spi_write8(uint8_t value)
{
    DL_SPI_transmitDataBlocking8(ILI9341_SPI_INST, value);
}

static void write_command(uint8_t command)
{
    cs_low();
    dc_command();
    spi_write8(command);
    cs_high();
}

static inline void write_data16_raw(uint16_t data)
{
    spi_write8((uint8_t)(data >> 8));
    spi_write8((uint8_t)data);
}

static void write_command_data(uint8_t command, const uint8_t *data,
                               size_t count)
{
    cs_low();
    dc_command();
    spi_write8(command);
    if (count != 0U) {
        dc_data();
        while (count-- != 0U) {
            spi_write8(*data++);
        }
    }
    cs_high();
}

static void hardware_reset(void)
{
    DL_GPIO_clearPins(ILI9341_RST_PORT, ILI9341_RST_PIN);
    delay_ms(20U);
    DL_GPIO_setPins(ILI9341_RST_PORT, ILI9341_RST_PIN);
    delay_ms(120U);
}

void LCD_Init(void)
{
    static const uint8_t cf[] = {0x00, 0xC1, 0x30};
    static const uint8_t ed[] = {0x64, 0x03, 0x12, 0x81};
    static const uint8_t e8[] = {0x85, 0x10, 0x7A};
    static const uint8_t cb[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    static const uint8_t ea[] = {0x00, 0x00};
    static const uint8_t c5[] = {0x30, 0x30};
    static const uint8_t b1[] = {0x00, 0x1A};
    static const uint8_t b6[] = {0x0A, 0xA2};
    static const uint8_t gamma_pos[] = {
        0x0F, 0x2A, 0x28, 0x08, 0x0E, 0x08, 0x54, 0xA9,
        0x43, 0x0A, 0x0F, 0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t gamma_neg[] = {
        0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2B, 0x56,
        0x3C, 0x05, 0x10, 0x0F, 0x3F, 0x3F, 0x0F
    };

    cs_high();
    DL_GPIO_clearPins(ILI9341_BL_PORT, ILI9341_BL_PIN);
    hardware_reset();

    write_command_data(0xCF, cf, sizeof(cf));
    write_command_data(0xED, ed, sizeof(ed));
    write_command_data(0xE8, e8, sizeof(e8));
    write_command_data(0xCB, cb, sizeof(cb));
    write_command_data(0xF7, (const uint8_t[]){0x20}, 1U);
    write_command_data(0xEA, ea, sizeof(ea));
    write_command_data(0xC0, (const uint8_t[]){0x1B}, 1U);
    write_command_data(0xC1, (const uint8_t[]){0x01}, 1U);
    write_command_data(0xC5, c5, sizeof(c5));
    write_command_data(0xC7, (const uint8_t[]){0xB7}, 1U);
    write_command_data(0x36, (const uint8_t[]){0x6C}, 1U);
    write_command_data(0x3A, (const uint8_t[]){0x55}, 1U);
    write_command_data(0xB1, b1, sizeof(b1));
    write_command_data(0xB6, b6, sizeof(b6));
    write_command_data(0xF2, (const uint8_t[]){0x00}, 1U);
    write_command_data(0x26, (const uint8_t[]){0x01}, 1U);
    write_command_data(0xE0, gamma_pos, sizeof(gamma_pos));
    write_command_data(0xE1, gamma_neg, sizeof(gamma_neg));

    write_command(0x11);
    delay_ms(120U);
    write_command(0x29);
    delay_ms(20U);
    DL_GPIO_setPins(ILI9341_BL_PORT, ILI9341_BL_PIN);
}

void LCD_DisplayOn(void)
{
    write_command(0x29);
    DL_GPIO_setPins(ILI9341_BL_PORT, ILI9341_BL_PIN);
}

void LCD_DisplayOff(void)
{
    DL_GPIO_clearPins(ILI9341_BL_PORT, ILI9341_BL_PIN);
    write_command(0x28);
}

void LCD_SetWindows(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t range[4];

    range[0] = (uint8_t)(x0 >> 8);
    range[1] = (uint8_t)x0;
    range[2] = (uint8_t)(x1 >> 8);
    range[3] = (uint8_t)x1;
    write_command_data(0x2A, range, sizeof(range));

    range[0] = (uint8_t)(y0 >> 8);
    range[1] = (uint8_t)y0;
    range[2] = (uint8_t)(y1 >> 8);
    range[3] = (uint8_t)y1;
    write_command_data(0x2B, range, sizeof(range));
    write_command(0x2C);
}

void LCD_SetCursor(uint16_t x, uint16_t y)
{
    LCD_SetWindows(x, y, x, y);
}

void LCD_WritePixels(const uint16_t *pixels, size_t count)
{
    if ((pixels == NULL) || (count == 0U)) {
        return;
    }
    cs_low();
    dc_data();
    while (count-- != 0U) {
        write_data16_raw(*pixels++);
    }
    cs_high();
}

void LCD_DrawPoint_16Bit(uint16_t color)
{
    cs_low();
    dc_data();
    write_data16_raw(color);
    cs_high();
}

void LCD_DrawPointColor(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetCursor(x, y);
    LCD_DrawPoint_16Bit(color);
}

void LCD_DrawPoint(uint16_t x, uint16_t y)
{
    LCD_DrawPointColor(x, y, POINT_COLOR);
}

void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
              uint16_t color)
{
    uint32_t count;

    if ((x0 > x1) || (y0 > y1) || (x1 >= lcddev.width) ||
        (y1 >= lcddev.height)) {
        return;
    }
    count = ((uint32_t)x1 - x0 + 1U) * ((uint32_t)y1 - y0 + 1U);
    LCD_SetWindows(x0, y0, x1, y1);
    cs_low();
    dc_data();
    while (count-- != 0U) {
        write_data16_raw(color);
    }
    cs_high();
}

void LCD_Clear(uint16_t color)
{
    LCD_Fill(0U, 0U, lcddev.width - 1U, lcddev.height - 1U, color);
}
