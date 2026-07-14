#ifndef ADC_H
#define ADC_H


#include <stdint.h>
#include <stdbool.h>


/* ADC采样点数 */
#define ADC_SAMPLE_SIZE   (1024U)



/*
 * ADC测量结果
 */
typedef struct
{

    float averageV;

    float vppV;

    float amplitudeV;

    float rmsV;


}ADC_Result;


/*
 * 外部访问
 */
extern uint16_t gADCSamples[ADC_SAMPLE_SIZE];

extern volatile bool gADCFrameReady;

extern ADC_Result adcResult;



/*
 * ADC初始化采样
 */
void ADC_StartFrame(void);


/*
 * ADC数据分析
 */
void ADC_AnalyzeSamples(void);



#endif