#include "ADC.h"

#include "ti_msp_dl_config.h"

#include <math.h>
#include <stddef.h>

#define ADC_CODE_MASK          (0x0FFFU)
#define ADC_FULL_SCALE_CODE_U  (4095U)
#define ADC_FULL_SCALE_CODE_F  (4095.0f)
#define ADC_CLIP_MARGIN_CODE   (4U)
#define ADC_CAPTURE_TIMEOUT_LOOPS (8000000U)

/* 默认跟随 SysConfig 中 ADC MEM0 的参考电压配置。 */
#ifndef ADC_REFERENCE_V
#define ADC_REFERENCE_V ((float)ADC12_0_ADCMEM_0_REF_VOLTAGE_V)
#endif

uint16_t gADCSamples[ADC_SAMPLE_SIZE];
volatile bool gADCFrameReady = false;
ADC_Result adcResult;

/* 保存 3 点中值滤波结果，原始 DMA 波形保持不变。 */
static uint16_t gFilteredSamples[ADC_SAMPLE_SIZE];

static uint16_t ADC_Median3(uint16_t a, uint16_t b, uint16_t c);
static float ADC_RefineExtremum(uint32_t index, uint32_t count);
static void ADC_FindAccurateExtrema(
    const uint16_t *samples, uint32_t count, float *minCode, float *maxCode);
static void ADC_DMA_PrepareFrame(void);

void ADC_Init(void)
{
    NVIC_ClearPendingIRQ(DMA_INT_IRQn);
    NVIC_EnableIRQ(DMA_INT_IRQn);
}

static uint16_t ADC_Median3(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) {
        uint16_t t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        uint16_t t = b;
        b = c;
        c = t;
    }
    if (a > b) {
        b = a;
    }
    return b;
}

/*
 * 在离散极值左右各取一点做二次曲线插值，估计采样点之间的峰值。
 * 对平顶波、边界点或曲率过小的情况直接返回离散极值。
 */
static float ADC_RefineExtremum(uint32_t index, uint32_t count)
{
    float ym;
    float y0;
    float yp;
    float curvature;
    float offset;
    float refined;

    if ((index == 0U) || ((index + 1U) >= count)) {
        return (float)gFilteredSamples[index];
    }

    ym = (float)gFilteredSamples[index - 1U];
    y0 = (float)gFilteredSamples[index];
    yp = (float)gFilteredSamples[index + 1U];
    curvature = ym - (2.0f * y0) + yp;

    if (fabsf(curvature) < 1.0f) {
        return y0;
    }

    offset = (ym - yp) / (2.0f * curvature);
    if ((offset < -0.5f) || (offset > 0.5f)) {
        return y0;
    }

    refined = y0 - (((yp - ym) * (yp - ym)) / (8.0f * curvature));
    if (refined < 0.0f) {
        refined = 0.0f;
    } else if (refined > ADC_FULL_SCALE_CODE_F) {
        refined = ADC_FULL_SCALE_CODE_F;
    }
    return refined;
}

static void ADC_FindAccurateExtrema(
    const uint16_t *samples, uint32_t count, float *minCode, float *maxCode)
{
    uint16_t minimum;
    uint16_t maximum;
    uint32_t minIndex = 0U;
    uint32_t maxIndex = 0U;
    uint32_t i;

    if ((samples == NULL) || (minCode == NULL) || (maxCode == NULL) ||
        (count == 0U) || (count > ADC_SAMPLE_SIZE)) {
        if (minCode != NULL) {
            *minCode = 0.0f;
        }
        if (maxCode != NULL) {
            *maxCode = 0.0f;
        }
        return;
    }

    gFilteredSamples[0] = samples[0] & ADC_CODE_MASK;
    if (count == 1U) {
        *minCode = (float)gFilteredSamples[0];
        *maxCode = (float)gFilteredSamples[0];
        return;
    }

    for (i = 1U; i + 1U < count; i++) {
        gFilteredSamples[i] = ADC_Median3(
            samples[i - 1U] & ADC_CODE_MASK,
            samples[i] & ADC_CODE_MASK,
            samples[i + 1U] & ADC_CODE_MASK);
    }
    gFilteredSamples[count - 1U] = samples[count - 1U] & ADC_CODE_MASK;

    /*
     * 忽略帧首尾点寻找极值，避免无法进行插值；3 点中值滤波会消除
     * 单采样点毛刺，但不会像“顶部/底部 16 点平均”那样压低幅值。
     */
    minimum = gFilteredSamples[1U];
    maximum = gFilteredSamples[1U];
    minIndex = 1U;
    maxIndex = 1U;
    for (i = 2U; i + 1U < count; i++) {
        if (gFilteredSamples[i] < minimum) {
            minimum = gFilteredSamples[i];
            minIndex = i;
        }
        if (gFilteredSamples[i] > maximum) {
            maximum = gFilteredSamples[i];
            maxIndex = i;
        }
    }

    *minCode = ADC_RefineExtremum(minIndex, count);
    *maxCode = ADC_RefineExtremum(maxIndex, count);
    if (*maxCode < *minCode) {
        *maxCode = *minCode;
    }
}

void ADC_AnalyzeSamples(void)
{
    uint32_t sumCode = 0U;
    float squareSum = 0.0f;
    float averageCode;
    float minCode;
    float maxCode;
    float centerCode;
    uint32_t i;

    for (i = 0U; i < ADC_SAMPLE_SIZE; i++) {
        sumCode += gADCSamples[i] & ADC_CODE_MASK;
    }
    averageCode = (float)sumCode / (float)ADC_SAMPLE_SIZE;

    for (i = 0U; i < ADC_SAMPLE_SIZE; i++) {
        float acCode = (float)(gADCSamples[i] & ADC_CODE_MASK) - averageCode;
        squareSum += acCode * acCode;
    }

    ADC_FindAccurateExtrema(
        gADCSamples, ADC_SAMPLE_SIZE, &minCode, &maxCode);
    centerCode = (minCode + maxCode) * 0.5f;

    adcResult.averageV =
        averageCode * ADC_REFERENCE_V / ADC_FULL_SCALE_CODE_F;
    adcResult.minV = minCode * ADC_REFERENCE_V / ADC_FULL_SCALE_CODE_F;
    adcResult.maxV = maxCode * ADC_REFERENCE_V / ADC_FULL_SCALE_CODE_F;
    adcResult.centerV = centerCode * ADC_REFERENCE_V / ADC_FULL_SCALE_CODE_F;
    adcResult.vppV = adcResult.maxV - adcResult.minV;
    adcResult.amplitudeV = adcResult.vppV * 0.5f;
    adcResult.rmsV =
        sqrtf(squareSum / (float)ADC_SAMPLE_SIZE) *
        ADC_REFERENCE_V / ADC_FULL_SCALE_CODE_F;

    adcResult.minCode = (uint16_t)(minCode + 0.5f);
    adcResult.maxCode = (uint16_t)(maxCode + 0.5f);
    adcResult.centerCode = (uint16_t)(centerCode + 0.5f);
    adcResult.clipped =
        (adcResult.minCode <= ADC_CLIP_MARGIN_CODE) ||
        (adcResult.maxCode >= (ADC_FULL_SCALE_CODE_U - ADC_CLIP_MARGIN_CODE));
}

static void ADC_DMA_PrepareFrame(void)
{
    gADCFrameReady = false;

    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
    DL_DMA_setSrcAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        DL_ADC12_getMemResultAddress(ADC12_0_INST, DL_ADC12_MEM_IDX_0));
    DL_DMA_setDestAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t)&gADCSamples[0]);
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, ADC_SAMPLE_SIZE);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
}

void ADC_StartFrame(void)
{
    ADC_DMA_PrepareFrame();
    DL_ADC12_startConversion(ADC12_0_INST);
    DL_TimerG_startCounter(TIMER_0_INST);
}

bool ADC_CaptureFrame(void)
{
    uint32_t timeout = ADC_CAPTURE_TIMEOUT_LOOPS;

    ADC_StartFrame();

    while ((!gADCFrameReady) && (timeout > 0U)) {
        timeout--;
        __NOP();
    }

    DL_TimerG_stopCounter(TIMER_0_INST);
    DL_ADC12_stopConversion(ADC12_0_INST);

    if (!gADCFrameReady) {
        DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
        DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);
        NVIC_ClearPendingIRQ(DMA_INT_IRQn);
        return false;
    }

    return true;
}

void DMA_IRQHandler(void)
{
    switch (DL_DMA_getPendingInterrupt(DMA)) {
        case DL_DMA_EVENT_IIDX_DMACH0:
            gADCFrameReady = true;
            break;

        default:
            break;
    }
}
