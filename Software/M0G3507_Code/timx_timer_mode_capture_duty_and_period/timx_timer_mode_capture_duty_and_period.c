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

#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

/* 当前捕获定时器实际计数频率：32 MHz */
#define TIMER_CLOCK_HZ     (32000000U)

/* 连续测量100个周期后计算一次结果 */
#define AVERAGE_PERIODS    (100U)

/* 官方例程需要的计数器装载值 */
static uint32_t gLoadValue = 0U;

/* 第一次捕获只用于同步，不参与计算 */
static volatile bool gSynced = false;

/* 100个周期计算完成标志 */
static volatile bool gMeasurementReady = false;

/* 周期累计 */
static volatile uint32_t gPeriodCount = 0U;
static volatile uint64_t gPeriodTicksSum = 0U;

/* 高电平时间累计 */
static volatile uint64_t gHighTicksSum = 0U;

/* 最终测量结果，放进Watch查看 */
volatile uint32_t frequencyHz = 0U;

volatile float periodUs = 0.0f;
volatile float highTimeUs = 0.0f;
volatile float lowTimeUs = 0.0f;
volatile float dutyPercent = 0.0f;

/* 平均计数值，方便调试 */
volatile uint32_t averagePeriodTicks = 0U;
volatile uint32_t averageHighTicks = 0U;


int main(void)
{
    SYSCFG_DL_init();

    /*
     * 获取SysConfig生成的定时器装载值。
     * 当前设置约为2 ms，对应约64000个计数。
     */
    gLoadValue =
        DL_TimerG_getLoadValue(CAPTURE_0_INST);

    gSynced = false;
    gMeasurementReady = false;

    /*
     * 调试暂停时立即停止定时器，
     * 防止PWM和捕获定时器失去同步。
     */
    DL_TimerG_setCoreHaltBehavior(
        CAPTURE_0_INST,
        DL_TIMER_CORE_HALT_IMMEDIATE);

    DL_TimerG_setCoreHaltBehavior(
        PWM_0_INST,
        DL_TIMER_CORE_HALT_IMMEDIATE);

    NVIC_EnableIRQ(CAPTURE_0_INST_INT_IRQN);

    DL_TimerG_startCounter(CAPTURE_0_INST);
    DL_TimerG_startCounter(PWM_0_INST);

    while (1)
    {
        /*
         * 等待100个周期测量完成。
         */
        while (!gMeasurementReady)
        {
            __WFE();
        }

        gMeasurementReady = false;

        /*
         * 此处停下后，在Watch中查看：
         *
         * frequencyHz
         * periodUs
         * dutyPercent
         * highTimeUs
         * lowTimeUs
         * averagePeriodTicks
         * averageHighTicks
         */
        __BKPT(0);
    }
}


void CAPTURE_0_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(CAPTURE_0_INST))
    {
        case DL_TIMER_IIDX_CC1_DN:
        {
            /*
             * Combined Capture模式：
             *
             * CC1：记录完整周期对应的捕获值
             * CC0：记录高电平时间对应的捕获值
             *
             * 定时器为向下计数，所以：
             * ticks = LoadValue - CaptureValue
             */
            uint32_t capturePeriodValue =
                DL_TimerG_getCaptureCompareValue(
                    CAPTURE_0_INST,
                    DL_TIMER_CC_1_INDEX);

            uint32_t captureHighValue =
                DL_TimerG_getCaptureCompareValue(
                    CAPTURE_0_INST,
                    DL_TIMER_CC_0_INDEX);

            if (!gSynced)
            {
                /*
                 * 第一次捕获用于完成同步。
                 */
                gSynced = true;
            }
            else
            {
                uint32_t currentPeriodTicks =
                    gLoadValue - capturePeriodValue;

                uint32_t currentHighTicks =
                    gLoadValue - captureHighValue;

                /*
                 * 防止异常捕获值进入统计。
                 */
                if ((currentPeriodTicks > 0U) &&
                    (currentHighTicks <= currentPeriodTicks))
                {
                    gPeriodTicksSum += currentPeriodTicks;
                    gHighTicksSum += currentHighTicks;
                    gPeriodCount++;

                    if (gPeriodCount >= AVERAGE_PERIODS)
                    {
                        averagePeriodTicks =
                            (uint32_t)(
                                gPeriodTicksSum /
                                AVERAGE_PERIODS);

                        averageHighTicks =
                            (uint32_t)(
                                gHighTicksSum /
                                AVERAGE_PERIODS);

                        /*
                         * 多周期平均频率：
                         *
                         * f = TimerClock × 周期数
                         *     ------------------
                         *       总周期计数
                         */
                        frequencyHz =
                            (uint32_t)(
                                ((uint64_t)TIMER_CLOCK_HZ *
                                 AVERAGE_PERIODS) /
                                gPeriodTicksSum);

                        periodUs =
                            ((float)averagePeriodTicks *
                             1000000.0f) /
                            (float)TIMER_CLOCK_HZ;

                        highTimeUs =
                            ((float)averageHighTicks *
                             1000000.0f) /
                            (float)TIMER_CLOCK_HZ;

                        lowTimeUs =
                            periodUs - highTimeUs;

                        dutyPercent =
                            ((float)averageHighTicks *
                             100.0f) /
                            (float)averagePeriodTicks;

                        /*
                         * 清零，准备下一组100周期测量。
                         */
                        gPeriodTicksSum = 0U;
                        gHighTicksSum = 0U;
                        gPeriodCount = 0U;

                        gMeasurementReady = true;
                    }
                }
            }

            /*
             * 官方例程针对捕获限制采用手动重载。
             */
            DL_TimerG_setTimerCount(
                CAPTURE_0_INST,
                gLoadValue);

            break;
        }

        default:
            break;
    }
}
