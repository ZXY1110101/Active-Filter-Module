# AD9833 DDS 通用保底版

## 1. 最终目标

- MCU：MSPM0G3507，3.3 V逻辑。
- 输出波形：正弦、三角、方波。
- 保证使用范围：1 Hz~1 MHz。
- 25 MHz时钟下频率分辨率约0.093 Hz。
- 正弦/三角最大输出目标约3 Vpp，旋钮连续向下调节。
- 正弦/三角经过3.2 MHz低通；方波走旁路，不经过低通。
- 默认驱动示波器1 MΩ高阻输入，不把首版目标设成标准50 Ω信号源。

## 2. VOUT后面只保留这一条信号链

```text
AD9833 VOUT（DDS_RAW，内部约200Ω）
   |
   +----> RAW_OUT：原始波形测试点/方波旁路
   |
   +-> 10uF隔直
       -> 2.5V偏置 + 1k调幅电位器
       -> OPA2356B固定x3缓冲放大
       -> OPA356二阶Sallen-Key低通（3.2MHz，固定增益约1.59）
       -> FILTER_OUT
            |-> OUT_BIASED：约2.5V中心
            +-> 470uF隔直 -> OUT_AC：约0V中心
```

信号实际只经过两个运放：

1. `OPA2356B`：调幅后的固定x3缓冲放大，并隔离AD9833内部200 Ω。
2. `OPA356`：完成3.2 MHz二阶Butterworth滤波并驱动输出。

`OPA2356A`只缓冲2.5 V中点参考，不在信号传输链中。

删除原来的x1/x3跳档，统一固定x3；幅度大小全部由1 kΩ多圈电位器控制，减少跳帽和调试状态。

## 3. AD9833最小系统

器件：`AD9833BRMZ`，MSOP-10，供电3.3 V。

| 引脚 | 名称 | 连接 |
|---|---|---|
| 1 | COMP | 10 nF接`3V3_DDS`，紧贴引脚 |
| 2 | VDD | `3V3_DDS`，100 nF + 10 uF到AGND |
| 3 | CAP/2.5V | 100 nF到DGND，只作去耦，不给晶振供电 |
| 4 | DGND | 数字地，与AGND在U1附近明确连接 |
| 5 | MCLK | `DDS_MCLK` |
| 6 | SDATA | `DDS_SDATA` |
| 7 | SCLK | `DDS_SCLK` |
| 8 | FSYNC | `DDS_FSYNC`，10 kΩ上拉到3V3_DDS |
| 9 | AGND | 模拟地 |
| 10 | VOUT | `DDS_RAW`，内部约200 Ω |

AD9833模拟DAC输出约38 mV~0.65 V，正弦波约0.6 Vpp，所以需要后级放大。

## 4. M0接口

| DDS信号 | M0G3507引脚 | 说明 |
|---|---|---|
| `DDS_SCLK` | PA17 | 与LCD SCLK共享 |
| `DDS_SDATA` | PA18 | 与LCD MOSI共享 |
| `DDS_FSYNC` | PA16 | DDS独立片选 |
| `VIN_5V` | +5V | DDS板本地降压 |
| `GND` | GND | 共地 |

SPI先用500 kHz~1 MHz。DDS_FSYNC和LCD_CS不能同时拉低。

接口排针：

```text
VIN_5V  GND  DDS_FSYNC  DDS_SCLK  DDS_SDATA
```

## 5. 时钟与电源

### 时钟

- 板载3.3 V、25 MHz CMOS有源晶振。
- 晶振使用`3V3_DDS`供电，100 nF紧贴电源脚。
- 晶振输出串22~33 Ω进入MCLK，串阻靠近晶振。
- 预留外部MCLK接口，用两个0 Ω位选择`INT/EXT`，只能焊一个。

### 电源

- `VIN_5V`经`LP5907-3.3`或同类低噪声LDO得到`3V3_DDS`。
- `3V3_DDS`只供AD9833、25 MHz晶振和可选逻辑缓冲。
- OPA2356、OPA356使用`+5V/GND`单电源，不能接±5 V。
- 每颗芯片电源脚旁放100 nF，板上5 V入口放4.7~10 uF。

## 6. 2.5 V中点

```text
+5V -> 10k -> VMID_RAW -> 10k -> GND
                  |
             100nF + 1uF
                  |
                 GND

OPA2356A电压跟随 -> VMID_2V5
```

`VMID_2V5`只给本板的偏置和运放反馈使用，不向外供电。

## 7. 隔直、调幅和OPA2356B固定x3

```text
DDS_RAW -> C31=10uF -> AMP_IN
AMP_IN -> R31=100k -> VMID_2V5

RV1=1k多圈电位器：
上端 -> AMP_IN
下端 -> VMID_2V5
滑端 -> OPA2356B同相输入

OPA2356B：
反相输入 -> RG31=1.00k -> VMID_2V5
输出 -> RF31=2.00k -> 反相输入
输出网络名 -> PREAMP_OUT
```

闭环增益：

```text
G1 = 1 + 2.00k/1.00k = 3
```

RV1负责0%~100%幅度调整，不再设置增益跳帽。

## 8. OPA356二阶3.2 MHz低通

OPA2356B已经隔离AD9833内部200 Ω，因此下面滤波器只按自身R/C计算，不再把200 Ω加进去。

采用等R、等C、非反相Sallen-Key结构：

```text
PREAMP_OUT -> R41=330Ω -> F1 -> R42=330Ω -> F2 -> OPA356同相输入
                            |                   |
                         C41=150pF           C42=150pF
                            |                   |
                       FILTER_OUT           VMID_2V5

OPA356反相网络：
FILTER_OUT -> RF41=590Ω -> 反相输入 -> RG41=1.00k -> VMID_2V5
```

必须注意：

- `C41`接在F1与OPA356输出`FILTER_OUT`之间。
- `C42`接在F2与`VMID_2V5`之间。
- 两个150 pF不能都直接接地。
- 150 pF使用C0G/NP0，2%或5%；330 Ω建议1%。

理论参数：

```text
K = 1 + 590/1000 ≈ 1.59
fc = 1/(2π×330Ω×150pF) ≈ 3.2MHz
Q = 1/(3-K) ≈ 0.71
```

因此接近二阶Butterworth：1 MHz理论衰减约0.04 dB，25 MHz理论抑制约35 dB。

最大理论幅度：

```text
约0.6Vpp × 3 × 1.59 ≈ 2.9Vpp
```

## 9. 输出只分为三种节点

### RAW_OUT

- 直接来自`DDS_RAW`。
- 用于调试AD9833本体和输出方波。
- 只接高阻示波器或可选逻辑缓冲，不能直接驱动50 Ω。

### OUT_BIASED

```text
FILTER_OUT -> R51=0Ω（默认焊） -> SMA/排针
```

- 正弦/三角波中心约2.5 V。
- 适合测试ADC、比较器和单电源电路。

### OUT_AC

```text
FILTER_OUT -> C51=470uF（正端朝OPA356） -> R52=0Ω（默认焊） -> SMA
```

- 隔直后中心约0 V，适合示波器观察。
- 默认按1 MΩ高阻输入标定。

R51/R52旁边可以各预留49.9 Ω位置，但首版默认不焊49.9 Ω，避免高阻/50 Ω幅度定义混乱。

## 10. 方波处理

- AD9833切到方波模式时，从`RAW_OUT`输出。
- 方波不经过10 uF调幅链和3.2 MHz低通。
- 若需要更可靠的3.3 V方波，预留一个施密特逻辑缓冲器封装，第一版可不焊。
- 正弦、三角使用`OUT_BIASED/OUT_AC`；方波使用`RAW_OUT`，不要现场混用接口。

## 11. PCB布局

```text
晶振/数字接口 -> AD9833 -> OPA2356B -> OPA356滤波 -> 输出SMA
                         |
                    OPA2356A/VMID靠近两级运放
```

- 晶振、22~33 Ω串阻和MCLK脚靠近放置。
- MCLK、SCLK、SDATA、FSYNC不穿过运放和滤波区。
- COMP、CAP/2.5V、VDD去耦紧贴AD9833。
- OPA2356B靠近AD9833，`DDS_RAW/AMP_IN`走线短。
- R41、R42、C41、C42围绕OPA356同相输入紧凑摆放；F1、F2不引长测试线。
- 反馈电阻紧贴反相输入和输出。
- 底层尽量保持连续地平面，SMA附近增加地过孔。
- 测试点：`3V3_DDS`、`MCLK`、`DDS_RAW`、`VMID_2V5`、`PREAMP_OUT`、`FILTER_OUT`、`OUT_AC`。

## 12. BOM核心器件

- AD9833BRMZ：至少3片。
- 25 MHz、3.3 V CMOS有源晶振：至少3只。
- OPA2356：至少3片。
- OPA356：至少3片。
- LP5907-3.3或同类LDO：至少3片。
- 1 kΩ多圈电位器。
- 精密电阻：330 Ω、590 Ω、1.00 kΩ、2.00 kΩ。
- C0G/NP0：150 pF。
- 100 nF、1 uF、10 uF、470 uF电容。
- SMA、排针、0 Ω及可选49.9 Ω。

## 13. 软件与验收

驱动文件：

```text
dds_ad9833.c
dds_ad9833.h
```

频率字：

```text
FREQ_WORD = round(fout × 2^28 / 25,000,000)
```

上电：置RESET -> 写频率/相位 -> 清RESET。

焊接验收顺序：

1. 只焊电源、晶振、AD9833，确认3.3 V、25 MHz和`RAW_OUT`。
2. 焊OPA2356A，确认`VMID_2V5=2.5 V`。
3. 焊调幅和OPA2356B，确认`PREAMP_OUT`静态约2.5 V、交流增益约3倍。
4. 焊OPA356及330 Ω/150 pF网络，确认`FILTER_OUT`静态约2.5 V且无振荡。
5. 测100 Hz、1 kHz、10 kHz、100 kHz、1 MHz正弦，记录相对1 kHz的幅度变化。
6. 测三角波；方波只看`RAW_OUT`。
7. 最后接LCD菜单、扫频和频率步进。
