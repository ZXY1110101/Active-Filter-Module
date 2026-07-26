#include "app_ui.h"

#include "lcd.h"
#include "main.h"
#include "signal_analyzer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UI_BG              0x0000U
#define UI_PLOT_BG         0x0000U
#define UI_HEADER_BG       0x18E3U
#define UI_FOOTER_BG       0x18E3U
#define UI_GRID            0x3186U
#define UI_BORDER          0xFFFFU
#define UI_WAVE            0xFFE0U
#define UI_SPECTRUM        0x07FFU
#define UI_ACCENT          0xF800U
#define UI_TEXT            0xFFFFU
#define UI_MUTED           0x7BEFU
#define UI_WARNING         0xFD20U
#define UI_MAX_PLOT_WIDTH  800U
#define UI_FRAME_PERIOD_MS 33U
#define UI_TEXT_PERIOD_MS  1000U

typedef struct
{
    uint16_t wave_x;
    uint16_t wave_y;
    uint16_t wave_w;
    uint16_t wave_h;
    uint16_t fft_x;
    uint16_t fft_y;
    uint16_t fft_w;
    uint16_t fft_h;
    uint16_t footer_y;
    uint16_t footer_h;
    uint16_t spectrum_bars;
} ui_layout_t;

static ui_layout_t s_layout;
static uint16_t s_old_wave_min[UI_MAX_PLOT_WIDTH];
static uint16_t s_old_wave_max[UI_MAX_PLOT_WIDTH];
static uint16_t s_old_wave_mid[UI_MAX_PLOT_WIDTH];
static uint16_t s_old_spectrum_top[SIGNAL_SPECTRUM_BARS];
static uint8_t s_wave_valid;
static uint8_t s_spectrum_valid;
static uint32_t s_last_sequence;
static uint32_t s_last_frame_ms;
static uint32_t s_last_text_ms;
static uint32_t s_last_led_ms;

static uint16_t ui_clamp_u16(uint16_t value, uint16_t minimum, uint16_t maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static void ui_text(uint16_t x, uint16_t y, uint16_t width,
                    uint8_t size, const char *text, uint16_t color)
{
    lcd_show_string(x, y, width, size, size, (char *)text, color);
}

static uint8_t ui_is_grid_coordinate(uint16_t coordinate, uint16_t origin,
                                     uint16_t span, uint16_t divisions)
{
    uint16_t division;

    for (division = 1U; division < divisions; ++division)
    {
        if (coordinate == (uint16_t)(origin +
            ((uint32_t)division * span) / divisions))
        {
            return 1U;
        }
    }
    return 0U;
}

static uint16_t ui_plot_background(uint16_t x, uint16_t y,
                                   uint16_t origin_x, uint16_t origin_y,
                                   uint16_t width, uint16_t height,
                                   uint16_t x_divisions, uint16_t y_divisions)
{
    if ((x == origin_x) || (y == origin_y) ||
        (x == (uint16_t)(origin_x + width - 1U)) ||
        (y == (uint16_t)(origin_y + height - 1U)))
    {
        return UI_BORDER;
    }
    if ((ui_is_grid_coordinate(x, origin_x, width, x_divisions) != 0U) ||
        (ui_is_grid_coordinate(y, origin_y, height, y_divisions) != 0U))
    {
        return UI_GRID;
    }
    return UI_PLOT_BG;
}

static uint16_t ui_wave_background(uint16_t x, uint16_t y)
{
    return ui_plot_background(x, y, s_layout.wave_x, s_layout.wave_y,
                              s_layout.wave_w, s_layout.wave_h, 10U, 8U);
}

static uint16_t ui_fft_background(uint16_t x, uint16_t y)
{
    return ui_plot_background(x, y, s_layout.fft_x, s_layout.fft_y,
                              s_layout.fft_w, s_layout.fft_h, 8U, 4U);
}

static void ui_wave_line(int x0, int y0, int x1, int y1, uint8_t restore)
{
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int error = dx + dy;

    while (1)
    {
        uint16_t x = (uint16_t)x0;
        uint16_t y = (uint16_t)y0;
        uint16_t color = (restore != 0U) ? ui_wave_background(x, y) : UI_WAVE;

        lcd_draw_point(x, y, color);
        if ((x0 == x1) && (y0 == y1))
        {
            break;
        }

        {
            int twice_error = 2 * error;
            if (twice_error >= dy)
            {
                error += dy;
                x0 += sx;
            }
            if (twice_error <= dx)
            {
                error += dx;
                y0 += sy;
            }
        }
    }
}

static void ui_restore_wave_vertical(uint16_t x, uint16_t top, uint16_t bottom)
{
    uint16_t y;

    for (y = top; y <= bottom; ++y)
    {
        lcd_draw_point(x, y, ui_wave_background(x, y));
    }
}
static uint16_t ui_map_wave_y(int16_t value, uint16_t scale)
{
    int32_t center = (int32_t)s_layout.wave_y + (int32_t)s_layout.wave_h / 2;
    int32_t half_height = (int32_t)s_layout.wave_h / 2 - 2;
    int32_t y;

    if (scale == 0U)
    {
        scale = 1U;
    }

    y = center - ((int32_t)value * half_height) / (int32_t)scale;
    if (y < (int32_t)s_layout.wave_y + 1)
    {
        y = (int32_t)s_layout.wave_y + 1;
    }
    if (y > (int32_t)(s_layout.wave_y + s_layout.wave_h - 2U))
    {
        y = (int32_t)(s_layout.wave_y + s_layout.wave_h - 2U);
    }

    return (uint16_t)y;
}

static void ui_draw_static_grid(uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height,
                                uint16_t x_divisions, uint16_t y_divisions)
{
    uint16_t division;

    lcd_fill(x, y, (uint16_t)(x + width - 1U),
             (uint16_t)(y + height - 1U), UI_PLOT_BG);
    lcd_draw_rectangle(x, y, (uint16_t)(x + width - 1U),
                       (uint16_t)(y + height - 1U), UI_BORDER);

    for (division = 1U; division < x_divisions; ++division)
    {
        uint16_t line_x = (uint16_t)(x + (division * width) / x_divisions);
        lcd_draw_line(line_x, y, line_x,
                      (uint16_t)(y + height - 1U), UI_GRID);
    }

    for (division = 1U; division < y_divisions; ++division)
    {
        uint16_t line_y = (uint16_t)(y + (division * height) / y_divisions);
        lcd_draw_line(x, line_y, (uint16_t)(x + width - 1U),
                      line_y, UI_GRID);
    }
}

static void ui_draw_waveform(const signal_analysis_t *result)
{
    uint16_t x;

    if (s_wave_valid != 0U)
    {
        for (x = 0U; x < s_layout.wave_w; ++x)
        {
            uint16_t screen_x = (uint16_t)(s_layout.wave_x + x);
            ui_restore_wave_vertical(screen_x,
                                     s_old_wave_min[x],
                                     s_old_wave_max[x]);
        }

        for (x = 1U; x < s_layout.wave_w; ++x)
        {
            ui_wave_line((int)(s_layout.wave_x + x - 1U),
                                 (int)s_old_wave_mid[x - 1U],
                                 (int)(s_layout.wave_x + x),
                                 (int)s_old_wave_mid[x], 1U);
        }
    }

    for (x = 0U; x < s_layout.wave_w; ++x)
    {
        uint32_t source_index =
            ((uint32_t)x * SIGNAL_WAVE_POINTS) / s_layout.wave_w;
        uint16_t screen_x = (uint16_t)(s_layout.wave_x + x);
        uint16_t y_a = ui_map_wave_y(result->wave_min[source_index],
                                     result->wave_scale_counts);
        uint16_t y_b = ui_map_wave_y(result->wave_max[source_index],
                                     result->wave_scale_counts);
        uint16_t top = (y_a < y_b) ? y_a : y_b;
        uint16_t bottom = (y_a > y_b) ? y_a : y_b;

        s_old_wave_min[x] = top;
        s_old_wave_max[x] = bottom;
        s_old_wave_mid[x] = (uint16_t)((top + bottom) / 2U);
        lcd_draw_line(screen_x, top, screen_x, bottom, UI_WAVE);
    }

    for (x = 1U; x < s_layout.wave_w; ++x)
    {
        ui_wave_line((int)(s_layout.wave_x + x - 1U),
                     (int)s_old_wave_mid[x - 1U],
                     (int)(s_layout.wave_x + x),
                     (int)s_old_wave_mid[x], 0U);
    }

    s_wave_valid = 1U;
}

static void ui_restore_fft_area(uint16_t x0, uint16_t x1,
                                uint16_t y0, uint16_t y1)
{
    uint16_t x;
    uint16_t y;

    for (x = x0; x <= x1; ++x)
    {
        for (y = y0; y <= y1; ++y)
        {
            lcd_draw_point(x, y, ui_fft_background(x, y));
        }
    }
}

static void ui_draw_spectrum(const signal_analysis_t *result)
{
    uint16_t bar;
    uint16_t bottom = (uint16_t)(s_layout.fft_y + s_layout.fft_h - 2U);

    for (bar = 0U; bar < s_layout.spectrum_bars; ++bar)
    {
        uint32_t source_index =
            ((uint32_t)bar * SIGNAL_SPECTRUM_BARS) / s_layout.spectrum_bars;
        uint16_t x0 = (uint16_t)(s_layout.fft_x +
            ((uint32_t)bar * s_layout.fft_w) / s_layout.spectrum_bars);
        uint16_t x1 = (uint16_t)(s_layout.fft_x +
            ((uint32_t)(bar + 1U) * s_layout.fft_w) /
            s_layout.spectrum_bars - 1U);
        uint16_t height = (uint16_t)(((uint32_t)result->spectrum[source_index] *
            (s_layout.fft_h - 3U)) / 1000U);
        uint16_t new_top = (uint16_t)(bottom - height);
        uint16_t old_top = s_old_spectrum_top[bar];

        if (x1 > x0)
        {
            x1--;
        }

        if (s_spectrum_valid == 0U)
        {
            old_top = bottom;
        }

        if (new_top < old_top)
        {
            lcd_fill(x0, new_top, x1, (uint16_t)(old_top - 1U), UI_SPECTRUM);
        }
        else if (new_top > old_top)
        {
            ui_restore_fft_area(x0, x1, old_top,
                                (uint16_t)(new_top - 1U));
        }

        s_old_spectrum_top[bar] = new_top;
    }

    s_spectrum_valid = 1U;
}

static void ui_format_frequency(char *buffer, size_t size, float frequency)
{
    if (frequency >= 1000000.0f)
    {
        (void)snprintf(buffer, size, "%.3fM", frequency / 1000000.0f);
    }
    else if (frequency >= 1000.0f)
    {
        (void)snprintf(buffer, size, "%.2fk", frequency / 1000.0f);
    }
    else
    {
        (void)snprintf(buffer, size, "%.1f", frequency);
    }
}

static void ui_update_header(void)
{
    const signal_analysis_t *result =
        signal_analyzer_result();

    char sample_rate[16];
    char line[64];

    ui_format_frequency(sample_rate,
                        sizeof(sample_rate),
                        result->sample_rate_hz);

    lcd_fill(0U, 0U,
             (uint16_t)(lcddev.width - 1U),
             23U,
             UI_HEADER_BG);

    lcd_fill(0U, 22U,
             (uint16_t)(lcddev.width - 1U),
             23U,
             UI_ACCENT);

    g_back_color = UI_HEADER_BG;

    (void)snprintf(
        line,
        sizeof(line),
        "RUN  Fs:%sS/s  FFT:%u",
        sample_rate,
        (unsigned int)SIGNAL_FFT_SIZE);

    ui_text(6U,
            4U,
            (uint16_t)(lcddev.width - 12U),
            16U,
            line,
            UI_TEXT);
}

static void ui_update_measurements(const signal_analysis_t *result)
{
    char capture_frequency[16];
    char fft_frequency[16];
    char line[96];
    uint16_t adc_color = UI_MUTED;

    ui_format_frequency(capture_frequency, sizeof(capture_frequency),
                        result->capture_frequency_hz);
    ui_format_frequency(fft_frequency, sizeof(fft_frequency),
                        result->fft_peak_frequency_hz);

    lcd_fill(0U, s_layout.footer_y,
             (uint16_t)(lcddev.width - 1U),
             (uint16_t)(lcddev.height - 1U), UI_FOOTER_BG);
    g_back_color = UI_FOOTER_BG;

    (void)snprintf(line, sizeof(line),
                   "F:%sHz D:%4.1f%% FFT:%sHz A:%.3f VPP(F):%.3fV",
                   capture_frequency, result->capture_duty_percent,
                   fft_frequency, result->fft_peak_amplitude_v,
                   result->peak_to_peak_v);
    ui_text(5U, (uint16_t)(s_layout.footer_y + 4U),
            (uint16_t)(lcddev.width - 10U), 12U, line, UI_TEXT);

    if ((result->peak_to_peak_v < 0.010f) &&
        ((result->dc_voltage_v < 0.030f) ||
         (result->dc_voltage_v > 3.270f)))
    {
        adc_color = UI_WARNING;
    }

    (void)snprintf(line, sizeof(line),
                   "DC:%.3f MIN:%.3f MAX:%.3f RMS:%.3fV",
                   result->dc_voltage_v, result->minimum_voltage_v,
                   result->maximum_voltage_v, result->rms_ac_voltage_v);
    ui_text(5U, (uint16_t)(s_layout.footer_y + 21U),
            (uint16_t)(lcddev.width - 10U), 12U, line, adc_color);
}

void app_ui_init(void)
{
    uint16_t fft_height;

    lcd_display_dir(1U);
    lcd_clear(UI_BG);

    s_layout.footer_h = (lcddev.height >= 200U) ? 40U : 32U;
    fft_height = ui_clamp_u16((uint16_t)(lcddev.height / 4U), 44U, 100U);

    s_layout.wave_x = 4U;
    s_layout.wave_y = 27U;
    s_layout.wave_w = ui_clamp_u16((uint16_t)(lcddev.width - 8U),
                                   1U, UI_MAX_PLOT_WIDTH);
    s_layout.footer_y = (uint16_t)(lcddev.height - s_layout.footer_h);
    s_layout.footer_h = (uint16_t)(lcddev.height - s_layout.footer_y);

    s_layout.fft_x = 4U;
    s_layout.fft_w = s_layout.wave_w;
    s_layout.fft_h = fft_height;
    s_layout.fft_y = (uint16_t)(s_layout.footer_y - fft_height - 2U);
    s_layout.wave_h = (uint16_t)(s_layout.fft_y - s_layout.wave_y - 2U);
    s_layout.spectrum_bars =
        (uint16_t)(s_layout.fft_w / 2U);
    if (s_layout.spectrum_bars > SIGNAL_SPECTRUM_BARS)
    {
        s_layout.spectrum_bars = SIGNAL_SPECTRUM_BARS;
    }
    if (s_layout.spectrum_bars == 0U)
    {
        s_layout.spectrum_bars = 1U;
    }

    ui_update_header();
    ui_draw_static_grid(s_layout.wave_x, s_layout.wave_y,
                        s_layout.wave_w, s_layout.wave_h, 10U, 8U);
    ui_draw_static_grid(s_layout.fft_x, s_layout.fft_y,
                        s_layout.fft_w, s_layout.fft_h, 8U, 4U);
    ui_update_measurements(signal_analyzer_result());

    memset(s_old_spectrum_top, 0, sizeof(s_old_spectrum_top));
    s_wave_valid = 0U;
    s_spectrum_valid = 0U;
}

void app_ui_process(uint32_t now_ms)
{
    const signal_analysis_t *result =
        signal_analyzer_result();

    /*
     * 运行指示灯每500ms翻转一次。
     */
    if ((now_ms - s_last_led_ms) >= 500U)
    {
        s_last_led_ms = now_ms;

        HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
    }

    /*
     * 有新分析帧时刷新波形和频谱，
     * 最高刷新速度由UI_FRAME_PERIOD_MS限制。
     */
    if ((result->sequence != s_last_sequence) &&
        ((now_ms - s_last_frame_ms) >=
         UI_FRAME_PERIOD_MS))
    {
        s_last_frame_ms = now_ms;
        s_last_sequence = result->sequence;

        ui_draw_waveform(result);
        ui_draw_spectrum(result);
    }

    /*
     * 每秒刷新实际采样率、频率、幅值等文字。
     */
    if ((now_ms - s_last_text_ms) >=
        UI_TEXT_PERIOD_MS)
    {
        s_last_text_ms = now_ms;

        ui_update_header();
        ui_update_measurements(result);
    }
}
