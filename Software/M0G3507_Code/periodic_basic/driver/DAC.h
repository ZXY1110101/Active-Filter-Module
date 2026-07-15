#ifndef DAC_H
#define DAC_H

#include <stdbool.h>
#include <stdint.h>

#define DAC_FULL_SCALE_CODE (4095U)
#define DAC_MID_SCALE_CODE  (2048U)

/* 初始化 TLV3507 比较器基准输出，默认输出半量程。 */
void DAC_Init(void);

/* 设置 DAC0（PA15）的 12 位输出码。超范围值会被限制到 4095。 */
void DAC_SetCode(uint16_t code);

/* 获取最近一次写入 DAC0 的输出码。 */
uint16_t DAC_GetCode(void);

/*
 * 对 ADC 中点进行跨帧中值滤波、死区判断和分级跟踪。
 * valid=false 时冻结当前基准，不把削顶或微弱噪声写入比较器阈值。
 */
void DAC_TrackReference(uint16_t targetCode, bool valid);

#endif
