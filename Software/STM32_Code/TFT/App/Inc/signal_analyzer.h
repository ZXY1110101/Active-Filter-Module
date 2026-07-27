#ifndef SIGNAL_ANALYZER_H
#define SIGNAL_ANALYZER_H

#include <stdbool.h>
#include <stdint.h>

#define SIGNAL_FFT_SIZE          1024U
#define SIGNAL_WAVE_POINTS       320U
#define SIGNAL_SPECTRUM_BARS     128U
#define SIGNAL_SAMPLE_RATE_HZ    1000000.0f

typedef struct
{
    uint32_t sequence;
    uint32_t dropped_frames;
    float sample_rate_hz;
    float dc_voltage_v;
    float minimum_voltage_v;
    float maximum_voltage_v;
    float peak_to_peak_v;
    float rms_ac_voltage_v;
    float fft_peak_frequency_hz;
    float fft_peak_amplitude_v;
    float capture_frequency_hz;
    float capture_duty_percent;
    uint16_t wave_scale_counts;
    int16_t wave_min[SIGNAL_WAVE_POINTS];
    int16_t wave_max[SIGNAL_WAVE_POINTS];
    uint16_t spectrum[SIGNAL_SPECTRUM_BARS];
} signal_analysis_t;

/*
 * PA1输入边沿中断调用，用于等精度测频闸门边界。
 */
void signal_analyzer_frequency_edge_isr(void);

bool signal_analyzer_init(void);
bool signal_analyzer_start(void);
void signal_analyzer_process(void);
const signal_analysis_t *signal_analyzer_result(void);

#endif
