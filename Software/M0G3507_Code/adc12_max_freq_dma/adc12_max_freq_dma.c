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

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* 实际ADC采样点数 */
#define ADC_SAMPLE_SIZE      (512U)

/*
 * ADC FIFO开启压缩后：
 * 一个32位FIFO数据包含两个12位ADC采样值。
 * 因此DMA只需要搬运512个32位数据。
 */
#define ADC_FIFO_TRANSFERS   (ADC_SAMPLE_SIZE / 2U)

#define ADC_REFERENCE_V      (3.3f)
#define ADC_FULL_SCALE_CODE  (4095.0f)

/*
 * DMA直接把FIFO中的压缩数据搬到这里。
 * 数组总大小为2048字节，刚好容纳1024个uint16_t采样点。
 */
static uint16_t gADCSamples[ADC_SAMPLE_SIZE];

/* DMA完成标志 */
static volatile bool gADCFrameReady = false;

/* 最终需要的四个测量结果 */
volatile float adcAverageV   = 0.0f;
volatile float adcVppV       = 0.0f;
volatile float adcAmplitudeV = 0.0f;
volatile float adcRmsV       = 0.0f;


/**
 * @brief 分析一帧ADC采样
 *
 * adcAverageV：
 *     原始采样平均电压，包括直流偏置。
 *
 * adcVppV：
 *     最大值减最小值。
 *
 * adcAmplitudeV：
 *     峰峰值的一半。
 *
 * adcRmsV：
 *     去掉平均值后的交流RMS。
 */
static void ADC_AnalyzeSamples(
    const uint16_t *samples,
    uint32_t count)
{
    if ((samples == NULL) || (count == 0U)) {
        adcAverageV   = 0.0f;
        adcVppV       = 0.0f;
        adcAmplitudeV = 0.0f;
        adcRmsV       = 0.0f;
        return;
    }

    uint16_t minCode = 4095U;
    uint16_t maxCode = 0U;
    uint32_t sumCode = 0U;

    /*
     * 第一遍：
     * 计算最小值、最大值和平均值。
     */
    for (uint32_t i = 0U; i < count; i++)
    {
        uint16_t value = samples[i] & 0x0FFFU;

        if (value < minCode) {
            minCode = value;
        }

        if (value > maxCode) {
            maxCode = value;
        }

        sumCode += value;
    }

    float averageCode =
        (float)sumCode / (float)count;

    /*
     * 第二遍：
     * 减掉平均值后计算交流RMS。
     */
    float acSquareSum = 0.0f;

    for (uint32_t i = 0U; i < count; i++)
    {
        float acCode =
            (float)(samples[i] & 0x0FFFU) - averageCode;

        acSquareSum += acCode * acCode;
    }

    float vppCode =
        (float)(maxCode - minCode);

    float rmsCode =
        sqrtf(acSquareSum / (float)count);

    adcAverageV =
        averageCode *
        ADC_REFERENCE_V /
        ADC_FULL_SCALE_CODE;

    adcVppV =
        vppCode *
        ADC_REFERENCE_V /
        ADC_FULL_SCALE_CODE;

    adcAmplitudeV =
        adcVppV / 2.0f;

    adcRmsV =
        rmsCode *
        ADC_REFERENCE_V /
        ADC_FULL_SCALE_CODE;
}


/**
 * @brief 配置并启动DMA
 */
static void ADC_DMA_Start(void)
{
    gADCFrameReady = false;

    /*
     * ADC FIFO寄存器作为DMA源地址。
     * 源地址固定，不递增。
     */
    DL_DMA_setSrcAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t)DL_ADC12_getFIFOAddress(ADC12_0_INST));

    /*
     * 采样数组作为DMA目的地址。
     * 目的地址递增方式由SysConfig生成。
     */
    DL_DMA_setDestAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t)&gADCSamples[0]);

    /*
     * FIFO压缩模式下：
     * 每次DMA传输得到两个uint16_t采样点。
     */
    DL_DMA_setTransferSize(
        DMA,
        DMA_CH0_CHAN_ID,
        ADC_FIFO_TRANSFERS);

    DL_DMA_enableChannel(
        DMA,
        DMA_CH0_CHAN_ID);
}


int main(void)
{
    SYSCFG_DL_init();

    ADC_DMA_Start();

    /*
     * 此例程使用ADC的DMA完成中断。
     */
    NVIC_ClearPendingIRQ(
        ADC12_0_INST_INT_IRQN);

    NVIC_EnableIRQ(
        ADC12_0_INST_INT_IRQN);

    /*
     * 满速自由运行版本：
     * 启动ADC后，ADC会持续转换并产生DMA请求。
     */
    DL_ADC12_startConversion(
        ADC12_0_INST);

    while (!gADCFrameReady)
    {
        __WFE();
    }

    /*
     * 不要在中断中遍历1024点或计算sqrtf。
     * DMA完成后回到主程序分析。
     */
    ADC_AnalyzeSamples(
        gADCSamples,
        ADC_SAMPLE_SIZE);

    /*
     * 程序停在这里，方便Watch查看：
     *
     * adcAverageV
     * adcVppV
     * adcAmplitudeV
     * adcRmsV
     * gADCSamples
     */
    __BKPT(0);

    while (1)
    {
        __WFI();
    }
}


/**
 * @brief ADC中断
 *
 * 中断中只置位标志，避免执行耗时浮点运算。
 */
void ADC12_0_INST_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST))
    {
        case DL_ADC12_IIDX_DMA_DONE:
            gADCFrameReady = true;
            break;

        default:
            break;
    }
}