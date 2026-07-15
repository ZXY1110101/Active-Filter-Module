#include "DAC.h"

#include "ti_msp_dl_config.h"

#define DAC_CENTER_WINDOW_SIZE      (5U)
#define DAC_UPDATE_FRAME_DIVIDER    (4U)
#define DAC_DEADBAND_CODE           (8)
#define DAC_FAST_TRACK_THRESHOLD    (64)

static uint16_t gDACCode = DAC_MID_SCALE_CODE;
static uint16_t gCenterWindow[DAC_CENTER_WINDOW_SIZE];
static uint8_t gCenterCount = 0U;
static uint8_t gCenterIndex = 0U;
static uint8_t gUpdateFrameCount = 0U;

static uint16_t DAC_MedianCenter(void);

void DAC_Init(void)
{
    gCenterCount = 0U;
    gCenterIndex = 0U;
    gUpdateFrameCount = 0U;
    DAC_SetCode(DAC_MID_SCALE_CODE);
}

void DAC_SetCode(uint16_t code)
{
    if (code > DAC_FULL_SCALE_CODE) {
        code = DAC_FULL_SCALE_CODE;
    }

    gDACCode = code;
    DL_DAC12_output12(DAC0, code);
}

uint16_t DAC_GetCode(void)
{
    return gDACCode;
}

static uint16_t DAC_MedianCenter(void)
{
    uint16_t sorted[DAC_CENTER_WINDOW_SIZE];
    uint8_t i;
    uint8_t j;

    for (i = 0U; i < gCenterCount; i++) {
        sorted[i] = gCenterWindow[i];
    }

    for (i = 1U; i < gCenterCount; i++) {
        uint16_t value = sorted[i];
        j = i;
        while ((j > 0U) && (sorted[j - 1U] > value)) {
            sorted[j] = sorted[j - 1U];
            j--;
        }
        sorted[j] = value;
    }

    return sorted[gCenterCount / 2U];
}

void DAC_TrackReference(uint16_t targetCode, bool valid)
{
    uint16_t medianCode;
    int32_t error;
    int32_t step;
    int32_t nextCode;

    if (!valid) {
        return;
    }
    if (targetCode > DAC_FULL_SCALE_CODE) {
        targetCode = DAC_FULL_SCALE_CODE;
    }

    gCenterWindow[gCenterIndex] = targetCode;
    gCenterIndex++;
    if (gCenterIndex >= DAC_CENTER_WINDOW_SIZE) {
        gCenterIndex = 0U;
    }
    if (gCenterCount < DAC_CENTER_WINDOW_SIZE) {
        gCenterCount++;
    }

    /* 第一帧快速建立正确阈值，后续才进入慢速稳定跟踪。 */
    if (gCenterCount == 1U) {
        DAC_SetCode(targetCode);
        return;
    }

    gUpdateFrameCount++;
    if (gUpdateFrameCount < DAC_UPDATE_FRAME_DIVIDER) {
        return;
    }
    gUpdateFrameCount = 0U;

    medianCode = DAC_MedianCenter();
    error = (int32_t)medianCode - (int32_t)gDACCode;
    if ((error >= -DAC_DEADBAND_CODE) && (error <= DAC_DEADBAND_CODE)) {
        return;
    }

    if ((error >= DAC_FAST_TRACK_THRESHOLD) ||
        (error <= -DAC_FAST_TRACK_THRESHOLD)) {
        step = error / 2;
    } else {
        step = error / 8;
    }
    if (step == 0) {
        step = (error > 0) ? 1 : -1;
    }

    nextCode = (int32_t)gDACCode + step;
    if (nextCode < 0) {
        nextCode = 0;
    } else if (nextCode > (int32_t)DAC_FULL_SCALE_CODE) {
        nextCode = DAC_FULL_SCALE_CODE;
    }
    DAC_SetCode((uint16_t)nextCode);
}
