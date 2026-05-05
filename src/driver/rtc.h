/*
 * RTC driver for DP32G030
 */

#ifndef DRIVER_RTC_H
#define DRIVER_RTC_H

#include <stdint.h>
#include <stdbool.h>

void RTC_INIT(void);
void RTC_Stop(void);
void RTC_Set(void);
void RTC_Get(void);
bool RTC_HasTick(void);
void RTC_SetTick(void);

extern uint8_t rtc_time[6];

#endif // DRIVER_RTC_H
