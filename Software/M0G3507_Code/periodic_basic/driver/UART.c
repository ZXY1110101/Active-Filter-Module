#include "UART.h"

#include "ti_msp_dl_config.h"

#include <stdio.h>



/*
 * UART初始化
 *
 * MSPM0中UART由SysConfig配置
 */
void UART_Init(void)
{

}



/*
 * 发送字符
 */
void UART_SendChar(char c)
{

    while(DL_UART_isBusy(
            UART_0_INST))
    {

    }


    DL_UART_Main_transmitDataBlocking(
            UART_0_INST,
            (uint8_t)c);

}



/*
 * 发送字符串
 */
void UART_SendString(const char *str)
{

    while(*str)
    {

        UART_SendChar(*str);

        str++;

    }

}



/*
 * 发送浮点
 */
void UART_SendFloat(float value)
{

    char buffer[32];


    snprintf(
        buffer,
        sizeof(buffer),
        "%.3f",
        value);



    UART_SendString(buffer);

}




/*
 * 输出ADC结果
 */
void UART_PrintADCResult(
        float average,
        float vpp,
        float rms)
{


    UART_SendString(
        "\r\n======== ADC ========\r\n");


    UART_SendString(
        "AVG: ");

    UART_SendFloat(
        average);


    UART_SendString(
        " V\r\n");



    UART_SendString(
        "VPP: ");

    UART_SendFloat(
        vpp);


    UART_SendString(
        " V\r\n");



    UART_SendString(
        "RMS: ");

    UART_SendFloat(
        rms);


    UART_SendString(
        " V\r\n");



    UART_SendString(
        "=====================\r\n");


}