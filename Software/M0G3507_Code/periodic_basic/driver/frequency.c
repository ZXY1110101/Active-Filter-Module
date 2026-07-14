#include "frequency.h"
#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

/* TIMG12在当前时钟树下使用80 MHz BUSCLK。 */
#define PERIOD_TIMER_CLOCK_HZ       (80000000U)
#define PERIOD_AVERAGE_PERIODS      (4U)

/* 10个100 ms窗口组成1秒闸门，最终分辨率为1 Hz。 */
#define GATE_AVERAGE_WINDOWS        (10U)
#define EDGE_COUNTER_RELOAD         (0xFFFFFFFFUL)

/* 自动量程滞回：高于5 kHz进闸门，低于4 kHz回测周。 */
#define AUTO_PERIOD_TO_GATE_HZ      (5000U)
#define AUTO_GATE_TO_PERIOD_HZ      (4000U)

typedef enum
{
    ACTIVE_MEASUREMENT_PERIOD = 0,
    ACTIVE_MEASUREMENT_GATE
} ActiveMeasurement;

static const DL_TimerG_CompareConfig gEdgeCounterConfig = {
    .compareMode = DL_TIMER_COMPARE_MODE_EDGE_COUNT,
    .count = EDGE_COUNTER_RELOAD - 1U,
    .startTimer = DL_TIMER_STOP,
    .edgeDetectMode = DL_TIMER_COMPARE_EDGE_DETECTION_MODE_RISING,
    .inputChan = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

static uint32_t gCaptureLoadValue = 0U;

static volatile bool gPeriodSynced = false;
static volatile uint32_t gPeriodCount = 0U;
static volatile uint64_t gPeriodTicksSum = 0U;
static volatile uint64_t gHighTicksSum = 0U;

static volatile uint32_t gEdgePrevious = EDGE_COUNTER_RELOAD;
static volatile uint64_t gPulseAccumulator = 0U;
static volatile uint32_t gGateWindowCount = 0U;

volatile uint32_t frequencyHz = 0U;
static volatile float gPeriodUs = 0.0f;
static volatile float gDutyPercent = 0.0f;

static volatile FrequencyMode gCurrentMode = FREQUENCY_MODE_AUTO;
static volatile ActiveMeasurement gActiveMeasurement =
    ACTIVE_MEASUREMENT_PERIOD;
static volatile ActiveMeasurement gRequestedMeasurement =
    ACTIVE_MEASUREMENT_PERIOD;

static void Frequency_StopHardware(void);
static void Frequency_ResetPeriodState(void);
static void Frequency_ResetGateState(void);
static void Frequency_ConfigurePeriodHardware(void);
static void Frequency_ConfigureGateHardware(void);

static void Frequency_StopHardware(void)
{
    DL_TimerG_stopCounter(CAPTURE_0_INST);
    DL_TimerA_stopCounter(TIMER_GATE_INST);

    NVIC_DisableIRQ(CAPTURE_0_INST_INT_IRQN);
    NVIC_DisableIRQ(TIMER_GATE_INST_INT_IRQN);
    NVIC_ClearPendingIRQ(CAPTURE_0_INST_INT_IRQN);
    NVIC_ClearPendingIRQ(TIMER_GATE_INST_INT_IRQN);
}

static void Frequency_ResetPeriodState(void)
{
    gPeriodSynced = false;
    gPeriodCount = 0U;
    gPeriodTicksSum = 0U;
    gHighTicksSum = 0U;
    gPeriodUs = 0.0f;
    gDutyPercent = 0.0f;
}

static void Frequency_ResetGateState(void)
{
    gEdgePrevious = EDGE_COUNTER_RELOAD;
    gPulseAccumulator = 0U;
    gGateWindowCount = 0U;
}

static void Frequency_ConfigurePeriodHardware(void)
{
    Frequency_StopHardware();

    /* 恢复SysConfig生成的TIMG12联合捕获配置。 */
    SYSCFG_DL_CAPTURE_0_init();
    gCaptureLoadValue = DL_TimerG_getLoadValue(CAPTURE_0_INST);
    DL_TimerG_setTimerCount(CAPTURE_0_INST, gCaptureLoadValue);

    DL_TimerG_clearInterruptStatus(
        CAPTURE_0_INST,
        DL_TIMERG_INTERRUPT_CC1_DN_EVENT |
            DL_TIMERG_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(CAPTURE_0_INST_INT_IRQN);

    gActiveMeasurement = ACTIVE_MEASUREMENT_PERIOD;
    NVIC_EnableIRQ(CAPTURE_0_INST_INT_IRQN);
    DL_TimerG_startCounter(CAPTURE_0_INST);
}

static void Frequency_ConfigureGateHardware(void)
{
    Frequency_StopHardware();

    /* 同一个TIMG12/PB13动态切换为32位外部上升沿计数器。 */
    DL_TimerG_disableInterrupt(
        CAPTURE_0_INST,
        DL_TIMERG_INTERRUPT_CC1_DN_EVENT |
            DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_clearInterruptStatus(
        CAPTURE_0_INST,
        DL_TIMERG_INTERRUPT_CC1_DN_EVENT |
            DL_TIMERG_INTERRUPT_ZERO_EVENT |
            DL_TIMERG_INTERRUPT_LOAD_EVENT);

    DL_TimerG_initCompareMode(
        CAPTURE_0_INST,
        (DL_TimerG_CompareConfig *)&gEdgeCounterConfig);
    DL_TimerG_setLoadValue(CAPTURE_0_INST, EDGE_COUNTER_RELOAD);
    DL_TimerG_setTimerCount(CAPTURE_0_INST, EDGE_COUNTER_RELOAD);
    DL_TimerG_enableClock(CAPTURE_0_INST);

    DL_TimerA_clearInterruptStatus(
        TIMER_GATE_INST,
        DL_TIMERA_INTERRUPT_LOAD_EVENT);
    NVIC_ClearPendingIRQ(TIMER_GATE_INST_INT_IRQN);

    gEdgePrevious = DL_TimerG_getTimerCount(CAPTURE_0_INST);
    gActiveMeasurement = ACTIVE_MEASUREMENT_GATE;

    NVIC_EnableIRQ(TIMER_GATE_INST_INT_IRQN);
    DL_TimerG_startCounter(CAPTURE_0_INST);
    DL_TimerA_startCounter(TIMER_GATE_INST);
}

void Frequency_Init(void)
{
    Frequency_StopHardware();

    DL_TimerG_setCoreHaltBehavior(
        CAPTURE_0_INST,
        DL_TIMER_CORE_HALT_IMMEDIATE);
    DL_TimerA_enableInterrupt(
        TIMER_GATE_INST,
        DL_TIMERA_INTERRUPT_LOAD_EVENT);
    DL_TimerA_clearInterruptStatus(
        TIMER_GATE_INST,
        DL_TIMERA_INTERRUPT_LOAD_EVENT);

    Frequency_ResetPeriodState();
    Frequency_ResetGateState();
    frequencyHz = 0U;
}

void Frequency_StartPeriod(void)
{
    gCurrentMode = FREQUENCY_MODE_PERIOD;
    gRequestedMeasurement = ACTIVE_MEASUREMENT_PERIOD;
    frequencyHz = 0U;
    Frequency_ResetPeriodState();
    Frequency_ResetGateState();
    Frequency_ConfigurePeriodHardware();
}

void Frequency_StartGate(void)
{
    gCurrentMode = FREQUENCY_MODE_GATE;
    gRequestedMeasurement = ACTIVE_MEASUREMENT_GATE;
    frequencyHz = 0U;
    Frequency_ResetPeriodState();
    Frequency_ResetGateState();
    Frequency_ConfigureGateHardware();
}

void Frequency_StartAuto(void)
{
    gCurrentMode = FREQUENCY_MODE_AUTO;
    gRequestedMeasurement = ACTIVE_MEASUREMENT_PERIOD;
    frequencyHz = 0U;
    Frequency_ResetPeriodState();
    Frequency_ResetGateState();
    Frequency_ConfigurePeriodHardware();
}

void Frequency_Task(void)
{
    if ((gCurrentMode != FREQUENCY_MODE_AUTO) ||
        (gRequestedMeasurement == gActiveMeasurement))
    {
        return;
    }

    if (gRequestedMeasurement == ACTIVE_MEASUREMENT_GATE)
    {
        Frequency_ResetGateState();
        Frequency_ConfigureGateHardware();
    }
    else
    {
        Frequency_ResetPeriodState();
        Frequency_ConfigurePeriodHardware();
    }
}

uint32_t Frequency_GetHz(void)
{
    return frequencyHz;
}

float Frequency_GetPeriodUs(void)
{
    return gPeriodUs;
}

float Frequency_GetDuty(void)
{
    return gDutyPercent;
}

FrequencyMode Frequency_GetMode(void)
{
    return gCurrentMode;
}

void CAPTURE_0_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(CAPTURE_0_INST))
    {
        case DL_TIMER_IIDX_CC1_DN:
        {
            uint32_t capturePeriod;
            uint32_t captureHigh;
            uint32_t periodTicks;
            uint32_t highTicks;

            if (gActiveMeasurement != ACTIVE_MEASUREMENT_PERIOD)
            {
                break;
            }

            capturePeriod = DL_TimerG_getCaptureCompareValue(
                CAPTURE_0_INST, DL_TIMER_CC_1_INDEX);
            captureHigh = DL_TimerG_getCaptureCompareValue(
                CAPTURE_0_INST, DL_TIMER_CC_0_INDEX);

            if (!gPeriodSynced)
            {
                gPeriodSynced = true;
            }
            else
            {
                periodTicks = gCaptureLoadValue - capturePeriod;
                highTicks = gCaptureLoadValue - captureHigh;

                if ((periodTicks > 0U) && (highTicks <= periodTicks))
                {
                    gPeriodTicksSum += periodTicks;
                    gHighTicksSum += highTicks;
                    gPeriodCount++;

                    if (gPeriodCount >= PERIOD_AVERAGE_PERIODS)
                    {
                        uint32_t averagePeriodTicks =
                            (uint32_t)(gPeriodTicksSum /
                                PERIOD_AVERAGE_PERIODS);
                        uint32_t averageHighTicks =
                            (uint32_t)(gHighTicksSum /
                                PERIOD_AVERAGE_PERIODS);

                        frequencyHz =
                            (uint32_t)(((uint64_t)PERIOD_TIMER_CLOCK_HZ *
                                PERIOD_AVERAGE_PERIODS) /
                                gPeriodTicksSum);
                        gPeriodUs =
                            ((float)averagePeriodTicks * 1000000.0f) /
                            (float)PERIOD_TIMER_CLOCK_HZ;
                        gDutyPercent =
                            ((float)averageHighTicks * 100.0f) /
                            (float)averagePeriodTicks;

                        gPeriodCount = 0U;
                        gPeriodTicksSum = 0U;
                        gHighTicksSum = 0U;

                        if ((gCurrentMode == FREQUENCY_MODE_AUTO) &&
                            (frequencyHz >= AUTO_PERIOD_TO_GATE_HZ))
                        {
                            gRequestedMeasurement =
                                ACTIVE_MEASUREMENT_GATE;
                            DL_TimerG_stopCounter(CAPTURE_0_INST);
                            NVIC_DisableIRQ(CAPTURE_0_INST_INT_IRQN);
                        }
                    }
                }
            }

            DL_TimerG_setTimerCount(
                CAPTURE_0_INST, gCaptureLoadValue);
            break;
        }

        case DL_TIMER_IIDX_ZERO:
            gPeriodSynced = false;
            gPeriodCount = 0U;
            gPeriodTicksSum = 0U;
            gHighTicksSum = 0U;
            frequencyHz = 0U;
            break;

        default:
            break;
    }
}

void TIMER_GATE_INST_IRQHandler(void)
{
    uint32_t currentSnapshot =
        DL_TimerG_getTimerCount(CAPTURE_0_INST);
    uint32_t pendingInterrupt =
        DL_TimerA_getPendingInterrupt(TIMER_GATE_INST);
    uint32_t pulseCount;

    if ((pendingInterrupt != DL_TIMERA_IIDX_LOAD) ||
        (gActiveMeasurement != ACTIVE_MEASUREMENT_GATE))
    {
        return;
    }

    /* 32位无符号减法自动处理一次计数器回绕。 */
    pulseCount = gEdgePrevious - currentSnapshot;
    gEdgePrevious = currentSnapshot;

    gPulseAccumulator += pulseCount;
    gGateWindowCount++;

    if (gGateWindowCount >= GATE_AVERAGE_WINDOWS)
    {
        frequencyHz = (uint32_t)gPulseAccumulator;
        gPulseAccumulator = 0U;
        gGateWindowCount = 0U;

        if ((gCurrentMode == FREQUENCY_MODE_AUTO) &&
            (frequencyHz <= AUTO_GATE_TO_PERIOD_HZ))
        {
            gRequestedMeasurement = ACTIVE_MEASUREMENT_PERIOD;
            DL_TimerA_stopCounter(TIMER_GATE_INST);
            DL_TimerG_stopCounter(CAPTURE_0_INST);
            NVIC_DisableIRQ(TIMER_GATE_INST_INT_IRQN);
        }
    }
}
