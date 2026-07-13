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

/*
 * TIMA0闸门为100 ms：
 * frequency = 100ms内脉冲数 × 10
 */
#define GATE_FREQUENCY_SCALE       (10U)

/*
 * 连续累计10个100 ms窗口，得到1秒平均结果。
 */
#define AVERAGE_GATE_WINDOWS       (10U)

/*
 * TIMG12是32位定时器，使用完整32位计数范围。
 */
#define EDGE_COUNTER_RELOAD        (0xFFFFFFFFUL)

/* ==================== 测量结果 ==================== */

/* 上一次和本次读取的TIMG12计数器值 */
volatile uint32_t edgePrevious     = 0U;
volatile uint32_t edgeCurrent      = 0U;

/* 最近100 ms内的脉冲数 */
volatile uint32_t pulseCount       = 0U;

/* 100 ms快速频率结果，分辨率10 Hz */
volatile uint32_t frequencyHzFast  = 0U;

/* 1秒累计平均频率结果，分辨率1 Hz */
volatile uint32_t frequencyHz      = 0U;

/* 1秒累计使用 */
volatile uint64_t pulseAccumulator = 0U;
volatile uint32_t averageWindowCount = 0U;

/* ==================== 诊断变量 ==================== */

volatile uint32_t gateIrqCount     = 0U;
volatile uint32_t gateIrqIndex     = 0U;

/* 当前输入已变成PA10，不再是PB2 */
volatile uint32_t inputLevel       = 0U;

volatile bool edgeRunning          = false;
volatile bool gateRunning          = false;

int main(void)
{
    SYSCFG_DL_init();

    /*
     * SysConfig当前已经设置为不自动启动。
     * 这里仍强制停止，保证后面的寄存器设置安全。
     */
    DL_TimerG_stopCounter(COMPARE_0_INST);
    DL_TimerA_stopCounter(TIMER_GATE_INST);

    /*
     * SysConfig目前只生成LOAD=65534。
     * TIMG12为32位定时器，这里扩展到完整32位范围。
     */
    DL_TimerG_setLoadValue(
        COMPARE_0_INST,
        EDGE_COUNTER_RELOAD);

    /*
     * DL_TimerG_setTimerCount()只能在计数器停止时调用。
     */
    DL_TimerG_setTimerCount(
        COMPARE_0_INST,
        EDGE_COUNTER_RELOAD);

    /*
     * 清除可能遗留的外设中断标志。
     */
    DL_TimerG_clearInterruptStatus(
        COMPARE_0_INST,
        DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerA_clearInterruptStatus(
        TIMER_GATE_INST,
        DL_TIMERA_INTERRUPT_LOAD_EVENT);

    /*
     * 明确打开TIMA0的LOAD中断。
     */
    DL_TimerA_enableInterrupt(
        TIMER_GATE_INST,
        DL_TIMERA_INTERRUPT_LOAD_EVENT);

    NVIC_ClearPendingIRQ(TIMER_GATE_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_GATE_INST_INT_IRQN);

    /*
     * 记录初始计数值。
     */
    edgePrevious =
        DL_TimerG_getTimerCount(COMPARE_0_INST);

    /*
     * 先启动外部边沿计数，再启动100 ms闸门。
     */
    DL_TimerG_startCounter(COMPARE_0_INST);
    DL_TimerA_startCounter(TIMER_GATE_INST);

    while (1)
    {
        edgeRunning =
            DL_TimerG_isRunning(COMPARE_0_INST);

        gateRunning =
            DL_TimerA_isRunning(TIMER_GATE_INST);

        /*
         * 输入引脚现在是PA10。
         * 该变量只是辅助观察瞬时电平。
         */
        inputLevel =
            (DL_GPIO_readPins(
                GPIO_COMPARE_0_C0_PORT,
                GPIO_COMPARE_0_C0_PIN) != 0U);

        __WFI();
    }
}

/*
 * TIMA0的100 ms周期中断。
 *
 * TIMER_GATE_INST_IRQHandler宏会展开为TIMA0_IRQHandler。
 */
void TIMER_GATE_INST_IRQHandler(void)
{
    uint32_t currentSnapshot;

    /*
     * 尽量在进入ISR后立即读取TIMG12，
     * 减少其他运算引入的额外时间偏差。
     */
    currentSnapshot =
        DL_TimerG_getTimerCount(COMPARE_0_INST);

    gateIrqIndex =
        DL_TimerA_getPendingInterrupt(TIMER_GATE_INST);

    if (gateIrqIndex == DL_TIMERA_IIDX_LOAD)
    {
        gateIrqCount++;

        edgeCurrent = currentSnapshot;

        /*
         * TIMG12配置为向下计数。
         *
         * 无符号32位减法能够自动处理一次计数器回卷：
         *
         * 正常情况：
         * previous = 1000
         * current  = 900
         * delta    = 100
         *
         * 回卷情况：
         * previous = 2
         * current  = 0xFFFFFFFF
         * delta    = 3
         */
        pulseCount =
            edgePrevious - edgeCurrent;

        edgePrevious = edgeCurrent;

        /*
         * 100 ms快速结果：
         * frequency = pulseCount × 10
         */
        frequencyHzFast =
            pulseCount * GATE_FREQUENCY_SCALE;

        /*
         * 累计10个100 ms窗口，也就是1秒。
         *
         * 1秒内累计脉冲数在数值上就等于Hz。
         */
        pulseAccumulator += pulseCount;
        averageWindowCount++;

        if (averageWindowCount >= AVERAGE_GATE_WINDOWS)
        {
            frequencyHz =
                (uint32_t) pulseAccumulator;

            pulseAccumulator = 0U;
            averageWindowCount = 0U;
        }

        /*
         * 每100 ms翻转一次PB14，确认闸门中断运行。
         */
        DL_GPIO_togglePins(
            GPIO_LEDS_PORT,
            GPIO_LEDS_USER_TEST_PIN);
    }
}