#ifndef UART_H
#define UART_H


#include <stdint.h>


/*
 * UART初始化
 *
 * 当前由SYSCFG完成，
 * 保留接口方便以后扩展
 */
void UART_Init(void);



/*
 * 发送一个字符
 */
void UART_SendChar(char c);



/*
 * 发送字符串
 */
void UART_SendString(const char *str);



/*
 * 发送浮点数
 *
 * 例如:
 * 1.234
 */
void UART_SendFloat(float value);



/*
 * 输出ADC测量结果
 */
void UART_PrintADCResult(
        float average,
        float vpp,
        float rms);


#endif