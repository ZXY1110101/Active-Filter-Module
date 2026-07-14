#ifndef PGA_H
#define PGA_H


#include <stdint.h>


/*
 * 增益等级
 *
 * 0 : ×1
 * 1 : ×2
 * 2 : ×5
 * 3 : ×10
 */
typedef enum
{

    PGA_GAIN_X1 = 0,

    PGA_GAIN_X2,

    PGA_GAIN_X5,

    PGA_GAIN_X10


}PGA_Gain;



/*
 * 初始化PGA
 */
void PGA_Init(void);



/*
 * 设置增益
 */
void PGA_SetGain(PGA_Gain gain);



/*
 * 获取当前增益
 */
PGA_Gain PGA_GetGain(void);



/*
 * 按键扫描
 *
 * 返回true表示档位变化
 */
uint8_t PGA_KeyScan(void);



/*
 * LED显示
 */
void PGA_LED_Show(void);



#endif