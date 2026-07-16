# AD9833 DDS 通用保底版

## 1. 最终目标

- MCU：MSPM0G3507，3.3 V逻辑。
- 输出波形：正弦、三角、方波。
- 保证使用范围：1 Hz~1 MHz。
- 25 MHz时钟下频率分辨率约0.093 Hz。
- 正弦/三角最大输出约1.8 Vpp，旋钮连续向下调节。
- 正弦/三角经过3.2 MHz三阶Butterworth低通；方波走旁路。
- 默认连接示波器1 MΩ高阻输入，不把首版目标设为标准50 Ω信号源。

## 2. VOUT后的最终信号链

```text
AD9833 VOUT（DDS_RAW，内部约200Ω）
   |
   +----> RAW_OUT：原始波形测试点/方波旁路
   |
   +-> 10uF隔直
       -> 2.5V偏置 + 1k多圈电位器调幅
       -> OPA2356 U2B固定x3
       -> 一级RC低通 + OPA2356 U3A单位增益缓冲
       -> 二阶Sallen-Key + OPA2356 U3B单位增益缓冲
       -> FILTER_OUT
            |-> OUT_BIASED：约2.5V中心
            +-> 47uF隔直 -> OUT_AC：约0V中心
```

运放通道分工：

| 通道 | 功能 |
|---|---|
| OPA2356 U2A | 缓冲2.5 V参考`VMID_2V5` |
| OPA2356 U2B | 调幅后的固定x3前级 |
| OPA2356 U3A | 一级RC后的单位增益缓冲 |
| OPA2356 U3B | 二阶Sallen-Key单位增益及输出驱动 |

整板使用两颗OPA2356，不使用ADA4857，也不再单独使用OPA356。

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

AD9833模拟DAC输出约38 mV~0.65 V，正弦波约0.6 Vpp，前级x3后约1.8 Vpp。

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
- 晶振使用`3V3_DDS`供电，不能使用AD9833的CAP/2.5V供电。
- 晶振电源脚放100 nF，输出串22~33 Ω进入MCLK；串阻靠近晶振。
- 预留外部MCLK接口，通过两个0 Ω位选择`INT/EXT`，只能焊一个。

### 电源

- `VIN_5V`经`LP5907-3.3`或同类低噪声LDO得到`3V3_DDS`。
- `3V3_DDS`只供AD9833、25 MHz晶振和可选逻辑缓冲。
- 两颗OPA2356均使用`+5V/GND`单电源，不能接±5 V。
- 每颗OPA2356电源脚旁放100 nF，板上5 V入口放4.7~10 uF。

## 6. 2.5 V参考

```text
+5V -> R21=10k -> VMID_RAW -> R22=10k -> GND
                         |
                    100nF + 1uF
                         |
                        GND

VMID_RAW -> OPA2356 U2A电压跟随 -> 22Ω隔离 -> VMID_2V5
                                                   |
                                              100nF + 10uF
                                                   |
                                                  GND
```

- 22 Ω用于隔离OPA2356与REF节点的大电容，降低容性负载振荡风险。
- `VMID_2V5`只给本板偏置、滤波电容和运放反馈使用，不向外供电。
- 所有标注`REF`的滤波节点都接`VMID_2V5`，不能接真实GND。

## 7. 隔直、调幅和固定x3前级

```text
DDS_RAW -> C31=10uF -> AMP_IN
AMP_IN -> R31=100k -> VMID_2V5

RV1=1k多圈电位器：
上端 -> AMP_IN
下端 -> VMID_2V5
滑端 -> OPA2356 U2B同相输入

U2B反相网络：
反相输入 -> RG31=1.00k -> VMID_2V5
输出 -> RF31=2.00k -> 反相输入
输出网络名 -> PREAMP_OUT
```

闭环增益：

```text
G = 1 + 2.00k/1.00k = 3
```

- RV1负责0%~100%幅度调整，不设置增益跳帽。
- 如果C31使用有极性电解，正极接`AMP_IN/2.5V偏置侧`，负极接`DDS_RAW/约0.3V侧`。

## 8. 3.2 MHz三阶Butterworth低通

最终采用ADI滤波器向导生成的0 dB三阶Butterworth，由“一级RC + 二阶Sallen-Key”组成。滤波总增益为1。

### 阶段A：一级RC与U3A缓冲

```text
PREAMP_OUT -> R41=453Ω -> FA -> OPA2356 U3A同相输入
                         |
                      C41=110pF
                         |
                     VMID_2V5

U3A输出直接反馈到反相输入
U3A输出网络名 -> STAGE_A_OUT
```

理论极点约：

```text
1/(2π×453Ω×110pF) ≈ 3.19MHz
```

### 阶段B：二阶Sallen-Key与U3B缓冲

```text
STAGE_A_OUT -> R42=110Ω -> FB1 -> R43=221Ω -> FB2 -> U3B同相输入
                              |                   |
                           C42=680pF           C43=150pF
                              |                   |
                         FILTER_OUT           VMID_2V5

U3B输出直接反馈到反相输入
U3B输出网络名 -> FILTER_OUT
```

必须注意：

- `C42=680 pF`连接在节点FB1与U3B输出`FILTER_OUT`之间。
- `C43=150 pF`连接在节点FB2与`VMID_2V5`之间。
- `C41=110 pF`也接`VMID_2V5`，不是GND。
- 三个滤波电容全部使用C0G/NP0，建议2%或5%。
- 453 Ω、110 Ω、221 Ω使用1%电阻。
- U3A/U3B均为单位增益，OPA2356满足单位增益稳定要求。

理想响应：

- 1 MHz衰减约0.004 dB，基本可忽略。
- 25 MHz衰减约53 dB。
- 实际响应还受OPA2356模型、器件容差和PCB寄生影响，焊板后必须实测。

## 9. 三个输出节点

### RAW_OUT

- 直接来自`DDS_RAW`。
- 用于调试AD9833本体和输出方波。
- 只接高阻示波器或可选逻辑缓冲，不直接驱动50 Ω。

### OUT_BIASED

```text
FILTER_OUT -> R51=0Ω（默认焊） -> OUT_BIASED/SMA或排针
```

- 正弦/三角波中心约2.5 V。
- 适合测试ADC、比较器和单电源电路。

### OUT_AC

```text
FILTER_OUT -> C51=47uF/16V铝电解 -> R52=0Ω（默认焊） -> OUT_AC/SMA
                                                    |
                                               R53=100k
                                                    |
                                                   GND
```

- C51正极朝`FILTER_OUT/约2.5V侧`，负极朝`OUT_AC/约0V侧`。
- R53给电容负极提供直流参考，防止输出悬空。
- 47 uF与100 kΩ的理论高通截止约0.034 Hz，适合1 Hz输出。
- 默认按示波器1 MΩ高阻输入标定。
- R51/R52旁可以预留49.9 Ω位置，但首版默认不焊49.9 Ω。

## 10. 方波处理

- AD9833切换到方波模式时，从`RAW_OUT`输出。
- 方波不经过10 uF调幅链和三阶低通。
- 若需要可靠的3.3 V方波，预留施密特逻辑缓冲器封装，第一版可DNP。
- 正弦/三角使用`OUT_BIASED/OUT_AC`；方波使用`RAW_OUT`。

## 11. PCB布局

```text
晶振/数字接口 -> AD9833 -> U2B前级x3 -> U3A一级RC -> U3B二阶滤波 -> 输出
                         |
                    U2A/VMID靠近三颗信号运放的REF节点
```

- 晶振、22~33 Ω串阻和MCLK脚靠近放置。
- MCLK、SCLK、SDATA、FSYNC不穿过运放和滤波区。
- COMP、CAP/2.5V、VDD去耦紧贴AD9833。
- U2B靠近AD9833，`DDS_RAW/AMP_IN`走线短。
- U3A靠近一级RC，U3B与110 Ω、221 Ω、680 pF、150 pF紧凑摆放。
- FB1、FB2为敏感节点，不引长测试线，不靠近MCLK/SPI。
- `VMID_2V5`走线短而粗，滤波电容回流直接回到REF区域。
- 底层尽量保持连续地平面，SMA附近增加地过孔。
- 测试点：`3V3_DDS`、`MCLK`、`DDS_RAW`、`VMID_2V5`、`PREAMP_OUT`、`STAGE_A_OUT`、`FILTER_OUT`、`OUT_AC`。

## 12. BOM核心器件

- AD9833BRMZ：至少3片。
- 25 MHz、3.3 V CMOS有源晶振：至少3只。
- OPA2356：每板2片，备料至少6片。
- LP5907-3.3或同类LDO：至少3片。
- 1 kΩ多圈电位器。
- 1%电阻：22 Ω、110 Ω、221 Ω、453 Ω、1.00 kΩ、2.00 kΩ、10 kΩ、100 kΩ。
- C0G/NP0电容：110 pF、150 pF、680 pF。
- 普通去耦/隔直电容：10 nF、100 nF、1 uF、10 uF。
- 输出隔直：47 uF/16 V铝电解。
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
2. 焊U2A及22 Ω隔离，确认`VMID_2V5≈2.5 V`且无振荡。
3. 焊调幅和U2B，确认`PREAMP_OUT`静态约2.5 V、交流增益约3倍。
4. 焊一级RC和U3A，确认`STAGE_A_OUT`静态约2.5 V。
5. 焊二阶Sallen-Key和U3B，确认`FILTER_OUT`静态约2.5 V且无高频振荡。
6. 测100 Hz、1 kHz、10 kHz、100 kHz、1 MHz正弦，记录相对1 kHz的幅度变化。
7. 测三角波；方波只看`RAW_OUT`。
8. 接47 uF输出电容后，确认极性、`OUT_AC`直流接近0 V以及1 Hz衰减。
9. 最后接LCD菜单、扫频和频率步进。
