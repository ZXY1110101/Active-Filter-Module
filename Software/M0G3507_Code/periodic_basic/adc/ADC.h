#ifndef ADC_H
#define ADC_H

#include <stdbool.h>
#include <stdint.h>

/* ADC 采样点数 */
#define ADC_SAMPLE_SIZE (1024U)

typedef struct
{
    float averageV;
    float vppV;
    float amplitudeV;
    float rmsV;

    /* 去毛刺并插值后的真实极值和中点。 */
    float maxV;
    float minV;
    float centerV;
    uint16_t maxCode;
    uint16_t minCode;
    uint16_t centerCode;

    /* 接近 0 或满量程时置位，此时极值已经被 ADC 削顶。 */
    bool clipped;
} ADC_Result;

extern uint16_t gADCSamples[ADC_SAMPLE_SIZE];
extern volatile bool gADCFrameReady;
extern ADC_Result adcResult;

void ADC_Init(void);
void ADC_StartFrame(void);
bool ADC_CaptureFrame(void);
void ADC_AnalyzeSamples(void);

#endif
