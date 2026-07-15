# periodic_basic 引脚分配表

> 芯片：MSPM0G3507  
> 封装：LQFP-64 (PM)  
> 配置来源：`main.syscfg` 及其生成的 `ti_msp_dl_config.h`  
> 更新时间：2026-07-15

## ILI9341 接线

| ILI9341 信号 | MSPM0G3507 引脚 | 工程符号 | 方向 | 说明 |
|---|---|---|---|---|
| SCK / CLK | PA17 | `LCD_SPI SCLK` | 输出 | SPI1 时钟 |
| MOSI / SDI | PA18 | `LCD_SPI PICO` | 输出 | SPI1 主机输出 |
| MISO / SDO | PA16 | `LCD_SPI POCI` | 输入 | SPI1 主机输入；纯写屏时通常可不接 |
| CS | PA12 | `LCD_GPIO_CS` | 输出 | 片选，低电平有效 |
| DC / RS | PA13 | `LCD_GPIO_DC` | 输出 | 命令/数据选择 |
| RST / RES | PB15 | `LCD_GPIO_RST` | 输出 | LCD 硬件复位 |
| BL / LED | PB16 | `LCD_GPIO_BL` | 输出 | 背光控制 |
| VCC | 3.3V | — | 电源 | 按模块实际供电要求连接 |
| GND | GND | — | 电源 | 与开发板共地 |

SPI 配置：`SPI1`、Motorola SPI 模式 3（`MOTO3`）、极性配置值为 `1`。

## 全部已占用外部引脚

| 序号 | MSPM0G3507 引脚 | 模块/用途 | 外设功能 | 方向 | 工程标识 |
|---:|---|---|---|---|---|
| 1 | PA5 | 40 MHz 外部高速晶振输入 | HFXIN | 输入 | `GPIO_HFXIN` |
| 2 | PA6 | 40 MHz 外部高速晶振输出 | HFXOUT | 输出 | `GPIO_HFXOUT` |
| 3 | PA10 | 串口发送 | UART0 TX | 输出 | `UART_0 TX` |
| 4 | PA11 | 串口接收 | UART0 RX | 输入 | `UART_0 RX` |
| 5 | PA12 | LCD 片选 | GPIO | 输出 | `LCD_GPIO_CS` |
| 6 | PA13 | LCD 命令/数据选择 | GPIO | 输出 | `LCD_GPIO_DC` |
| 7 | PA16 | LCD SPI 数据输入 | SPI1 POCI / MISO | 输入 | `LCD_SPI POCI` |
| 8 | PA17 | LCD SPI 时钟 | SPI1 SCLK | 输出 | `LCD_SPI SCLK` |
| 9 | PA18 | LCD SPI 数据输出 | SPI1 PICO / MOSI | 输出 | `LCD_SPI PICO` |
| 10 | PA19 | 调试数据 | SWDIO | 双向 | `DEBUGSS SWDIO` |
| 11 | PA20 | 调试时钟 | SWCLK | 输入 | `DEBUGSS SWCLK` |
| 12 | PA25 | 模拟信号采样 | ADC0 通道 2 | 模拟输入 | `ADC12_0 C2` |
| 13 | PB0 | 按键输入 | GPIO | 输入 | `GPIO_KEY_PIN_3` |
| 14 | PB1 | ADG704 使能 | GPIO | 输出 | `ADG704_EN_PIN_0` |
| 15 | PB2 | ADG704 地址 A0 | GPIO | 输出 | `ADG704_A0_PIN_1` |
| 16 | PB3 | ADG704 地址 A1 | GPIO | 输出 | `ADG704_A1_PIN_2` |
| 17 | PB13 | 频率/占空比捕获输入 | TIMG12 CCP0 | 输入 | `CAPTURE_0 C0` |
| 18 | PB14 | 用户 LED | GPIO | 输出 | `GPIO_LEDS_USER_LED` |
| 19 | PB15 | LCD 复位 | GPIO | 输出 | `LCD_GPIO_RST` |
| 20 | PB16 | LCD 背光 | GPIO | 输出 | `LCD_GPIO_BL` |

## 不占用外部引脚的已配置资源

| 资源 | 实例 | 用途 |
|---|---|---|
| ADC DMA | DMA_CH0 | 搬运 ADC0 采样数据，传输长度 1024 |
| ADC 采样定时器 | TIMG7 | 10 us 周期，通过事件触发 ADC |
| 频率门控定时器 | TIMA1 | 100 ms 门控周期 |
| 捕获定时器 | TIMG12 | 通过 PB13 捕获频率和占空比 |

## 端口速查

```text
PA5   HFXIN                 PB0   KEY
PA6   HFXOUT                PB1   ADG704_EN
PA10  UART0_TX              PB2   ADG704_A0
PA11  UART0_RX              PB3   ADG704_A1
PA12  LCD_CS                PB13  CAPTURE_IN
PA13  LCD_DC                PB14  USER_LED
PA16  LCD_MISO              PB15  LCD_RST
PA17  LCD_SCK               PB16  LCD_BL
PA18  LCD_MOSI
PA19  SWDIO
PA20  SWCLK
PA25  ADC_IN
```

修改引脚时以 `main.syscfg` 为唯一配置入口，不要直接修改 `Debug/ti_msp_dl_config.h`，因为该文件会由 SysConfig 自动重新生成。
