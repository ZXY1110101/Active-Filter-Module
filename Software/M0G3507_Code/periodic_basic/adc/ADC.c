#include "ADC.h"
#include "ti_msp_dl_config.h"

#include <math.h>
#include <stdlib.h>


/* ================= 参数 ================= */


#define ADC_REFERENCE_V       (3.3f)

#define ADC_FULL_SCALE_CODE   (4095.0f)


/* 稳健峰值平均数量 */
#define PEAK_AVG_COUNT        (16U)



/* ================= 全局变量 ================= */


/*
 * ADC采样缓冲区
 */
uint16_t gADCSamples[ADC_SAMPLE_SIZE];



/*
 * 排序缓存
 */
static uint16_t gSortBuffer[ADC_SAMPLE_SIZE];



/*
 * DMA完成标志
 */
volatile bool gADCFrameReady = false;



/*
 * ADC最终结果
 */
ADC_Result adcResult;



/* ================= 内部函数声明 ================= */


static int CompareUint16(
        const void *a,
        const void *b);



static float ADC_CalculateRobustVppCode(
        const uint16_t *samples,
        uint32_t count);



static void ADC_DMA_PrepareFrame(void);


/* ================= 模块初始化 ================= */


void ADC_Init(void)
{
    NVIC_ClearPendingIRQ(DMA_INT_IRQn);
    NVIC_EnableIRQ(DMA_INT_IRQn);
}




/* ================= 排序函数 ================= */


static int CompareUint16(
        const void *a,
        const void *b)
{

    uint16_t va =
            *(const uint16_t *)a;


    uint16_t vb =
            *(const uint16_t *)b;



    if(va < vb)
    {
        return -1;
    }


    if(va > vb)
    {
        return 1;
    }


    return 0;

}




/* ================= 稳健Vpp计算 ================= */


static float ADC_CalculateRobustVppCode(
        const uint16_t *samples,
        uint32_t count)
{


    if(samples == NULL ||
       count < 2 * PEAK_AVG_COUNT)
    {
        return 0.0f;
    }



    /*
     * 复制数据
     */
    for(uint32_t i=0;i<count;i++)
    {

        gSortBuffer[i]
                = samples[i] & 0x0FFFU;

    }



    /*
     * 排序
     */
    qsort(
        gSortBuffer,
        count,
        sizeof(uint16_t),
        CompareUint16);



    uint32_t lowSum = 0;

    uint32_t highSum = 0;



    for(uint32_t i=0;i<PEAK_AVG_COUNT;i++)
    {

        lowSum += gSortBuffer[i];


        highSum +=
            gSortBuffer[count-1-i];

    }



    float lowCode =
            (float)lowSum /
            PEAK_AVG_COUNT;



    float highCode =
            (float)highSum /
            PEAK_AVG_COUNT;



    return highCode-lowCode;

}




/* ================= 数据分析 ================= */


void ADC_AnalyzeSamples(void)
{

    uint32_t sumCode=0;



    /*
     * 平均值
     */
    for(uint32_t i=0;
        i<ADC_SAMPLE_SIZE;
        i++)
    {

        sumCode +=
            gADCSamples[i]&0x0FFFU;

    }



    float averageCode =
            (float)sumCode /
            ADC_SAMPLE_SIZE;



    /*
     * RMS
     */

    float squareSum=0;



    for(uint32_t i=0;
        i<ADC_SAMPLE_SIZE;
        i++)
    {


        float acCode =
            (float)(gADCSamples[i]&0x0FFFU)
            - averageCode;



        squareSum +=
            acCode*acCode;

    }



    float rmsCode =
        sqrtf(
            squareSum /
            ADC_SAMPLE_SIZE);



    /*
     * Vpp
     */

    float vppCode =
        ADC_CalculateRobustVppCode(
            gADCSamples,
            ADC_SAMPLE_SIZE);



    /*
     * 转换电压
     */

    adcResult.averageV =
        averageCode *
        ADC_REFERENCE_V /
        ADC_FULL_SCALE_CODE;



    adcResult.vppV =
        vppCode *
        ADC_REFERENCE_V /
        ADC_FULL_SCALE_CODE;



    adcResult.amplitudeV =
        adcResult.vppV/2.0f;



    adcResult.rmsV =
        rmsCode *
        ADC_REFERENCE_V /
        ADC_FULL_SCALE_CODE;



}





/* ================= DMA准备 ================= */


static void ADC_DMA_PrepareFrame(void)
{


    gADCFrameReady=false;



    DL_DMA_disableChannel(
        DMA,
        DMA_CH0_CHAN_ID);



    /*
     * ADC MEM0作为DMA源
     */

    DL_DMA_setSrcAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        DL_ADC12_getMemResultAddress(
            ADC12_0_INST,
            DL_ADC12_MEM_IDX_0));



    /*
     * buffer作为目标
     */

    DL_DMA_setDestAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t)&gADCSamples[0]);



    /*
     * 1024次传输
     */

    DL_DMA_setTransferSize(
        DMA,
        DMA_CH0_CHAN_ID,
        ADC_SAMPLE_SIZE);



    DL_DMA_enableChannel(
        DMA,
        DMA_CH0_CHAN_ID);


}




/* ================= 启动采样 ================= */


void ADC_StartFrame(void)
{


    ADC_DMA_PrepareFrame();



    /*
     * ADC等待Timer触发
     */

    DL_ADC12_startConversion(
        ADC12_0_INST);



    /*
     * 开启采样Timer
     */

    DL_TimerG_startCounter(
        TIMER_0_INST);



}





/* ================= 完整帧采集 ================= */


void ADC_CaptureFrame(void)
{
    ADC_StartFrame();

    while (!gADCFrameReady)
    {
        __WFE();
    }

    DL_TimerG_stopCounter(TIMER_0_INST);
    DL_ADC12_stopConversion(ADC12_0_INST);
}




/* ================= DMA中断 ================= */


void DMA_IRQHandler(void)
{

    switch(
        DL_DMA_getPendingInterrupt(DMA))
    {


        case DL_DMA_EVENT_IIDX_DMACH0:


            gADCFrameReady=true;


            break;



        default:

            break;

    }

}