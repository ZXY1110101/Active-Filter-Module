/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
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

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define GPIO_HFXT_PORT                                                     GPIOA
#define GPIO_HFXIN_PIN                                             DL_GPIO_PIN_5
#define GPIO_HFXIN_IOMUX                                         (IOMUX_PINCM10)
#define GPIO_HFXOUT_PIN                                            DL_GPIO_PIN_6
#define GPIO_HFXOUT_IOMUX                                        (IOMUX_PINCM11)
#define CPUCLK_FREQ                                                     80000000
/* Defines for SYSPLL_ERR_01 Workaround */
/* Represent 1.000 as 1000 */
#define FLOAT_TO_INT_SCALE                                               (1000U)
#define FCC_EXPECTED_RATIO                                                  2500
#define FCC_UPPER_BOUND                       (FCC_EXPECTED_RATIO * (1 + 0.003))
#define FCC_LOWER_BOUND                       (FCC_EXPECTED_RATIO * (1 - 0.003))

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);


/* Defines for CAPTURE_0 */
#define CAPTURE_0_INST                                                  (TIMG12)
#define CAPTURE_0_INST_IRQHandler                              TIMG12_IRQHandler
#define CAPTURE_0_INST_INT_IRQN                                (TIMG12_INT_IRQn)
#define CAPTURE_0_INST_LOAD_VALUE                                    (79999999U)
/* GPIO defines for channel 0 */
#define GPIO_CAPTURE_0_C0_PORT                                             GPIOB
#define GPIO_CAPTURE_0_C0_PIN                                     DL_GPIO_PIN_13
#define GPIO_CAPTURE_0_C0_IOMUX                                  (IOMUX_PINCM30)
#define GPIO_CAPTURE_0_C0_IOMUX_FUNC                IOMUX_PINCM30_PF_TIMG12_CCP0





/* Defines for TIMER_0 */
#define TIMER_0_INST                                                     (TIMG7)
#define TIMER_0_INST_IRQHandler                                 TIMG7_IRQHandler
#define TIMER_0_INST_INT_IRQN                                   (TIMG7_INT_IRQn)
#define TIMER_0_INST_LOAD_VALUE                                            (88U)
#define TIMER_0_INST_PUB_0_CH                                                (1)
/* Defines for TIMER_GATE */
#define TIMER_GATE_INST                                                  (TIMA1)
#define TIMER_GATE_INST_IRQHandler                              TIMA1_IRQHandler
#define TIMER_GATE_INST_INT_IRQN                                (TIMA1_INT_IRQn)
#define TIMER_GATE_INST_LOAD_VALUE                                      (31249U)



/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           40000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                        DL_GPIO_PIN_11
#define GPIO_UART_0_TX_PIN                                        DL_GPIO_PIN_10
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM22)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM21)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM21_PF_UART0_TX
#define UART_0_BAUD_RATE                                                (115200)
#define UART_0_IBRD_40_MHZ_115200_BAUD                                      (21)
#define UART_0_FBRD_40_MHZ_115200_BAUD                                      (45)




/* Defines for LCD_SPI */
#define LCD_SPI_INST                                                       SPI1
#define LCD_SPI_INST_IRQHandler                                 SPI1_IRQHandler
#define LCD_SPI_INST_INT_IRQN                                     SPI1_INT_IRQn
#define GPIO_LCD_SPI_PICO_PORT                                            GPIOA
#define GPIO_LCD_SPI_PICO_PIN                                    DL_GPIO_PIN_18
#define GPIO_LCD_SPI_IOMUX_PICO                                 (IOMUX_PINCM40)
#define GPIO_LCD_SPI_IOMUX_PICO_FUNC                 IOMUX_PINCM40_PF_SPI1_PICO
#define GPIO_LCD_SPI_POCI_PORT                                            GPIOA
#define GPIO_LCD_SPI_POCI_PIN                                    DL_GPIO_PIN_16
#define GPIO_LCD_SPI_IOMUX_POCI                                 (IOMUX_PINCM38)
#define GPIO_LCD_SPI_IOMUX_POCI_FUNC                 IOMUX_PINCM38_PF_SPI1_POCI
/* GPIO configuration for LCD_SPI */
#define GPIO_LCD_SPI_SCLK_PORT                                            GPIOA
#define GPIO_LCD_SPI_SCLK_PIN                                    DL_GPIO_PIN_17
#define GPIO_LCD_SPI_IOMUX_SCLK                                 (IOMUX_PINCM39)
#define GPIO_LCD_SPI_IOMUX_SCLK_FUNC                 IOMUX_PINCM39_PF_SPI1_SCLK



/* Defines for ADC12_0 */
#define ADC12_0_INST                                                        ADC0
#define ADC12_0_INST_IRQHandler                                  ADC0_IRQHandler
#define ADC12_0_INST_INT_IRQN                                    (ADC0_INT_IRQn)
#define ADC12_0_ADCMEM_0                                      DL_ADC12_MEM_IDX_0
#define ADC12_0_ADCMEM_0_REF                     DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC12_0_ADCMEM_0_REF_VOLTAGE_V                                       3.3
#define ADC12_0_INST_SUB_CH                                                  (1)
#define GPIO_ADC12_0_C2_PORT                                               GPIOA
#define GPIO_ADC12_0_C2_PIN                                       DL_GPIO_PIN_25
#define GPIO_ADC12_0_IOMUX_C2                                    (IOMUX_PINCM55)
#define GPIO_ADC12_0_IOMUX_C2_FUNC                (IOMUX_PINCM55_PF_UNCONNECTED)



/* Defines for DMA_CH0 */
#define DMA_CH0_CHAN_ID                                                      (0)
#define ADC12_0_INST_DMA_TRIGGER                      (DMA_ADC0_EVT_GEN_BD_TRIG)


/* Port definition for Pin Group GPIO_LEDS */
#define GPIO_LEDS_PORT                                                   (GPIOB)

/* Defines for USER_LED: GPIOB.14 with pinCMx 31 on package pin 2 */
#define GPIO_LEDS_USER_LED_PIN                                  (DL_GPIO_PIN_14)
#define GPIO_LEDS_USER_LED_IOMUX                                 (IOMUX_PINCM31)
/* Port definition for Pin Group ADG704_EN */
#define ADG704_EN_PORT                                                   (GPIOB)

/* Defines for PIN_0: GPIOB.1 with pinCMx 13 on package pin 48 */
#define ADG704_EN_PIN_0_PIN                                      (DL_GPIO_PIN_1)
#define ADG704_EN_PIN_0_IOMUX                                    (IOMUX_PINCM13)
/* Port definition for Pin Group ADG704_A0 */
#define ADG704_A0_PORT                                                   (GPIOB)

/* Defines for PIN_1: GPIOB.2 with pinCMx 15 on package pin 50 */
#define ADG704_A0_PIN_1_PIN                                      (DL_GPIO_PIN_2)
#define ADG704_A0_PIN_1_IOMUX                                    (IOMUX_PINCM15)
/* Port definition for Pin Group ADG704_A1 */
#define ADG704_A1_PORT                                                   (GPIOB)

/* Defines for PIN_2: GPIOB.3 with pinCMx 16 on package pin 51 */
#define ADG704_A1_PIN_2_PIN                                      (DL_GPIO_PIN_3)
#define ADG704_A1_PIN_2_IOMUX                                    (IOMUX_PINCM16)
/* Port definition for Pin Group GPIO_KEY */
#define GPIO_KEY_PORT                                                    (GPIOB)

/* Defines for PIN_3: GPIOB.0 with pinCMx 12 on package pin 47 */
#define GPIO_KEY_PIN_3_PIN                                       (DL_GPIO_PIN_0)
#define GPIO_KEY_PIN_3_IOMUX                                     (IOMUX_PINCM12)
/* Defines for CS: GPIOA.12 with pinCMx 34 on package pin 5 */
#define LCD_GPIO_CS_PORT                                                 (GPIOA)
#define LCD_GPIO_CS_PIN                                         (DL_GPIO_PIN_12)
#define LCD_GPIO_CS_IOMUX                                        (IOMUX_PINCM34)
/* Defines for DC: GPIOA.13 with pinCMx 35 on package pin 6 */
#define LCD_GPIO_DC_PORT                                                 (GPIOA)
#define LCD_GPIO_DC_PIN                                         (DL_GPIO_PIN_13)
#define LCD_GPIO_DC_IOMUX                                        (IOMUX_PINCM35)
/* Defines for RST: GPIOB.15 with pinCMx 32 on package pin 3 */
#define LCD_GPIO_RST_PORT                                                (GPIOB)
#define LCD_GPIO_RST_PIN                                        (DL_GPIO_PIN_15)
#define LCD_GPIO_RST_IOMUX                                       (IOMUX_PINCM32)
/* Defines for BL: GPIOB.16 with pinCMx 33 on package pin 4 */
#define LCD_GPIO_BL_PORT                                                 (GPIOB)
#define LCD_GPIO_BL_PIN                                         (DL_GPIO_PIN_16)
#define LCD_GPIO_BL_IOMUX                                        (IOMUX_PINCM33)



/* Defines for DAC12 */
#define DAC12_IRQHandler                                         DAC0_IRQHandler
#define DAC12_INT_IRQN                                           (DAC0_INT_IRQn)
#define GPIO_DAC12_OUT_PORT                                                GPIOA
#define GPIO_DAC12_OUT_PIN                                        DL_GPIO_PIN_15
#define GPIO_DAC12_IOMUX_OUT                                     (IOMUX_PINCM37)
#define GPIO_DAC12_IOMUX_OUT_FUNC                   IOMUX_PINCM37_PF_UNCONNECTED


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);
void SYSCFG_DL_CAPTURE_0_init(void);
void SYSCFG_DL_TIMER_0_init(void);
void SYSCFG_DL_TIMER_GATE_init(void);
void SYSCFG_DL_UART_0_init(void);
void SYSCFG_DL_LCD_SPI_init(void);
void SYSCFG_DL_ADC12_0_init(void);
void SYSCFG_DL_DMA_init(void);

void SYSCFG_DL_DAC12_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
