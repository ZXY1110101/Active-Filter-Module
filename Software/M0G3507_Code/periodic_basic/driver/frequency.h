#ifndef FREQUENCY_H
#define FREQUENCY_H

#include <stdint.h>

typedef enum
{
    FREQUENCY_MODE_PERIOD = 0,
    FREQUENCY_MODE_GATE,
    FREQUENCY_MODE_AUTO
} FrequencyMode;

void Frequency_Init(void);
void Frequency_StartPeriod(void);
void Frequency_StartGate(void);
void Frequency_StartAuto(void);
void Frequency_Task(void);

uint32_t Frequency_GetHz(void);
float Frequency_GetPeriodUs(void);
float Frequency_GetDuty(void);
FrequencyMode Frequency_GetMode(void);

extern volatile uint32_t frequencyHz;

#endif
