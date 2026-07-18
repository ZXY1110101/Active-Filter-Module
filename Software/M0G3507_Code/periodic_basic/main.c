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

#include "ADC.h"
#include "DAC.h"
#include "PGA.h"
#include "UART.h"
#include "display_ui.h"
#include "frequency.h"
#include "ili9341_mspm0.h"

#define MEASUREMENT_FRAMES_PER_REPORT (25U)
#define MAIN_LOOP_DELAY_CYCLES        (1600000U)
#define ADC_FULL_SCALE_CODE           (4095U)
#define DAC_MIN_VALID_VPP_CODE        (32U)

static void App_Init(void);
static void App_RunMeasurementCycle(void);
static void App_PrintBanner(void);
static void App_PrintMeasurement(uint32_t frequencyHz, bool adcFrameValid);
static void App_UpdateDisplay(uint32_t frequencyHz);

static int16_t gDisplaySamples[ADC_SAMPLE_SIZE];

int main(void)
{
    App_Init();

    while (1) {
        App_RunMeasurementCycle();
    }
}

/* 初始化各业务模块，并启动默认的低频测量模式。 */
static void App_Init(void)
{
    SYSCFG_DL_init();

    UART_Init();
    PGA_Init();
    ADC_Init();
    DAC_Init();
    Frequency_Init();

    LCD_Init();
    LCD_Clear(LCD_BLACK);
    DisplayUI_DrawStaticFrame();

    App_PrintBanner();
    Frequency_StartAuto();
}

/* 执行一次完整的“按键处理 -> ADC 采样 -> 数据输出”任务。 */
static void App_RunMeasurementCycle(void)
{
    static uint32_t frameCount = 0U;
    bool adcFrameValid;

    Frequency_Task();
    PGA_KeyScan();

    adcFrameValid = ADC_CaptureFrame();
    if (adcFrameValid) {
        ADC_AnalyzeSamples();
    }

    /*
     * TLV3507 基准使用跨帧中值、死区和分级低通跟踪。
     * 削顶或 VPP 小于约 26mV 时冻结基准，避免把噪声当成有效波形。
     */
    DAC_TrackReference(
        adcResult.centerCode,
        adcFrameValid && (!adcResult.clipped) &&
            ((uint16_t)(adcResult.maxCode - adcResult.minCode) >=
             DAC_MIN_VALID_VPP_CODE));

    frameCount++;
    if (frameCount >= MEASUREMENT_FRAMES_PER_REPORT) {
        uint32_t frequencyHz = Frequency_GetHz();

        frameCount = 0U;

        App_PrintMeasurement(frequencyHz, adcFrameValid);
        App_UpdateDisplay(frequencyHz);
    }

    delay_cycles(MAIN_LOOP_DELAY_CYCLES);
}

static void App_PrintBanner(void)
{
    UART_SendString("\r\n====================\r\n");
    UART_SendString("MSPM0 Measurement System\r\n");
    UART_SendString("Start\r\n");
    UART_SendString("====================\r\n");
}

static void App_PrintMeasurement(uint32_t frequencyHz, bool adcFrameValid)
{
    uint32_t sampleIndex;
    uint32_t previousLevel =
        (DL_GPIO_readPins(GPIO_CAPTURE_0_C0_PORT,
                          GPIO_CAPTURE_0_C0_PIN) != 0U);
    uint32_t pb13Level = previousLevel;
    uint32_t rawEdges = 0U;

    for (sampleIndex = 0U; sampleIndex < 500000U; sampleIndex++) {
        pb13Level =
            (DL_GPIO_readPins(GPIO_CAPTURE_0_C0_PORT,
                              GPIO_CAPTURE_0_C0_PIN) != 0U);
        if (pb13Level != previousLevel) {
            rawEdges++;
            previousLevel = pb13Level;
        }
    }

    UART_SendString("\r\n-------- DATA --------\r\n");

    UART_SendString("GAIN: ");
    UART_SendFloat((float)PGA_GetGain());
    UART_SendString("\r\n");

    UART_SendString("MAX: ");
    UART_SendFloat(adcResult.maxV);
    UART_SendString(" V\r\n");

    UART_SendString("MIN: ");
    UART_SendFloat(adcResult.minV);
    UART_SendString(" V\r\n");

    UART_SendString("VPP: ");
    UART_SendFloat(adcResult.vppV);
    UART_SendString(" V\r\n");

    UART_SendString("AMP: ");
    UART_SendFloat(adcResult.amplitudeV);
    UART_SendString(" V\r\n");

    UART_SendString("RMS: ");
    UART_SendFloat(adcResult.rmsV);
    UART_SendString(" V\r\n");

    UART_SendString("TLV_REF: ");
    UART_SendFloat(
        ((float)DAC_GetCode() *
         (float)ADC12_0_ADCMEM_0_REF_VOLTAGE_V) /
        (float)ADC_FULL_SCALE_CODE);
    UART_SendString(" V\r\n");

    UART_SendString("FREQ: ");
    UART_SendFloat((float)frequencyHz);
    UART_SendString(" Hz\r\n");

    UART_SendString("PB13_LEVEL: ");
    UART_SendFloat((float)pb13Level);
    UART_SendString("\r\n");

    UART_SendString("GPIO_TRANSITIONS: ");
    UART_SendFloat((float)rawEdges);
    UART_SendString("\r\n");

    UART_SendString("CAPTURE_IRQS: ");
    UART_SendFloat((float)frequencyCaptureIrqCount);
    UART_SendString("\r\n");

    UART_SendString("PERIOD_TICKS: ");
    UART_SendFloat((float)frequencyLastPeriodTicks);
    UART_SendString("\r\n");

    if (adcResult.clipped) {
        UART_SendString("WARNING: ADC CLIPPED\r\n");
    }
    if (!adcFrameValid) {
        UART_SendString("WARNING: ADC TIMEOUT\r\n");
    }

    UART_SendString("-----------------------\r\n");
}

static void App_UpdateDisplay(uint32_t frequencyHz)
{
    DisplayUI_Measurements measurements;
    uint32_t frequencyCentiHz;
    float dutyPercent;
    uint32_t i;

    for (i = 0U; i < ADC_SAMPLE_SIZE; i++) {
        gDisplaySamples[i] =
            (int16_t)(gADCSamples[i] & ADC_FULL_SCALE_CODE);
    }

    if (frequencyHz > (UINT32_MAX / 100U)) {
        frequencyCentiHz = UINT32_MAX;
    } else {
        frequencyCentiHz = frequencyHz * 100U;
    }

    dutyPercent = Frequency_GetDuty();
    if (dutyPercent < 0.0f) {
        dutyPercent = 0.0f;
    } else if (dutyPercent > 100.0f) {
        dutyPercent = 100.0f;
    }
    measurements.vpp_mv = (int32_t)(adcResult.vppV * 1000.0f + 0.5f);
    measurements.amplitude_mv =
        (int32_t)(adcResult.amplitudeV * 1000.0f + 0.5f);
    measurements.vmax_mv = (int32_t)(adcResult.maxV * 1000.0f + 0.5f);
    measurements.vmin_mv = (int32_t)(adcResult.minV * 1000.0f + 0.5f);
    measurements.vavg_mv =
        (int32_t)(adcResult.averageV * 1000.0f + 0.5f);
    measurements.freq_cHz = frequencyCentiHz;
    measurements.duty_x100 = (uint16_t)(dutyPercent * 100.0f + 0.5f);

    DisplayUI_DrawWaveformI16(gDisplaySamples, ADC_SAMPLE_SIZE, 0,
                              ADC_FULL_SCALE_CODE);
    DisplayUI_UpdateMeasurements(&measurements);
}
