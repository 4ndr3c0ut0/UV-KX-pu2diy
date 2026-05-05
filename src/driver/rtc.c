/*
 * RTC driver for DP32G030
 * Provides elapsed-time counting for Doppler satellite tracking.
 * NOT initialized at boot — only started when Doppler mode is activated.
 */

#ifdef ENABLE_DOPPLER

#include "driver/rtc.h"
#include "dp32g030/rtc.h"
#include "dp32g030/irq.h"
#include "ARMCM0.h"
#include "driver/system.h"
#include <string.h>

uint8_t rtc_time[6];
static volatile bool rtc_tick = false;

void RTC_INIT(void)
{
    uint32_t correct_freq = 32768 - 1 +
        ((((RC_FREQ_DELTA & 0x400) >> 10) ? 1 : -1) * (RC_FREQ_DELTA & 0x3ff));

    RTC_PRE &= ~((0x7fffU << 0) | (0xfU << 20) | (0x1U << 24));
    RTC_PRE |= correct_freq | (0U << 20) | (0U << 24);

    memset(rtc_time, 0, sizeof(rtc_time));
    RTC_Set();

    // Use low priority so we don't block other interrupts
    NVIC_SetPriority((IRQn_Type)DP32_RTC_IRQn, 3);

    RTC_IF = 0xFFU;             // Clear all pending interrupt flags
    RTC_IE = (1U << 0);        // Enable second interrupt only

    RTC_CFG |= (1U << 0);      // RTC enable

    NVIC_EnableIRQ((IRQn_Type)DP32_RTC_IRQn);
}

void RTC_Stop(void)
{
    NVIC_DisableIRQ((IRQn_Type)DP32_RTC_IRQn);
    RTC_IE = 0;
    RTC_CFG &= ~(1U << 0);     // RTC disable
}

bool RTC_HasTick(void)
{
    if (rtc_tick)
    {
        rtc_tick = false;
        return true;
    }
    return false;
}

void RTC_SetTick(void)
{
    rtc_tick = true;
}

void RTC_Set(void)
{
    RTC_DR = (2U << 24)
             | ((rtc_time[0] / 10) << 20)
             | ((rtc_time[0] % 10) << 16)
             | ((rtc_time[1] / 10) << 12)
             | ((rtc_time[1] % 10) << 8)
             | ((rtc_time[2] / 10) << 4)
             | ((rtc_time[2] % 10) << 0);

    RTC_TR = ((rtc_time[3] / 10) << 20)
             | ((rtc_time[3] % 10) << 16)
             | ((rtc_time[4] / 10) << 12)
             | ((rtc_time[4] % 10) << 8)
             | ((rtc_time[5] / 10) << 4)
             | ((rtc_time[5] % 10) << 0);

    RTC_CFG |= (1U << 2);
}

void RTC_Get(void)
{
    rtc_time[0] = ((RTC_TSDR >> 20) & 0xF) * 10 + ((RTC_TSDR >> 16) & 0xF);
    rtc_time[1] = ((RTC_TSDR >> 12) & 0x1) * 10 + ((RTC_TSDR >> 8)  & 0xF);
    rtc_time[2] = ((RTC_TSDR >> 4)  & 0xF) * 10 + ((RTC_TSDR >> 0)  & 0xF);

    rtc_time[3] = ((RTC_TSTR >> 20) & 0x7) * 10 + ((RTC_TSTR >> 16) & 0xF);
    rtc_time[4] = ((RTC_TSTR >> 12) & 0x7) * 10 + ((RTC_TSTR >> 8)  & 0xF);
    rtc_time[5] = ((RTC_TSTR >> 4)  & 0x7) * 10 + ((RTC_TSTR >> 0)  & 0xF);
}

#endif // ENABLE_DOPPLER
