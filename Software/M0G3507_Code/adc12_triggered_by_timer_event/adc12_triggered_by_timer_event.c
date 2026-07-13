/*
 * Copyright (c) 2020, Texas Instruments Incorporated
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

// #include "ti_msp_dl_config.h"

// volatile bool gCheckADC;
// volatile uint16_t gADCResult[4];

// int main(void)
// {
//     SYSCFG_DL_init();

//     NVIC_EnableIRQ(ADC12_0_INST_INT_IRQN);

//     gCheckADC = false;

//     DL_TimerG_startCounter(TIMER_0_INST);

//     while (false == gCheckADC) {
//         __WFE();
//     }

//     gADCResult[0] = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
//     gADCResult[1] = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_1);
//     gADCResult[2] = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_2);
//     gADCResult[3] = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_3);

//     __BKPT(0);

//     while (1) {
//         __WFI();
//     }
// }

// void ADC12_0_INST_IRQHandler(void)
// {
//     switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST)) {
//         case DL_ADC12_IIDX_MEM3_RESULT_LOADED:
//             gCheckADC = true;
//             break;
//         default:
//             break;
//     }
// }
#include "ti_msp_dl_config.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* 一帧ADC采样点数 */
#define ADC_SAMPLE_SIZE       (1024U)

/* 稳健峰值算法：分别平均最高、最低的16个采样点 */
#define PEAK_AVG_COUNT        (16U)

#define ADC_REFERENCE_V       (3.3f)
#define ADC_FULL_SCALE_CODE   (4095.0f)

/*
 * 固定采样率。
 * 必须与SysConfig中的Timer触发周期一致：
 * 10 us一次触发，对应100 kSPS。
 */
#define ADC_SAMPLE_RATE_HZ    (100000U)

/*
 * ADC采样缓冲区：
 * 当前配置为FIFO关闭、MEM0直接触发DMA、
 * Half Word到Half Word，每次DMA搬一个16位采样值。
 */
uint16_t gADCSamples[ADC_SAMPLE_SIZE];

/*
 * 排序工作缓冲区。
 * 不能直接排序gADCSamples，否则会破坏原始采样波形。
 */
static uint16_t gSortBuffer[ADC_SAMPLE_SIZE];

/* 一帧DMA采集完成标志 */
volatile bool gADCFrameReady = false;

/* ADC模块最终输出的四个测量结果 */
volatile float adcAverageV   = 0.0f;
volatile float adcVppV       = 0.0f;
volatile float adcAmplitudeV = 0.0f;
volatile float adcRmsV       = 0.0f;


/* ==================== 函数声明 ==================== */

static int CompareUint16(const void *a, const void *b);

static float ADC_CalculateRobustVppCode(
    const uint16_t *samples,
    uint32_t count);

static void ADC_AnalyzeSamples(
    const uint16_t *samples,
    uint32_t count);

static void ADC_DMA_PrepareFrame(void);

static void ADC_StartFrame(void);


/* ==================== 排序比较函数 ==================== */

static int CompareUint16(const void *a, const void *b)
{
    const uint16_t va = *(const uint16_t *)a;
    const uint16_t vb = *(const uint16_t *)b;

    if (va < vb) {
        return -1;
    }

    if (va > vb) {
        return 1;
    }

    return 0;
}


/**
 * @brief 使用顶部、底部多点平均，计算稳健峰峰值
 *
 * 与单纯max-min相比，可以减小单个毛刺、过冲和异常采样点
 * 对峰峰值结果的影响。
 *
 * @return 峰峰值对应的ADC码值
 */
static float ADC_CalculateRobustVppCode(
    const uint16_t *samples,
    uint32_t count)
{
    if ((samples == NULL) ||
        (count < (2U * PEAK_AVG_COUNT)) ||
        (count > ADC_SAMPLE_SIZE)) {
        return 0.0f;
    }

    /*
     * 复制采样数据。
     * 不直接对原数组排序，保留原始波形数据。
     */
    for (uint32_t i = 0U; i < count; i++)
    {
        gSortBuffer[i] = samples[i] & 0x0FFFU;
    }

    qsort(
        gSortBuffer,
        count,
        sizeof(gSortBuffer[0]),
        CompareUint16);

    uint32_t lowSum  = 0U;
    uint32_t highSum = 0U;

    for (uint32_t i = 0U; i < PEAK_AVG_COUNT; i++)
    {
        lowSum += gSortBuffer[i];

        highSum +=
            gSortBuffer[count - 1U - i];
    }

    const float lowCode =
        (float)lowSum / (float)PEAK_AVG_COUNT;

    const float highCode =
        (float)highSum / (float)PEAK_AVG_COUNT;

    return highCode - lowCode;
}


/**
 * @brief 分析固定采样率采集的一帧ADC数据
 *
 * 输出：
 * adcAverageV   平均值/直流偏置
 * adcVppV       稳健峰峰值
 * adcAmplitudeV 峰峰值的一半
 * adcRmsV       去除平均值后的交流RMS
 */
static void ADC_AnalyzeSamples(
    const uint16_t *samples,
    uint32_t count)
{
    if ((samples == NULL) ||
        (count == 0U) ||
        (count > ADC_SAMPLE_SIZE)) {
        adcAverageV   = 0.0f;
        adcVppV       = 0.0f;
        adcAmplitudeV = 0.0f;
        adcRmsV       = 0.0f;
        return;
    }

    uint32_t sumCode = 0U;

    /*
     * 第一遍：
     * 计算平均值。
     */
    for (uint32_t i = 0U; i < count; i++)
    {
        const uint16_t value =
            samples[i] & 0x0FFFU;

        sumCode += value;
    }

    const float averageCode =
        (float)sumCode / (float)count;

    /*
     * 第二遍：
     * 减去平均值后计算交流RMS。
     */
    float acSquareSum = 0.0f;

    for (uint32_t i = 0U; i < count; i++)
    {
        const float acCode =
            (float)(samples[i] & 0x0FFFU) -
            averageCode;

        acSquareSum += acCode * acCode;
    }

    /*
     * 使用稳健峰值算法计算Vpp，
     * 不再使用单个最大值减最小值。
     */
    const float vppCode =
        ADC_CalculateRobustVppCode(samples, count);

    const float rmsCode =
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
 * @brief 重新装载DMA，准备采集1024点
 */
static void ADC_DMA_PrepareFrame(void)
{
    gADCFrameReady = false;

    DL_DMA_disableChannel(
        DMA,
        DMA_CH0_CHAN_ID);

    /*
     * DMA源：
     * ADC MEM0结果寄存器。
     *
     * 当前SysConfig设置：
     * Source Length      = Half Word
     * Destination Length = Half Word
     * Source Address     = Fixed
     * Destination        = Increment
     */
    DL_DMA_setSrcAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        DL_ADC12_getMemResultAddress(
            ADC12_0_INST,
            DL_ADC12_MEM_IDX_0));

    /*
     * DMA目的：
     * 1024点采样缓冲区。
     */
    DL_DMA_setDestAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t)&gADCSamples[0]);

    /*
     * FIFO关闭，一次DMA触发只搬一个16位采样值。
     * 因此1024个采样点需要1024次DMA传输。
     */
    DL_DMA_setTransferSize(
        DMA,
        DMA_CH0_CHAN_ID,
        ADC_SAMPLE_SIZE);

    DL_DMA_enableChannel(
        DMA,
        DMA_CH0_CHAN_ID);
}


/**
 * @brief 启动一次固定采样率采集
 */
static void ADC_StartFrame(void)
{
    ADC_DMA_PrepareFrame();

    /*
     * ADC配置为Event硬件触发。
     * 启动后等待Timer事件，不会自行满速连续转换。
     */
    DL_ADC12_startConversion(
        ADC12_0_INST);

    /*
     * 启动产生ADC采样事件的定时器。
     */
    DL_TimerG_startCounter(
        TIMER_0_INST);
}


int main(void)
{
    SYSCFG_DL_init();

    /*
     * 旧ADC DMA done中断暂时保留注释。
     * 当前使用DMA通道完成中断判断整帧采集结束。
     */
    // NVIC_ClearPendingIRQ(
    //     ADC12_0_INST_INT_IRQN);

    // NVIC_EnableIRQ(
    //     ADC12_0_INST_INT_IRQN);

    NVIC_ClearPendingIRQ(DMA_INT_IRQn);
    NVIC_EnableIRQ(DMA_INT_IRQn);

    /*
     * 数据链：
     *
     * Timer每10 us发布一次事件
     * → ADC MEM0转换一次
     * → MEM0 result loaded请求DMA
     * → DMA搬一个16位采样值
     * → 搬满1024点产生DMA通道完成中断
     */
    ADC_StartFrame();

    while (!gADCFrameReady)
    {
        __WFE();
    }

    /*
     * 一帧采满后停止Timer，
     * 避免ADC继续被触发。
     */
    DL_TimerG_stopCounter(
        TIMER_0_INST);

    DL_ADC12_stopConversion(
        ADC12_0_INST);

    /*
     * 在主程序中分析数据，
     * 不在中断里执行循环、排序和sqrtf。
     */
    ADC_AnalyzeSamples(
        gADCSamples,
        ADC_SAMPLE_SIZE);

    /*
     * 100 kSPS下采1024点：
     * 一帧时长约10.24 ms。
     *
     * Watch查看：
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
 * @brief 原ADC DMA完成中断
 *
 * 当前暂时不用，但按你的要求保留。
 */
// void ADC12_0_INST_IRQHandler(void)
// {
//     switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST))
//     {
//         case DL_ADC12_IIDX_DMA_DONE:
//             gADCFrameReady = true;
//             break;
//
//         default:
//             break;
//     }
// }


/**
 * @brief DMA通道完成中断
 *
 * DMA真正完成1024次半字传输后，
 * 才把整帧完成标志置为true。
 */
void DMA_IRQHandler(void)
{
    switch (DL_DMA_getPendingInterrupt(DMA))
    {
        case DL_DMA_EVENT_IIDX_DMACH0:
            gADCFrameReady = true;
            break;

        default:
            break;
    }
}