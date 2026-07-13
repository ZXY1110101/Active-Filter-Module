/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "ili9341_mspm0.h"
#include "display_ui.h"

#include <stdint.h>
#include <stddef.h>

#define SAMPLE_COUNT     256U
#define SAMPLE_RATE_HZ   20000U

static int16_t wave_buf[SAMPLE_COUNT];

static void App_DelayMs(uint32_t ms);
static void App_MakeDemoWave(void);
static void App_UpdateDemoMeasure(void);

int main(void)
{
    SYSCFG_DL_init();

    LCD_Init();
    LCD_Clear(LCD_BLACK);

    DisplayUI_DrawStaticFrame();

    while (1) {
        App_MakeDemoWave();
        App_UpdateDemoMeasure();

        App_DelayMs(100);
    }
}

static void App_DelayMs(uint32_t ms)
{
    while (ms-- != 0U) {
        DL_Common_delayCycles(CPUCLK_FREQ / 1000U);
    }
}

/*
 * 先用模拟波形验证 LCD 显示。
 * 后面你接 ADC/DMA 时，只需要把 wave_buf[i] 换成真实采样值。
 */
static void App_MakeDemoWave(void)
{
    uint16_t i;

    for (i = 0; i < SAMPLE_COUNT; i++) {
        uint16_t phase = i % 64U;

        if (phase < 32U) {
            wave_buf[i] = (int16_t)(phase * 100);
        } else {
            wave_buf[i] = (int16_t)((64U - phase) * 100);
        }
    }
}

static void App_UpdateDemoMeasure(void)
{
    DisplayUI_Measurements m;

    m.vpp_mv = 3200;
    m.vmax_mv = 3200;
    m.vmin_mv = 0;
    m.vavg_mv = 1600;

    /*
     * freq_cHz 单位是 0.01Hz。
     * 100000 = 1000.00Hz
     */
    m.freq_cHz = 100000;

    /*
     * duty_x100 单位是 0.01%。
     * 5000 = 50.00%
     */
    m.duty_x100 = 5000;

    DisplayUI_DrawWaveformI16(wave_buf, SAMPLE_COUNT, 0, 3300);
    DisplayUI_UpdateMeasurements(&m);
}