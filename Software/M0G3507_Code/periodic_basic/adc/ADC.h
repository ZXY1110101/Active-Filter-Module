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
 * 初始化ADC采样模块使用的DMA中断
 */
void ADC_Init(void);



/*
 * ADC初始化采样
 */
void ADC_StartFrame(void);


/*
 * 采集一帧ADC数据；函数会等待DMA完成并停止采样定时器
 */
void ADC_CaptureFrame(void);


/*
 * ADC数据分析
 */
void ADC_AnalyzeSamples(void);



#endif