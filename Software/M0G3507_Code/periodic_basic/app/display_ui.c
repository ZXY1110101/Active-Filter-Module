#include "display_ui.h"

#define WAVE_X 8U
#define WAVE_Y 8U
#define WAVE_W 208U
#define WAVE_H 144U

#define PANEL_X 224U
#define PANEL_Y 8U
#define PANEL_W 88U
#define PANEL_H 224U

static const uint8_t *font5x7(char c)
{
    static const uint8_t space[5] = {0, 0, 0, 0, 0};
    static const uint8_t zero[5]  = {0x3E, 0x51, 0x49, 0x45, 0x3E};
    static const uint8_t one[5]   = {0x00, 0x42, 0x7F, 0x40, 0x00};
    static const uint8_t two[5]   = {0x42, 0x61, 0x51, 0x49, 0x46};
    static const uint8_t three[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
    static const uint8_t four[5]  = {0x18, 0x14, 0x12, 0x7F, 0x10};
    static const uint8_t five[5]  = {0x27, 0x45, 0x45, 0x45, 0x39};
    static const uint8_t six[5]   = {0x3C, 0x4A, 0x49, 0x49, 0x30};
    static const uint8_t seven[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
    static const uint8_t eight[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t nine[5]  = {0x06, 0x49, 0x49, 0x29, 0x1E};
    static const uint8_t a[5]     = {0x7E, 0x11, 0x11, 0x11, 0x7E};
    static const uint8_t d[5]     = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t e[5]     = {0x7F, 0x49, 0x49, 0x49, 0x41};
    static const uint8_t f[5]     = {0x7F, 0x09, 0x09, 0x09, 0x01};
    static const uint8_t h[5]     = {0x7F, 0x08, 0x08, 0x08, 0x7F};
    static const uint8_t i[5]     = {0x00, 0x41, 0x7F, 0x41, 0x00};
    static const uint8_t k[5]     = {0x7F, 0x08, 0x14, 0x22, 0x41};
    static const uint8_t m[5]     = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    static const uint8_t n[5]     = {0x7F, 0x04, 0x08, 0x10, 0x7F};
    static const uint8_t p[5]     = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t q[5]     = {0x3E, 0x41, 0x51, 0x21, 0x5E};
    static const uint8_t r[5]     = {0x7F, 0x09, 0x19, 0x29, 0x46};
    static const uint8_t s[5]     = {0x46, 0x49, 0x49, 0x49, 0x31};
    static const uint8_t t[5]     = {0x01, 0x01, 0x7F, 0x01, 0x01};
    static const uint8_t u[5]     = {0x3F, 0x40, 0x40, 0x40, 0x3F};
    static const uint8_t v[5]     = {0x1F, 0x20, 0x40, 0x20, 0x1F};
    static const uint8_t x[5]     = {0x63, 0x14, 0x08, 0x14, 0x63};
    static const uint8_t y[5]     = {0x07, 0x08, 0x70, 0x08, 0x07};
    static const uint8_t z[5]     = {0x61, 0x51, 0x49, 0x45, 0x43};
    static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t dot[5]   = {0x00, 0x60, 0x60, 0x00, 0x00};
    static const uint8_t minus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t percent[5] = {0x23, 0x13, 0x08, 0x64, 0x62};

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    switch (c) {
    case '0': return zero;
    case '1': return one;
    case '2': return two;
    case '3': return three;
    case '4': return four;
    case '5': return five;
    case '6': return six;
    case '7': return seven;
    case '8': return eight;
    case '9': return nine;
    case 'A': return a;
    case 'D': return d;
    case 'E': return e;
    case 'F': return f;
    case 'H': return h;
    case 'I': return i;
    case 'K': return k;
    case 'M': return m;
    case 'N': return n;
    case 'P': return p;
    case 'Q': return q;
    case 'R': return r;
    case 'S': return s;
    case 'T': return t;
    case 'U': return u;
    case 'V': return v;
    case 'X': return x;
    case 'Y': return y;
    case 'Z': return z;
    case ':': return colon;
    case '.': return dot;
    case '-': return minus;
    case '%': return percent;
    default: return space;
    }
}

static void append_char(char **p, char *end, char c)
{
    if (*p < end) {
        **p = c;
        (*p)++;
    }
}

static void append_uint(char **p, char *end, uint32_t value)
{
    char tmp[10];
    uint8_t n = 0U;

    do {
        tmp[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && n < sizeof(tmp));

    while (n != 0U) {
        append_char(p, end, tmp[--n]);
    }
}

static void append_fixed(char **p, char *end, int32_t value, uint8_t decimals)
{
    uint32_t scale = 1U;
    uint8_t i;

    for (i = 0U; i < decimals; i++) {
        scale *= 10U;
    }

    if (value < 0) {
        append_char(p, end, '-');
        value = -value;
    }

    append_uint(p, end, (uint32_t)value / scale);
    if (decimals != 0U) {
        uint32_t frac = (uint32_t)value % scale;
        uint32_t div = scale / 10U;
        append_char(p, end, '.');
        while (div != 0U) {
            append_char(p, end, (char)('0' + (frac / div) % 10U));
            div /= 10U;
        }
    }
}

static void pad_value_text(char *buf, size_t len, size_t width)
{
    size_t i = 0U;

    while (i < (len - 1U) && buf[i] != '\0') {
        i++;
    }

    while (i < width && i < (len - 1U)) {
        buf[i++] = ' ';
    }
    buf[i] = '\0';
}

static void make_voltage(char *buf, size_t len, int32_t mv)
{
    char *p = buf;
    char *end = buf + len - 1U;

    if (mv <= -1000 || mv >= 1000) {
        append_fixed(&p, end, mv, 3U);
        append_char(&p, end, 'V');
    } else {
        append_fixed(&p, end, mv, 0U);
        append_char(&p, end, 'm');
        append_char(&p, end, 'V');
    }
    *p = '\0';
}

static void make_frequency(char *buf, size_t len, uint32_t cHz)
{
    char *p = buf;
    char *end = buf + len - 1U;

    if (cHz >= 100000000U) {
        append_fixed(&p, end, (int32_t)(cHz / 10000U), 2U);
        append_char(&p, end, 'M');
        append_char(&p, end, 'H');
        append_char(&p, end, 'Z');
    } else if (cHz >= 100000U) {
        append_fixed(&p, end, (int32_t)(cHz / 10U), 2U);
        append_char(&p, end, 'K');
        append_char(&p, end, 'H');
        append_char(&p, end, 'Z');
    } else {
        append_fixed(&p, end, (int32_t)cHz, 2U);
        append_char(&p, end, 'H');
        append_char(&p, end, 'Z');
    }
    *p = '\0';
}

void DisplayUI_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        uint16_t color)
{
    if ((w == 0U) || (h == 0U) || (x >= DISPLAY_UI_WIDTH) ||
        (y >= DISPLAY_UI_HEIGHT)) {
        return;
    }
    if ((uint32_t)x + w > DISPLAY_UI_WIDTH) {
        w = DISPLAY_UI_WIDTH - x;
    }
    if ((uint32_t)y + h > DISPLAY_UI_HEIGHT) {
        h = DISPLAY_UI_HEIGHT - y;
    }
    LCD_Fill(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), color);
}

void DisplayUI_Clear(uint16_t color)
{
    LCD_Clear(color);
}

void DisplayUI_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        uint16_t color)
{
    DisplayUI_DrawLine(x, y, (int)x + (int)w - 1, y, color);
    DisplayUI_DrawLine(x, y, x, (int)y + (int)h - 1, color);
    DisplayUI_DrawLine((int)x + (int)w - 1, y,
                       (int)x + (int)w - 1, (int)y + (int)h - 1, color);
    DisplayUI_DrawLine(x, (int)y + (int)h - 1,
                       (int)x + (int)w - 1, (int)y + (int)h - 1, color);
}

void DisplayUI_DrawLineBresenham(int x0, int y0, int x1, int y1,
                                 uint16_t color)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        if (x0 >= 0 && y0 >= 0 && x0 < (int)DISPLAY_UI_WIDTH &&
            y0 < (int)DISPLAY_UI_HEIGHT) {
            DISPLAY_UI_DRAW_PIXEL((uint16_t)x0, (uint16_t)y0, color);
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        {
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

void DisplayUI_DrawLine(int x0, int y0, int x1, int y1, uint16_t color)
{
    DisplayUI_DrawLineBresenham(x0, y0, x1, y1, color);
}

void DisplayUI_DrawText(uint16_t x, uint16_t y, const char *text,
                        uint16_t color, uint16_t bg, uint8_t scale)
{
    uint16_t cursor_x = x;
    uint8_t col;
    uint8_t row;
    uint8_t sx;
    uint8_t sy;

    if (scale == 0U) {
        scale = 1U;
    }

    while (*text != '\0') {
        const uint8_t *glyph = font5x7(*text++);
        for (col = 0U; col < 6U; col++) {
            uint8_t bits = (col < 5U) ? glyph[col] : 0U;
            for (row = 0U; row < 8U; row++) {
                uint16_t c = ((bits >> row) & 0x01U) ? color : bg;
                for (sy = 0U; sy < scale; sy++) {
                    for (sx = 0U; sx < scale; sx++) {
                        uint16_t px = cursor_x + (uint16_t)(col * scale) + sx;
                        uint16_t py = y + (uint16_t)(row * scale) + sy;
                        if (px < DISPLAY_UI_WIDTH && py < DISPLAY_UI_HEIGHT) {
                            DISPLAY_UI_DRAW_PIXEL(px, py, c);
                        }
                    }
                }
            }
        }
        cursor_x += (uint16_t)(6U * scale);
    }
}

static void draw_wave_grid(void)
{
    uint16_t i;

    for (i = 1U; i < 4U; i++) {
        uint16_t gx = WAVE_X + (uint16_t)((WAVE_W * i) / 4U);
        uint16_t gy = WAVE_Y + (uint16_t)((WAVE_H * i) / 4U);
        DisplayUI_DrawLineBresenham(gx, WAVE_Y + 1U, gx,
                                    WAVE_Y + WAVE_H - 2U, DISPLAY_UI_DARK);
        DisplayUI_DrawLineBresenham(WAVE_X + 1U, gy,
                                    WAVE_X + WAVE_W - 2U, gy,
                                    DISPLAY_UI_DARK);
    }
}

void DisplayUI_DrawStaticFrame(void)
{
    DisplayUI_Clear(DISPLAY_UI_BLACK);
    DisplayUI_DrawRect(WAVE_X, WAVE_Y, WAVE_W, WAVE_H, DISPLAY_UI_GRAY);
    draw_wave_grid();
    DisplayUI_DrawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, DISPLAY_UI_GRAY);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 8U, "MEASURE",
                       DISPLAY_UI_CYAN, DISPLAY_UI_BLACK, 1U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 32U, "VPP",
                       DISPLAY_UI_GRAY, DISPLAY_UI_BLACK, 1U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 64U, "AMP",
                       DISPLAY_UI_GRAY, DISPLAY_UI_BLACK, 1U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 96U, "FREQ",
                       DISPLAY_UI_GRAY, DISPLAY_UI_BLACK, 1U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 128U, "DUTY",
                       DISPLAY_UI_GRAY, DISPLAY_UI_BLACK, 1U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 160U, "VMAX",
                       DISPLAY_UI_GRAY, DISPLAY_UI_BLACK, 1U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 192U, "VMIN",
                       DISPLAY_UI_GRAY, DISPLAY_UI_BLACK, 1U);
}

void DisplayUI_UpdateMeasurements(const DisplayUI_Measurements *m)
{
    char buf[18];
    char *p;
    char *end;

    if (m == NULL) {
        return;
    }
    make_voltage(buf, sizeof(buf), m->vpp_mv);
    pad_value_text(buf, sizeof(buf), 12U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 44U, buf,
                       DISPLAY_UI_YELLOW, DISPLAY_UI_BLACK, 1U);
    make_voltage(buf, sizeof(buf), m->amplitude_mv);
    pad_value_text(buf, sizeof(buf), 12U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 76U, buf,
                       DISPLAY_UI_MAGENTA, DISPLAY_UI_BLACK, 1U);
    make_frequency(buf, sizeof(buf), m->freq_cHz);
    pad_value_text(buf, sizeof(buf), 12U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 108U, buf,
                       DISPLAY_UI_GREEN, DISPLAY_UI_BLACK, 1U);
    p = buf;
    end = buf + sizeof(buf) - 1U;
    append_fixed(&p, end, m->duty_x100, 2U);
    append_char(&p, end, '%');
    *p = '\0';
    pad_value_text(buf, sizeof(buf), 12U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 140U, buf,
                       DISPLAY_UI_ORANGE, DISPLAY_UI_BLACK, 1U);
    make_voltage(buf, sizeof(buf), m->vmax_mv);
    pad_value_text(buf, sizeof(buf), 12U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 172U, buf,
                       DISPLAY_UI_WHITE, DISPLAY_UI_BLACK, 1U);
    make_voltage(buf, sizeof(buf), m->vmin_mv);
    pad_value_text(buf, sizeof(buf), 12U);
    DisplayUI_DrawText(PANEL_X + 6U, PANEL_Y + 204U, buf,
                       DISPLAY_UI_WHITE, DISPLAY_UI_BLACK, 1U);
}

static uint16_t map_sample_y(int16_t sample, int16_t min_value,
                             int16_t max_value)
{
    int32_t range = (int32_t)max_value - (int32_t)min_value;
    int32_t v;

    if (range <= 0) {
        return WAVE_Y + (WAVE_H / 2U);
    }

    v = ((int32_t)sample - (int32_t)min_value) *
        (int32_t)(WAVE_H - 1U);
    v = (int32_t)(WAVE_H - 1U) - (v / range);

    if (v < 0) {
        v = 0;
    } else if (v >= (int32_t)WAVE_H) {
        v = WAVE_H - 1U;
    }

    return (uint16_t)(WAVE_Y + v);
}

void DisplayUI_DrawWaveformI16(const int16_t *samples,
                               size_t count,
                               int16_t min_value,
                               int16_t max_value)
{
    static uint16_t last_y[WAVE_W - 2U];
    static uint16_t curr_y[WAVE_W - 2U];
    static uint8_t has_last_wave = 0U;
    uint16_t x;

    if (samples == NULL || count == 0U) {
        return;
    }

    for (x = 0U; x < (WAVE_W - 2U); x++) {
        size_t idx = ((size_t)x * count) / (WAVE_W - 2U);
        if (idx >= count) {
            idx = count - 1U;
        }
        curr_y[x] = map_sample_y(samples[idx], min_value, max_value);
    }

    if (has_last_wave != 0U) {
        for (x = 1U; x < (WAVE_W - 2U); x++) {
            DisplayUI_DrawLineBresenham(WAVE_X + x, last_y[x - 1U],
                                        WAVE_X + 1U + x, last_y[x],
                                        DISPLAY_UI_BLACK);
        }
    }

    for (x = 1U; x < (WAVE_W - 2U); x++) {
        DisplayUI_DrawLineBresenham(WAVE_X + x, curr_y[x - 1U],
                                    WAVE_X + 1U + x, curr_y[x],
                                    DISPLAY_UI_CYAN);
    }

    for (x = 0U; x < (WAVE_W - 2U); x++) {
        last_y[x] = curr_y[x];
    }
    has_last_wave = 1U;
}
