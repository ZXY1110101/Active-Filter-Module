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
#include "PGA.h"
#include "UART.h"
#include "frequency.h"

#define MEASUREMENT_FRAMES_PER_REPORT (10U)
#define MAIN_LOOP_DELAY_CYCLES        (1600000U)

static void App_Init(void);
static void App_RunMeasurementCycle(void);
static void App_PrintBanner(void);
static void App_PrintMeasurement(uint32_t frequencyHz);

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
    Frequency_Init();

    App_PrintBanner();
    Frequency_StartAuto();
}

/* 执行一次完整的“按键处理 -> ADC 采样 -> 数据输出”任务。 */
static void App_RunMeasurementCycle(void)
{
    static uint32_t frameCount = 0U;

    Frequency_Task();
    PGA_KeyScan();

    ADC_CaptureFrame();
    ADC_AnalyzeSamples();

    frameCount++;
    if (frameCount >= MEASUREMENT_FRAMES_PER_REPORT) {
        frameCount = 0U;
        App_PrintMeasurement(Frequency_GetHz());
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

static void App_PrintMeasurement(uint32_t frequencyHz)
{
    UART_SendString("\r\n-------- DATA --------\r\n");

    UART_SendString("GAIN: ");
    UART_SendFloat((float)PGA_GetGain());
    UART_SendString("\r\n");

    UART_SendString("VPP: ");
    UART_SendFloat(adcResult.vppV);
    UART_SendString(" V\r\n");

    UART_SendString("RMS: ");
    UART_SendFloat(adcResult.rmsV);
    UART_SendString(" V\r\n");

    UART_SendString("FREQ: ");
    UART_SendFloat((float)frequencyHz);
    UART_SendString(" Hz\r\n");

    UART_SendString("-----------------------\r\n");
}
