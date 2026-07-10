# Active Filter Module - 信号采集与处理程序

本工程基于 MSPM0G3507 芯片开发，主要包含以下功能模块：

* **核心信号采集与处理** (`adc12_triggered_by_timer_eventV_1.c`): 
  - 使用 Timer 触发 ADC (100kSPS) 并通过 DMA 搬运。
  - 包含 qsort 排序及掐头去尾法消除方波振铃。
  - 负责计算信号的 RMS 和 Vpp。
* **频率与周期测量** (`timx_timer_mode_capture_duty_and_period...c`):
  - 使用 Timer 输入捕获模式。
  - 测量信号频率，为后续自动量程切换提供基准。
* **波形发生/输出** (`dac12_fifo_timer_eventV_1.c`):
  - DAC 定时输出模块。
* **数据回传** (`uart_rw_multibyte_fifo_poll.c`):
  - 串口 FIFO 轮询通信，向上位机发送计算结果。