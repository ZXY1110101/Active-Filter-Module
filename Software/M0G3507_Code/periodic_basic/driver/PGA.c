#include "PGA.h"

#include "ti_msp_dl_config.h"



#define LED_FAST_DELAY   (3200000)    
#define LED_SLOW_DELAY   (16000000)   
#define LED_DOUBLE_GAP   (6400000)



static PGA_Gain currentGain = PGA_GAIN_X1;



/*
 * ADG704增益设置
 *
 * A1 A0:
 *
 * 00 -> ×1
 * 01 -> ×2
 * 10 -> ×5
 * 11 -> ×10
 *
 */
void PGA_SetGain(PGA_Gain gain)
{

    currentGain = gain;


    switch(gain)
    {


        case PGA_GAIN_X1:

            // A1=0 A0=0

            DL_GPIO_clearPins(
                ADG704_A0_PORT,
                ADG704_A0_PIN_1_PIN);


            DL_GPIO_clearPins(
                ADG704_A1_PORT,
                ADG704_A1_PIN_2_PIN);


            break;



        case PGA_GAIN_X2:

            // A1=0 A0=1

            DL_GPIO_setPins(
                ADG704_A0_PORT,
                ADG704_A0_PIN_1_PIN);


            DL_GPIO_clearPins(
                ADG704_A1_PORT,
                ADG704_A1_PIN_2_PIN);


            break;



        case PGA_GAIN_X5:

            // A1=1 A0=0

            DL_GPIO_clearPins(
                ADG704_A0_PORT,
                ADG704_A0_PIN_1_PIN);


            DL_GPIO_setPins(
                ADG704_A1_PORT,
                ADG704_A1_PIN_2_PIN);


            break;



        case PGA_GAIN_X10:

            // A1=1 A0=1

            DL_GPIO_setPins(
                ADG704_A0_PORT,
                ADG704_A0_PIN_1_PIN);


            DL_GPIO_setPins(
                ADG704_A1_PORT,
                ADG704_A1_PIN_2_PIN);


            break;



        default:

            break;

    }

}




/*
 * PGA初始化
 */
void PGA_Init(void)
{

    /*
     * ADG704使能
     */

    DL_GPIO_setPins(
        ADG704_EN_PORT,
        ADG704_EN_PIN_0_PIN);



    /*
     * 默认×1
     */

    PGA_SetGain(
        PGA_GAIN_X1);


}





/*
 * 获取当前增益
 */
PGA_Gain PGA_GetGain(void)
{

    return currentGain;

}




/*
 * 按键扫描
 *
 * 按键低电平有效
 */
uint8_t PGA_KeyScan(void)
{

    static uint8_t lastState = 1;


    uint8_t nowState;



    if(DL_GPIO_readPins(
        GPIO_KEY_PORT,
        GPIO_KEY_PIN_3_PIN))
    {
        nowState = 1;
    }
    else
    {
        nowState = 0;
    }



    /*
     * 检测下降沿
     */
    if(lastState == 1 && nowState == 0)
    {


        delay_cycles(160000);



        if(!DL_GPIO_readPins(
            GPIO_KEY_PORT,
            GPIO_KEY_PIN_3_PIN))
        {


            uint8_t nextGain =
                (uint8_t)currentGain + 1;



            if(nextGain >= 4)
            {
                nextGain = 0;
            }



            PGA_SetGain(
                (PGA_Gain)nextGain);



            lastState = 0;


            return 1;

        }

    }



    if(nowState == 1)
    {
        lastState = 1;
    }



    return 0;

}





/*
 * LED显示当前档位
 *
 * ×1 常亮
 * ×2 快闪
 * ×5 慢闪
 * ×10 双闪
 */
void PGA_LED_Show(void)
{


    switch(currentGain)
    {


        case PGA_GAIN_X1:


            DL_GPIO_setPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);


            break;



        case PGA_GAIN_X2:


            DL_GPIO_togglePins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);


            delay_cycles(
                LED_FAST_DELAY);


            break;



        case PGA_GAIN_X5:


            DL_GPIO_togglePins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);


            delay_cycles(
                LED_SLOW_DELAY);


            break;



        case PGA_GAIN_X10:


            DL_GPIO_setPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);



            delay_cycles(
                LED_FAST_DELAY);



            DL_GPIO_clearPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);



            delay_cycles(
                LED_FAST_DELAY);



            DL_GPIO_setPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);



            delay_cycles(
                LED_FAST_DELAY);



            DL_GPIO_clearPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);



            delay_cycles(
                LED_DOUBLE_GAP);



            break;



        default:

            break;


    }


}
