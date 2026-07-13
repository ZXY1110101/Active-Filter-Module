/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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

// #include "ti_msp_dl_config.h"

// /* This results in approximately 0.5s of delay assuming 32MHz CPU_CLK */
// #define DELAY (16000000)

// int main(void)
// {
//     /* Power on GPIO, initialize pins as digital outputs */
//     SYSCFG_DL_init();

//   while (1)
//     {
//         DL_GPIO_togglePins(GPIO_LEDS_PORT,
//                            GPIO_LEDS_USER_LED_PIN);
//         delay_cycles(DELAY);
//     }
// }
// #include "ti_msp_dl_config.h"


// #define KEY_DELAY   (320000)


// uint8_t pga_level = 0;



// /*
//  * ADG704档位控制
//  *
//  * 00  ×1
//  * 01  ×2
//  * 10  ×5
//  * 11  ×10
//  */
// void PGA_SetLevel(uint8_t level)
// {

//     switch(level)
//     {

//         case 0:     //00

//             DL_GPIO_clearPins(
//                 ADG704_A1_PORT,
//                 ADG704_A1_PIN_2_PIN);

//             DL_GPIO_clearPins(
//                 ADG704_A0_PORT,
//                 ADG704_A0_PIN_1_PIN);

//             break;



//         case 1:     //01

//             DL_GPIO_clearPins(
//                 ADG704_A1_PORT,
//                 ADG704_A1_PIN_2_PIN);

//             DL_GPIO_setPins(
//                 ADG704_A0_PORT,
//                 ADG704_A0_PIN_1_PIN);

//             break;



//         case 2:     //10

//             DL_GPIO_setPins(
//                 ADG704_A1_PORT,
//                 ADG704_A1_PIN_2_PIN);

//             DL_GPIO_clearPins(
//                 ADG704_A0_PORT,
//                 ADG704_A0_PIN_1_PIN);

//             break;



//         case 3:     //11

//             DL_GPIO_setPins(
//                 ADG704_A1_PORT,
//                 ADG704_A1_PIN_2_PIN);

//             DL_GPIO_setPins(
//                 ADG704_A0_PORT,
//                 ADG704_A0_PIN_1_PIN);

//             break;


//         default:
//             break;

//     }

// }




// /*
//  * LED显示档位
//  */
// void LED_ShowLevel(uint8_t level)
// {

//     for(uint8_t i=0;i<=level;i++)
//     {

//         DL_GPIO_togglePins(
//             GPIO_LEDS_PORT,
//             GPIO_LEDS_USER_LED_PIN);


//         delay_cycles(1600000);


//         DL_GPIO_togglePins(
//             GPIO_LEDS_PORT,
//             GPIO_LEDS_USER_LED_PIN);


//         delay_cycles(1600000);

//     }

// }



// /*
//  * 按键扫描
//  */
// void KEY_Scan(void)
// {

//     if(
//     DL_GPIO_readPins(
//         GPIO_KEY_PORT,
//         GPIO_KEY_PIN_3_PIN)==0)
//     {


//         delay_cycles(KEY_DELAY);



//         if(
//         DL_GPIO_readPins(
//             GPIO_KEY_PORT,
//             GPIO_KEY_PIN_3_PIN)==0)
//         {


//             pga_level++;


//             if(pga_level>=4)
//             {
//                 pga_level=0;
//             }


//             PGA_SetLevel(
//                 pga_level);



//             LED_ShowLevel(
//                 pga_level);



//             while(
//                 DL_GPIO_readPins(
//                     GPIO_KEY_PORT,
//                     GPIO_KEY_PIN_3_PIN)==0)
//                 {
//                     ;
//                 }


//             delay_cycles(KEY_DELAY);

//         }

//     }

// }




// int main(void)
// {

//     SYSCFG_DL_init();



//     /*
//      * ADG704使能
//      *
//      * 先假设EN高有效
//      */
//     DL_GPIO_setPins(
//         ADG704_EN_PORT,
//         ADG704_EN_PIN_0_PIN);



//     /*
//      * 初始×1
//      */
//     pga_level=0;

//     PGA_SetLevel(
//         pga_level);



//     LED_ShowLevel(
//         pga_level);



//     while(1)
//     {

//         KEY_Scan();

//     }

// }

#include "ti_msp_dl_config.h"


#define LED_FAST_DELAY   (3200000)    //100ms
#define LED_SLOW_DELAY   (16000000)   //500ms
#define LED_DOUBLE_GAP   (6400000)    //200ms


uint8_t gain_level = 0;


/*
 * ADG704控制
 *
 * gain:
 * 0 -> ×1
 * 1 -> ×2
 * 2 -> ×5
 * 3 -> ×10
 */
void ADG704_SetGain(uint8_t gain)
{

    switch(gain)
    {

        case 0:     //×1
            DL_GPIO_clearPins(
                ADG704_A0_PORT,
                ADG704_A0_PIN_1_PIN);

            DL_GPIO_clearPins(
                ADG704_A1_PORT,
                ADG704_A1_PIN_2_PIN);

            break;


        case 1:     //×2

            DL_GPIO_setPins(
                ADG704_A0_PORT,
                ADG704_A0_PIN_1_PIN);

            DL_GPIO_clearPins(
                ADG704_A1_PORT,
                ADG704_A1_PIN_2_PIN);

            break;



        case 2:     //×5

            DL_GPIO_clearPins(
                ADG704_A0_PORT,
                ADG704_A0_PIN_1_PIN);

            DL_GPIO_setPins(
                ADG704_A1_PORT,
                ADG704_A1_PIN_2_PIN);

            break;



        case 3:     //×10

            DL_GPIO_setPins(
                ADG704_A0_PORT,
                ADG704_A0_PIN_1_PIN);

            DL_GPIO_setPins(
                ADG704_A1_PORT,
                ADG704_A1_PIN_2_PIN);

            break;


    }


}


/*
 * LED显示当前档位
 *
 * ×1 : 常亮
 * ×2 : 快闪
 * ×5 : 慢闪
 * ×10: 双闪
 */
void LED_ShowGain(uint8_t gain)
{

    switch(gain)
    {


        case 0:     //×1 常亮

            DL_GPIO_setPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);

            break;



        case 1:     //×2 快闪


            DL_GPIO_togglePins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);

            delay_cycles(LED_FAST_DELAY);


            break;



        case 2:     //×5 慢闪


            DL_GPIO_togglePins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);

            delay_cycles(LED_SLOW_DELAY);


            break;



        case 3:     //×10 双闪


            DL_GPIO_setPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);


            delay_cycles(LED_FAST_DELAY);


            DL_GPIO_clearPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);


            delay_cycles(LED_FAST_DELAY);



            DL_GPIO_setPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);


            delay_cycles(LED_FAST_DELAY);


            DL_GPIO_clearPins(
                GPIO_LEDS_PORT,
                GPIO_LEDS_USER_LED_PIN);


            delay_cycles(LED_SLOW_DELAY);


            break;


    }

}




int main(void)
{

    SYSCFG_DL_init();



    /*
     * ADG704使能
     */
    DL_GPIO_setPins(
        ADG704_EN_PORT,
        ADG704_EN_PIN_0_PIN);



    /*
     * 初始×1
     */
    gain_level = 0;

    ADG704_SetGain(gain_level);



    while(1)
    {


        /*
         * 按键低电平触发
         */

        if(!DL_GPIO_readPins(
            GPIO_KEY_PORT,
            GPIO_KEY_PIN_3_PIN))
        {


            delay_cycles(160000);   //5ms消抖



            if(!DL_GPIO_readPins(
                GPIO_KEY_PORT,
                GPIO_KEY_PIN_3_PIN))
            {


                gain_level++;


                if(gain_level>=4)
                    gain_level=0;



                ADG704_SetGain(gain_level);



                /*
                 * 等待松手
                 */
                while(!DL_GPIO_readPins(
                    GPIO_KEY_PORT,
                    GPIO_KEY_PIN_3_PIN))
                {

                }


            }


        }



        LED_ShowGain(gain_level);


    }

}


