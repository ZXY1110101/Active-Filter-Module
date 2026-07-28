#include "signal_analyzer.h"

#include "adc.h"
#include "tim.h"
#include "arm_math.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define ADC_DMA_SAMPLE_COUNT         (2U * SIGNAL_FFT_SIZE)
#define FRAME_SLOT_COUNT             3U
#define FRAME_CAPTURE_PERIOD_MS      16U
#define ADC_REFERENCE_VOLTAGE        3.3f
#define ADC_FULL_SCALE_COUNTS        4095.0f
#define SPECTRUM_FLOOR_DB            (-60.0f)
#define SINE_FIT_PERIOD_MS           1000U
#define SAMPLE_RATE_ADJUST_PERIOD_MS 1000U
#define MAX_SAMPLE_RATE_HZ           1000000.0f
#define SAMPLE_RATE_HYSTERESIS_RATIO 0.02f
#define MAX_FULL_PERIOD_COUNT        256U
#define CAPTURE_TIMEOUT_MIN_MS       500U
#define CAPTURE_TIMEOUT_PERIODS      3U
#define EQUAL_GATE_TIME_MS           1000U
#define EQUAL_EDGE_TIMEOUT_MS        2000U
#define EQUAL_FREQUENCY_STALE_MS     3000U

enum
{
    FRAME_FREE = 0U,
    FRAME_READY,
    FRAME_READING
};

typedef enum
{
    EQUAL_STATE_IDLE = 0U,
    EQUAL_STATE_WAIT_START,
    EQUAL_STATE_RUNNING,
    EQUAL_STATE_WAIT_STOP,
    EQUAL_STATE_READY
} equal_frequency_state_t;

static uint16_t s_adc_dma[ADC_DMA_SAMPLE_COUNT] __attribute__((aligned(4)));
static uint16_t s_frames[FRAME_SLOT_COUNT][SIGNAL_FFT_SIZE] __attribute__((aligned(4)));
static volatile uint8_t s_frame_state[FRAME_SLOT_COUNT];
static volatile uint32_t s_frame_sequence[FRAME_SLOT_COUNT];
static volatile uint32_t s_next_sequence;
static volatile uint32_t s_dropped_frames;
static volatile uint8_t s_irq_divider;
static volatile uint8_t s_irq_divider_limit = 1U;

static arm_rfft_fast_instance_f32 s_fft;
static float32_t s_hann[SIGNAL_FFT_SIZE];
static float32_t s_fft_input[SIGNAL_FFT_SIZE];
static float32_t s_fft_output[SIGNAL_FFT_SIZE];
static float32_t s_magnitude[SIGNAL_FFT_SIZE / 2U];
static float32_t s_sample_rate_hz;
static uint32_t s_last_sample_rate_update_ms;
static signal_analysis_t s_result;
static uint32_t s_last_fit_ms;
static uint8_t s_fit_valid;

static volatile uint32_t s_capture_period;
static volatile uint32_t s_capture_high;
static volatile uint32_t s_capture_tick_ms;

/*
 * 等精度测频状态。
 */
static volatile equal_frequency_state_t s_equal_state;

static volatile uint32_t s_equal_input_start;
static volatile uint32_t s_equal_input_stop;
static volatile uint32_t s_equal_reference_start;
static volatile uint32_t s_equal_reference_stop;

static volatile uint32_t s_equal_gate_start_ms;
static volatile uint32_t s_equal_stop_request_ms;

static float32_t s_equal_frequency_hz;
static uint32_t s_equal_frequency_tick_ms;

static uint32_t analyzer_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void analyzer_exit_critical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static uint32_t absolute_i32(int32_t value)
{
    return (value < 0) ? (uint32_t)(-value) : (uint32_t)value;
}

static uint32_t tim5_clock_hz(void)
{
    uint32_t clock = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_HCLK_DIV1)
    {
        clock *= 2U;
    }

    return clock;
}

static void equal_frequency_exti_disable(void)
{
    /*
     * 只关闭EXTI1中断屏蔽，不改变PA1/TIM5_CH2复用功能。
     */
    EXTI->IMR &= ~((uint32_t)GPIO_PIN_1);
}

static void equal_frequency_exti_arm(void)
{
    /*
     * 清除可能残留的边沿标志，再开放下一次上升沿中断。
     */
    EXTI->PR = GPIO_PIN_1;
    EXTI->IMR |= GPIO_PIN_1;
}

static bool equal_frequency_init(void)
{
    /*
     * 开启Cortex-M4跟踪模块和DWT周期计数器。
     * DWT CYCCNT按HCLK计数，本工程约为168MHz。
     */
    CoreDebug->DEMCR |=
        CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0U;

    DWT->CTRL |=
        DWT_CTRL_CYCCNTENA_Msk;

    if ((DWT->CTRL &
         DWT_CTRL_CYCCNTENA_Msk) == 0U)
    {
        return false;
    }

    /*
     * EXTI1选择GPIOA，即PA1。
     * EXTI1映射位位于EXTICR[0]的[7:4]。
     */
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    SYSCFG->EXTICR[0] &=
        ~((uint32_t)0x0FU << 4U);

    /*
     * 只检测上升沿。
     */
    EXTI->RTSR |= GPIO_PIN_1;
    EXTI->FTSR &= ~((uint32_t)GPIO_PIN_1);

    /*
     * 初始化时保持EXTI关闭。
     * 等精度状态机需要边沿时才临时开放。
     */
    equal_frequency_exti_disable();
    EXTI->PR = GPIO_PIN_1;

    /*
     * EXTI1优先级高于ADC DMA，减少边沿时间戳抖动。
     */
    HAL_NVIC_SetPriority(EXTI1_IRQn, 0U, 0U);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);

    s_equal_state = EQUAL_STATE_IDLE;

    s_equal_input_start = 0U;
    s_equal_input_stop = 0U;
    s_equal_reference_start = 0U;
    s_equal_reference_stop = 0U;

    s_equal_gate_start_ms = 0U;
    s_equal_stop_request_ms = 0U;

    s_equal_frequency_hz = 0.0f;
    s_equal_frequency_tick_ms = 0U;

    return true;
}

static bool equal_frequency_is_fresh(uint32_t now_ms)
{
    if (s_equal_frequency_hz <= 0.0f)
    {
        return false;
    }

    return ((now_ms -
             s_equal_frequency_tick_ms) <
            EQUAL_FREQUENCY_STALE_MS);
}

static void equal_frequency_process(void)
{
    uint32_t now_ms = HAL_GetTick();

    if (s_equal_state == EQUAL_STATE_IDLE)
    {
        /*
         * 开放EXTI，等待下一个输入上升沿作为测量起点。
         */
        s_equal_state = EQUAL_STATE_WAIT_START;
        __DMB();

        equal_frequency_exti_arm();
        return;
    }

    if (s_equal_state == EQUAL_STATE_RUNNING)
    {
        /*
         * 从真实输入起始边沿开始，等待约1秒。
         */
        if ((now_ms - s_equal_gate_start_ms) >=
            EQUAL_GATE_TIME_MS)
        {
            /*
             * 下一输入上升沿作为测量终点。
             */
            s_equal_stop_request_ms = now_ms;
            s_equal_state = EQUAL_STATE_WAIT_STOP;
            __DMB();

            equal_frequency_exti_arm();
        }

        return;
    }

    if (s_equal_state == EQUAL_STATE_WAIT_STOP)
    {
        /*
         * 请求结束后长时间没有输入边沿，说明输入可能消失。
         */
        if ((now_ms - s_equal_stop_request_ms) >=
            EQUAL_EDGE_TIMEOUT_MS)
        {
            equal_frequency_exti_disable();

            s_equal_frequency_hz = 0.0f;
            s_equal_frequency_tick_ms = now_ms;

            s_equal_state = EQUAL_STATE_IDLE;
            __DMB();
        }

        return;
    }

    if (s_equal_state == EQUAL_STATE_READY)
    {
        uint32_t input_count;
        uint32_t reference_count;

        /*
         * uint32_t无符号减法可自动处理计数器溢出。
         *
         * 1秒内：
         * DWT 约计数168000000，不会超过32位。
         */
        input_count =
            s_equal_input_stop -
            s_equal_input_start;

        reference_count =
            s_equal_reference_stop -
            s_equal_reference_start;

        if ((input_count > 0U) &&
            (reference_count > 0U))
        {
            /*
             * 等精度测频：
             *
             * Fx = Fref × Nx / Ns
             *
             * Fref：DWT参考时钟，即HCLK
             * Nx：完整输入周期数
             * Ns：同一闸门内的参考时钟计数
             */
            s_equal_frequency_hz =
                ((float32_t)HAL_RCC_GetHCLKFreq() *
                 (float32_t)input_count) /
                (float32_t)reference_count;
        }
        else
        {
            s_equal_frequency_hz = 0.0f;
        }

        s_equal_frequency_tick_ms = now_ms;

        /*
         * 下一次主循环重新等待起始边沿。
         */
        s_equal_state = EQUAL_STATE_IDLE;
        __DMB();
    }
}

static bool capture_is_fresh(uint32_t period_ticks,
                             uint32_t capture_tick_ms,
                             uint32_t now_ms)
{
    uint32_t timer_clock;
    uint64_t timeout_ms;

    if (period_ticks == 0U)
    {
        return false;
    }

    timer_clock = tim5_clock_hz();

    if (timer_clock == 0U)
    {
        return false;
    }

    /*
     * TIM5周期计数换算为毫秒，然后允许最多等待3个周期。
     *
     * timeout =
     *     period_ticks / timer_clock
     *     × 1000
     *     × CAPTURE_TIMEOUT_PERIODS
     *
     * 加上timer_clock - 1，用于整数除法向上取整。
     */
    timeout_ms =
        (uint64_t)period_ticks *
        1000U *
        CAPTURE_TIMEOUT_PERIODS;

    timeout_ms += (uint64_t)timer_clock - 1U;
    timeout_ms /= (uint64_t)timer_clock;

    /*
     * 高频信号仍然至少保留500ms有效时间，
     * 避免偶发丢失一次输入捕获就立即清零。
     */
    if (timeout_ms < CAPTURE_TIMEOUT_MIN_MS)
    {
        timeout_ms = CAPTURE_TIMEOUT_MIN_MS;
    }

    if (timeout_ms > UINT32_MAX)
    {
        timeout_ms = UINT32_MAX;
    }

    return ((now_ms - capture_tick_ms) <
            (uint32_t)timeout_ms);
}

static void update_capture_irq_divider(void)
{
    uint32_t divider;

    /*
     * ADC DMA每完成1024点产生一次半满或全满回调。
     *
     * 目标是大约每16ms复制一帧：
     *
     * divider =
     *     Fs × 16ms / 1024
     */
    divider = (uint32_t)(
        (s_sample_rate_hz *
         (float32_t)FRAME_CAPTURE_PERIOD_MS) /
        (1000.0f * (float32_t)SIGNAL_FFT_SIZE) +
        0.5f);

    if (divider < 1U)
    {
        divider = 1U;
    }

    if (divider > UINT8_MAX)
    {
        divider = UINT8_MAX;
    }

    /*
     * uint8_t在Cortex-M上可以原子写入，
     * DMA中断读取时不会得到一半的新值。
     */
    s_irq_divider_limit = (uint8_t)divider;
}

static uint32_t select_cycles_per_frame(float32_t frequency_hz)
{
    uint32_t cycles = 16U;

    /*
     * 低频时减少一帧包含的周期数，
     * 尽量让一帧采集时间不超过1秒。
     *
     * 例如：
     * 1Hz  -> 1周期
     * 5Hz  -> 4周期
     * 10Hz -> 8周期
     * 20Hz及以上 -> 默认16周期
     */
    while ((cycles > 1U) &&
           ((float32_t)cycles > frequency_hz))
    {
        cycles >>= 1U;
    }

    /*
     * 高频时增加一帧包含的周期数，
     * 从而降低每周期采样点数，保证采样率不超过1MHz。
     */
    while ((cycles < MAX_FULL_PERIOD_COUNT) &&
           (((float32_t)SIGNAL_FFT_SIZE * frequency_hz /
             (float32_t)cycles) > MAX_SAMPLE_RATE_HZ))
    {
        cycles <<= 1U;
    }

    return cycles;
}

static void program_tim2_sample_ticks(uint32_t sample_ticks)
{
    __HAL_TIM_SET_AUTORELOAD(
        &htim2,
        sample_ticks - 1U);

    __HAL_TIM_SET_COUNTER(&htim2, 0U);

    (void)HAL_TIM_GenerateEvent(
        &htim2,
        TIM_EVENTSOURCE_UPDATE);
}

static bool start_adc_dma_and_tim2(void)
{
    if (HAL_ADC_Start_DMA(&hadc1,
                          (uint32_t *)s_adc_dma,
                          ADC_DMA_SAMPLE_COUNT) != HAL_OK)
    {
        return false;
    }

    if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        /*
         * TIM2启动失败时，撤销刚启动的ADC DMA。
         */
        (void)HAL_ADC_Stop_DMA(&hadc1);
        return false;
    }

    return true;
}


static bool set_tim2_sample_ticks(uint32_t sample_ticks)
{
    uint32_t old_sample_ticks;
    uint32_t primask;

    if (sample_ticks < 2U)
    {
        sample_ticks = 2U;
    }

    /*
     * 保存旧的TIM2分频值，用于失败回滚。
     */
    old_sample_ticks =
        __HAL_TIM_GET_AUTORELOAD(&htim2) + 1U;

    if (old_sample_ticks < 2U)
    {
        old_sample_ticks = 2U;
    }

    /*
     * 先停止TIM2，确保ADC不再收到新触发。
     */
    if (HAL_TIM_Base_Stop(&htim2) != HAL_OK)
    {
        return false;
    }

    /*
     * 再停止ADC DMA。
     *
     * 如果停止失败，恢复TIM2继续使用旧采样率。
     */
    if (HAL_ADC_Stop_DMA(&hadc1) != HAL_OK)
    {
        (void)HAL_TIM_Base_Start(&htim2);
        return false;
    }

    /*
     * 采样停止后，清除旧采样率产生的等待帧。
     */
    primask = analyzer_enter_critical();

    memset((void *)s_frame_state,
           0,
           sizeof(s_frame_state));

    s_irq_divider = 0U;

    analyzer_exit_critical(primask);

    /*
     * 写入新的TIM2采样周期。
     */
    program_tim2_sample_ticks(sample_ticks);

    /*
     * 尝试用新采样率重新启动。
     */
    if (!start_adc_dma_and_tim2())
    {
        /*
         * 新采样率启动失败，恢复旧ARR并尝试恢复采样。
         */
        program_tim2_sample_ticks(old_sample_ticks);

        (void)start_adc_dma_and_tim2();

        /*
         * s_sample_rate_hz保持旧值，不更新。
         */
        return false;
    }

    /*
     * 只有新采样率成功运行后，才更新软件状态。
     */
    s_sample_rate_hz =
        (float32_t)tim5_clock_hz() /
        (float32_t)sample_ticks;

    s_result.sample_rate_hz = s_sample_rate_hz;

    update_capture_irq_divider();

    /*
     * 采样率变化后，让最小二乘立即重新计算。
     */
    s_fit_valid = 0U;

    return true;
}

static void update_adaptive_sample_rate(void)
{
    uint32_t now_ms = HAL_GetTick();
    uint32_t capture_period;
    uint32_t capture_tick;
    uint32_t timer_clock;
    uint32_t cycles;
    uint32_t minimum_sample_ticks;
    uint32_t desired_sample_ticks;
    uint32_t current_sample_ticks;
    uint32_t difference;
    uint32_t primask;
    uint64_t scaled_ticks;
    float32_t frequency_hz;

    /*
     * 最多每秒调整一次采样率。
     */
    if ((now_ms - s_last_sample_rate_update_ms) <
        SAMPLE_RATE_ADJUST_PERIOD_MS)
    {
        return;
    }

    s_last_sample_rate_update_ms = now_ms;

    /*
     * 原子读取TIM5输入捕获结果。
     */
    primask = analyzer_enter_critical();

    capture_period = s_capture_period;
    capture_tick = s_capture_tick_ms;

    analyzer_exit_critical(primask);

    /*
     * 没有有效TIM5频率时保持当前采样率不变。
     */
    if (!capture_is_fresh(capture_period,
                          capture_tick,
                          now_ms))
    {
        return;
    }

    timer_clock = tim5_clock_hz();

    frequency_hz =
        (float32_t)timer_clock /
        (float32_t)capture_period;

    cycles = select_cycles_per_frame(frequency_hz);

    /*
     * TIM2和TIM5共用APB1定时器时钟。
     *
     * TIM5周期计数 = timer_clock / 输入频率
     *
     * 为了让1024点包含整数个周期：
     *
     * sample_ticks =
     *     TIM5周期计数 × 完整周期数 / 1024
     */
    scaled_ticks =
        (uint64_t)capture_period *
        (uint64_t)cycles +
        (uint64_t)SIGNAL_FFT_SIZE / 2U;

    scaled_ticks /= (uint64_t)SIGNAL_FFT_SIZE;

    /*
     * 限制最高采样率为1MHz。
     */
    minimum_sample_ticks =
        (uint32_t)ceilf(
            (float32_t)timer_clock /
            MAX_SAMPLE_RATE_HZ);

    if (scaled_ticks < (uint64_t)minimum_sample_ticks)
    {
        scaled_ticks = minimum_sample_ticks;
    }

    if (scaled_ticks > UINT32_MAX)
    {
        scaled_ticks = UINT32_MAX;
    }

    desired_sample_ticks = (uint32_t)scaled_ticks;

    current_sample_ticks =
        __HAL_TIM_GET_AUTORELOAD(&htim2) + 1U;

    difference =
        (desired_sample_ticks > current_sample_ticks) ?
        (desired_sample_ticks - current_sample_ticks) :
        (current_sample_ticks - desired_sample_ticks);

    /*
     * 变化不到2%时不调整，避免TIM5测量抖动导致
     * TIM2采样率不断重配。
     */
    if ((float32_t)difference <
        SAMPLE_RATE_HYSTERESIS_RATIO *
        (float32_t)current_sample_ticks)
    {
        return;
    }

    (void)set_tim2_sample_ticks(desired_sample_ticks);
}

static void capture_queue_half(const uint16_t *source)
{
    uint32_t slot;

    s_irq_divider++;

    if (s_irq_divider < s_irq_divider_limit)
    {
        return;
    }

    s_irq_divider = 0U;

    for (slot = 0U; slot < FRAME_SLOT_COUNT; ++slot)
    {
        if (s_frame_state[slot] == FRAME_FREE)
        {
            memcpy(s_frames[slot], source, SIGNAL_FFT_SIZE * sizeof(uint16_t));
            s_frame_sequence[slot] = ++s_next_sequence;
            __DMB();
            s_frame_state[slot] = FRAME_READY;
            return;
        }
    }

    s_dropped_frames++;
}

static int32_t capture_take_latest(void)
{
    uint32_t primask;
    uint32_t slot;
    int32_t newest = -1;
    uint32_t newest_sequence = 0U;

    primask = analyzer_enter_critical();

    for (slot = 0U; slot < FRAME_SLOT_COUNT; ++slot)
    {
        if ((s_frame_state[slot] == FRAME_READY) &&
            ((newest < 0) || (s_frame_sequence[slot] > newest_sequence)))
        {
            newest = (int32_t)slot;
            newest_sequence = s_frame_sequence[slot];
        }
    }

    if (newest >= 0)
    {
        for (slot = 0U; slot < FRAME_SLOT_COUNT; ++slot)
        {
            if (s_frame_state[slot] == FRAME_READY)
            {
                s_frame_state[slot] = ((int32_t)slot == newest) ?
                                      FRAME_READING : FRAME_FREE;
            }
        }
    }

    analyzer_exit_critical(primask);
    return newest;
}

static void capture_release(uint32_t slot)
{
    uint32_t primask = analyzer_enter_critical();
    s_frame_state[slot] = FRAME_FREE;
    analyzer_exit_critical(primask);
}

static void analyze_waveform(const uint16_t *samples, float32_t mean)
{
    uint32_t trigger = 0U;
    uint32_t available;
    uint32_t point;
    uint32_t max_absolute = 0U;

    for (point = 32U; point < 320U; ++point)
    {
        if (((float32_t)samples[point - 1U] < mean) &&
            ((float32_t)samples[point] >= mean))
        {
            trigger = point - 24U;
            break;
        }
    }

    available = SIGNAL_FFT_SIZE - trigger;

    for (point = 0U; point < SIGNAL_WAVE_POINTS; ++point)
    {
        uint32_t begin = trigger + (point * available) / SIGNAL_WAVE_POINTS;
        uint32_t end = trigger + ((point + 1U) * available) / SIGNAL_WAVE_POINTS;
        int32_t minimum = INT32_MAX;
        int32_t maximum = INT32_MIN;
        uint32_t index;

        if (end <= begin)
        {
            end = begin + 1U;
        }
        if (end > SIGNAL_FFT_SIZE)
        {
            end = SIGNAL_FFT_SIZE;
        }

        for (index = begin; index < end; ++index)
        {
            int32_t centered = (int32_t)lroundf((float32_t)samples[index] - mean);
            if (centered < minimum)
            {
                minimum = centered;
            }
            if (centered > maximum)
            {
                maximum = centered;
            }
        }

        s_result.wave_min[point] = (int16_t)minimum;
        s_result.wave_max[point] = (int16_t)maximum;

        if (absolute_i32(minimum) > max_absolute)
        {
            max_absolute = absolute_i32(minimum);
        }
        if (absolute_i32(maximum) > max_absolute)
        {
            max_absolute = absolute_i32(maximum);
        }
    }

    max_absolute += max_absolute / 8U;
    if (max_absolute < 32U)
    {
        max_absolute = 32U;
    }
    if (max_absolute > 2048U)
    {
        max_absolute = 2048U;
    }
    s_result.wave_scale_counts = (uint16_t)max_absolute;
}

static float32_t fit_sine_amplitude(const uint16_t *samples,
                                    float32_t frequency_hz)
{
    uint32_t index;
    const float32_t count_to_volts =
        ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE_COUNTS;
    const float32_t phase_step =
        2.0f * PI * frequency_hz / s_sample_rate_hz;
    const float32_t cos_step = cosf(phase_step);
    const float32_t sin_step = sinf(phase_step);
    float32_t cosine = 1.0f;
    float32_t sine = 0.0f;
    float32_t sum_cosine = 0.0f;
    float32_t sum_sine = 0.0f;
    float32_t sum_voltage = 0.0f;
    float32_t sum_cosine_cosine = 0.0f;
    float32_t sum_sine_sine = 0.0f;
    float32_t sum_cosine_sine = 0.0f;
    float32_t sum_voltage_cosine = 0.0f;
    float32_t sum_voltage_sine = 0.0f;
    float32_t count = (float32_t)SIGNAL_FFT_SIZE;
    float32_t normal_cosine_cosine;
    float32_t normal_sine_sine;
    float32_t normal_cosine_sine;
    float32_t normal_voltage_cosine;
    float32_t normal_voltage_sine;
    float32_t determinant;
    float32_t cosine_coefficient;
    float32_t sine_coefficient;

    if ((frequency_hz <= 0.0f) ||
        (frequency_hz >= 0.5f * s_sample_rate_hz))
    {
        return 0.0f;
    }

    /*
     * 模型：
     * voltage[n] = DC
     *            + cosine_coefficient * cos(w*n)
     *            + sine_coefficient   * sin(w*n)
     *
     * 使用递推振荡器，每次拟合只调用一次 sinf() 和 cosf()。
     */
    for (index = 0U; index < SIGNAL_FFT_SIZE; ++index)
    {
        float32_t voltage =
            (float32_t)samples[index] * count_to_volts;
        float32_t next_cosine;

        sum_cosine += cosine;
        sum_sine += sine;
        sum_voltage += voltage;

        sum_cosine_cosine += cosine * cosine;
        sum_sine_sine += sine * sine;
        sum_cosine_sine += cosine * sine;

        sum_voltage_cosine += voltage * cosine;
        sum_voltage_sine += voltage * sine;

        next_cosine = cosine * cos_step - sine * sin_step;
        sine = sine * cos_step + cosine * sin_step;
        cosine = next_cosine;
    }

    /*
     * 消去拟合模型中的 DC 项，将三参数问题化为
     * cosine_coefficient、sine_coefficient 的 2×2 方程。
     */
    normal_cosine_cosine =
        sum_cosine_cosine -
        sum_cosine * sum_cosine / count;

    normal_sine_sine =
        sum_sine_sine -
        sum_sine * sum_sine / count;

    normal_cosine_sine =
        sum_cosine_sine -
        sum_cosine * sum_sine / count;

    normal_voltage_cosine =
        sum_voltage_cosine -
        sum_voltage * sum_cosine / count;

    normal_voltage_sine =
        sum_voltage_sine -
        sum_voltage * sum_sine / count;

    determinant =
        normal_cosine_cosine * normal_sine_sine -
        normal_cosine_sine * normal_cosine_sine;

    if (fabsf(determinant) <= 1.0e-12f)
    {
        return 0.0f;
    }

    cosine_coefficient =
        (normal_voltage_cosine * normal_sine_sine -
         normal_voltage_sine * normal_cosine_sine) /
        determinant;

    sine_coefficient =
        (normal_voltage_sine * normal_cosine_cosine -
         normal_voltage_cosine * normal_cosine_sine) /
        determinant;

    return sqrtf(cosine_coefficient * cosine_coefficient +
                 sine_coefficient * sine_coefficient);
}

static void analyze_fft(const uint16_t *samples, float32_t mean,float32_t fit_frequency_hz)
{
    uint32_t index;
    uint32_t peak_bin = 1U;
    float32_t peak_magnitude = 0.0f;
    float32_t interpolated_bin;

    /*
     * FFT 仍然使用去直流后的电压和 Hann 窗。
     */
    for (index = 0U; index < SIGNAL_FFT_SIZE; ++index)
    {
        float32_t voltage =
            ((float32_t)samples[index] - mean) *
            (ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE_COUNTS);

        s_fft_input[index] = voltage * s_hann[index];
    }

    arm_rfft_fast_f32(&s_fft, s_fft_input, s_fft_output, 0U);

    s_magnitude[0] = fabsf(s_fft_output[0]);

    for (index = 1U; index < SIGNAL_FFT_SIZE / 2U; ++index)
    {
        float32_t real = s_fft_output[2U * index];
        float32_t imaginary = s_fft_output[2U * index + 1U];
        float32_t magnitude =
            sqrtf(real * real + imaginary * imaginary);

        s_magnitude[index] = magnitude;

        /*
         * 忽略 DC 和第一个频点，从第二个频点开始寻找峰值。
         */
        if ((index >= 2U) && (magnitude > peak_magnitude))
        {
            peak_magnitude = magnitude;
            peak_bin = index;
        }
    }

    /*
     * 三点抛物线插值，提高 FFT 定频精度。
     */
    interpolated_bin = (float32_t)peak_bin;

    if ((peak_bin > 1U) &&
        (peak_bin < SIGNAL_FFT_SIZE / 2U - 1U))
    {
        float32_t left = s_magnitude[peak_bin - 1U];
        float32_t center = s_magnitude[peak_bin];
        float32_t right = s_magnitude[peak_bin + 1U];
        float32_t denominator =
            left - 2.0f * center + right;

        if (fabsf(denominator) > 1.0e-12f)
        {
            interpolated_bin +=
                0.5f * (left - right) / denominator;
        }
    }

    /*
     * FFT 负责定频。
     */
    s_result.fft_peak_frequency_hz =
        interpolated_bin *
		s_sample_rate_hz /
        (float32_t)SIGNAL_FFT_SIZE;

    /*
     * 正弦最小二乘幅值每秒更新一次。
     * 第一次收到有效数据时立即执行，不等待一秒。
     */
    {
        uint32_t now_ms = HAL_GetTick();

        if ((fit_frequency_hz <= 0.0f) ||
            (fit_frequency_hz >= 0.5f * s_sample_rate_hz))
        {
            /* TIM5 没有有效频率时，拟合结果清零。 */
            s_result.fft_peak_amplitude_v = 0.0f;
            s_result.peak_to_peak_v = 0.0f;
            s_fit_valid = 0U;
        }
        else if ((!s_fit_valid) ||
                 ((now_ms - s_last_fit_ms) >= SINE_FIT_PERIOD_MS))
        {
            s_last_fit_ms = now_ms;

            s_result.fft_peak_amplitude_v =
                fit_sine_amplitude(samples, fit_frequency_hz);

            s_result.peak_to_peak_v =
                2.0f * s_result.fft_peak_amplitude_v;

            s_fit_valid = 1U;
        }
    }

    /*
     * 频谱柱状图仍然按照 FFT 峰值归一化。
     */
    for (index = 0U; index < SIGNAL_SPECTRUM_BARS; ++index)
    {
        uint32_t first_bin =
            1U +
            (index * (SIGNAL_FFT_SIZE / 2U - 1U)) /
            SIGNAL_SPECTRUM_BARS;

        uint32_t last_bin =
            1U +
            ((index + 1U) *
             (SIGNAL_FFT_SIZE / 2U - 1U)) /
            SIGNAL_SPECTRUM_BARS;

        uint32_t bin;
        float32_t bar_magnitude = 0.0f;
        float32_t level_db;

        if (last_bin <= first_bin)
        {
            last_bin = first_bin + 1U;
        }

        if (last_bin > SIGNAL_FFT_SIZE / 2U)
        {
            last_bin = SIGNAL_FFT_SIZE / 2U;
        }

        for (bin = first_bin; bin < last_bin; ++bin)
        {
            if (s_magnitude[bin] > bar_magnitude)
            {
                bar_magnitude = s_magnitude[bin];
            }
        }

        if ((peak_magnitude <= 1.0e-12f) ||
            (bar_magnitude <= 1.0e-12f))
        {
            s_result.spectrum[index] = 0U;
        }
        else
        {
            level_db =
                20.0f *
                log10f(bar_magnitude / peak_magnitude);

            if (level_db < SPECTRUM_FLOOR_DB)
            {
                level_db = SPECTRUM_FLOOR_DB;
            }

            if (level_db > 0.0f)
            {
                level_db = 0.0f;
            }

            s_result.spectrum[index] =
                (uint16_t)lroundf(
                    1000.0f *
                    (level_db - SPECTRUM_FLOOR_DB) /
                    (0.0f - SPECTRUM_FLOOR_DB));
        }
    }
}

static void analyze_frame(const uint16_t *samples, uint32_t sequence)
{
    uint32_t index;
    uint16_t minimum = UINT16_MAX;
    uint16_t maximum = 0U;
    float32_t sum = 0.0f;
    float32_t square_sum = 0.0f;
    float32_t mean;
    float32_t volts_per_count = ADC_REFERENCE_VOLTAGE / ADC_FULL_SCALE_COUNTS;

    for (index = 0U; index < SIGNAL_FFT_SIZE; ++index)
    {
        uint16_t value = samples[index];
        sum += (float32_t)value;
        if (value < minimum)
        {
            minimum = value;
        }
        if (value > maximum)
        {
            maximum = value;
        }
    }
    mean = sum / (float32_t)SIGNAL_FFT_SIZE;

    for (index = 0U; index < SIGNAL_FFT_SIZE; ++index)
    {
        float32_t centered = (float32_t)samples[index] - mean;
        square_sum += centered * centered;
    }

    s_result.sample_rate_hz = s_sample_rate_hz;
    s_result.dc_voltage_v = mean * volts_per_count;
    s_result.minimum_voltage_v = (float32_t)minimum * volts_per_count;
    s_result.maximum_voltage_v = (float32_t)maximum * volts_per_count;

    s_result.rms_ac_voltage_v =
        sqrtf(square_sum / (float32_t)SIGNAL_FFT_SIZE) * volts_per_count;

    analyze_waveform(samples, mean);


    if (equal_frequency_is_fresh(HAL_GetTick()))
    {
        s_result.capture_frequency_hz =
            s_equal_frequency_hz;

        s_result.capture_duty_percent = 0.0f;
    }
    else
    {
        s_result.capture_frequency_hz = 0.0f;
        s_result.capture_duty_percent = 0.0f;
    }

    /*
     * FFT 仍负责频谱显示；
     * 最小二乘拟合使用 TIM5 输入捕获频率定频。
     */
    analyze_fft(samples, mean, s_result.capture_frequency_hz);

    s_result.dropped_frames = s_dropped_frames;
    __DMB();
    s_result.sequence = sequence;
}

bool signal_analyzer_init(void)
{
    uint32_t index;

    memset(&s_result, 0, sizeof(s_result));
    memset((void *)s_frame_state, 0, sizeof(s_frame_state));

    s_last_fit_ms = 0U;
    s_fit_valid = 0U;
    s_last_sample_rate_update_ms = 0U;

    /*
     * TIM2更新事件触发ADC采样。
     *
     * 初始配置：
     * TIM2时钟约84MHz
     * ARR = 83
     * 初始采样率 = 84MHz / (83 + 1) = 1MHz
     *
     * 这里读取实际ARR，避免把采样率写死。
     */
    {
        uint32_t sample_ticks =
            __HAL_TIM_GET_AUTORELOAD(&htim2) + 1U;

        if (sample_ticks == 0U)
        {
            return false;
        }

        s_sample_rate_hz =
            (float32_t)tim5_clock_hz() /
            (float32_t)sample_ticks;

        s_result.sample_rate_hz = s_sample_rate_hz;

        update_capture_irq_divider();

        if (!equal_frequency_init())
        {
            return false;
        }
    }

    for (index = 0U; index < SIGNAL_FFT_SIZE; ++index)
    {
        s_hann[index] =
            0.5f -
            0.5f *
            cosf((2.0f * PI * (float32_t)index) /
                 (float32_t)(SIGNAL_FFT_SIZE - 1U));
    }

    return arm_rfft_fast_init_f32(
               &s_fft,
               SIGNAL_FFT_SIZE) == ARM_MATH_SUCCESS;
}

bool signal_analyzer_start(void)
{
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_dma,
                          ADC_DMA_SAMPLE_COUNT) != HAL_OK)
    {
        return false;
    }

    /*
     * TIM5使用TI2外部时钟模式，只启动CH2，不开启中断。
     */
    __HAL_TIM_SET_COUNTER(&htim5, 0U);

    if (HAL_TIM_IC_Start(&htim5,
                         TIM_CHANNEL_2) != HAL_OK)
    {
        return false;
    }

    if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        return false;
    }

    return true;

}

void signal_analyzer_process(void)
{
    int32_t slot;

    /*
     * 每次主循环都检查，但函数内部限制为最多每秒调整一次。
     * 放在取帧之前，这样低采样率、暂时没有完整帧时，
     * 仍然可以根据TIM5频率重新调整采样率。
     */
    /*
     * 等精度测频状态机。
     */
    equal_frequency_process();

    /*
     * 下一步再让自适应采样使用等精度频率。
     */
    update_adaptive_sample_rate();

    slot = capture_take_latest();

    if (slot < 0)
    {
        return;
    }

    analyze_frame(s_frames[slot],
                  s_frame_sequence[slot]);

    capture_release((uint32_t)slot);
}

const signal_analysis_t *signal_analyzer_result(void)
{
    return &s_result;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        capture_queue_half(&s_adc_dma[0]);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        capture_queue_half(&s_adc_dma[SIGNAL_FFT_SIZE]);
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if ((htim->Instance == TIM5) &&
        (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2))
    {
        uint32_t period = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
        uint32_t high = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

        if ((period > 0U) && (high <= period))
        {
            s_capture_period = period;
            s_capture_high = high;
            s_capture_tick_ms = HAL_GetTick();
        }
    }
}

/*
 * 等精度测频边沿入口。
 * 下一步加入起始、结束计数锁存逻辑。
 */
void signal_analyzer_frequency_edge_isr(void)
{
    /*
     * 先读取参考计数器，再读取输入计数器。
     * 两次读取间只有很少的CPU周期。
     */
    uint32_t reference_count = DWT->CYCCNT;
    uint32_t input_count =
        HAL_TIM_ReadCapturedValue(
            &htim5,
            TIM_CHANNEL_2);

    if (s_equal_state == EQUAL_STATE_WAIT_START)
    {
        /*
         * 第一个输入上升沿：同时锁存输入计数和参考计数。
         */
        s_equal_reference_start = reference_count;
        s_equal_input_start = input_count;
        s_equal_gate_start_ms = HAL_GetTick();

        s_equal_state = EQUAL_STATE_RUNNING;

        /*
         * 测量窗口中关闭EXTI，高频输入不会产生中断。
         */
        equal_frequency_exti_disable();
    }
    else if (s_equal_state == EQUAL_STATE_WAIT_STOP)
    {
        /*
         * 约1秒后的第一个输入上升沿：锁存终点。
         */
        s_equal_reference_stop = reference_count;
        s_equal_input_stop = input_count;

        s_equal_state = EQUAL_STATE_READY;

        equal_frequency_exti_disable();
    }
    else
    {
        /*
         * 非预期边沿直接关闭EXTI。
         */
        equal_frequency_exti_disable();
    }

    __DMB();
}
