/*
 * RTC (Real Time Clock) register definitions for DP32G030
 * Based on LOSEHU's implementation for Doppler satellite tracking
 */

#ifndef HARDWARE_DP32G030_RTC_H
#define HARDWARE_DP32G030_RTC_H

#include <stdint.h>

// RCLF 32768HZ
#define RTC_BASE_ADD        0x40069000U

#define RTC_CFG_ADD         (0x00U + RTC_BASE_ADD)  // Configuration register
#define RTC_IE_ADD          (0x04U + RTC_BASE_ADD)  // Interrupt enable register
#define RTC_IF_ADD          (0x08U + RTC_BASE_ADD)  // Status register
#define RTC_PRE_ADD         (0x10U + RTC_BASE_ADD)  // Prescaler register
#define RTC_TR_ADD          (0x14U + RTC_BASE_ADD)  // Time register
#define RTC_DR_ADD          (0x18U + RTC_BASE_ADD)  // Date register
#define RTC_AR_ADD          (0x1CU + RTC_BASE_ADD)  // Alarm register
#define RTC_TSTR_ADD        (0x20U + RTC_BASE_ADD)  // Current time register
#define RTC_TSDR_ADD        (0x24U + RTC_BASE_ADD)  // Current date register
#define RTC_CNT_ADD         (0x28U + RTC_BASE_ADD)  // Second counter current value
#define RTC_VALID_ADD       (0x2CU + RTC_BASE_ADD)  // Current time valid flag register

#define RTC_CFG             (*(volatile uint32_t *)RTC_CFG_ADD)
#define RTC_IE              (*(volatile uint32_t *)RTC_IE_ADD)
#define RTC_IF              (*(volatile uint32_t *)RTC_IF_ADD)
#define RTC_PRE             (*(volatile uint32_t *)RTC_PRE_ADD)
#define RTC_TR              (*(volatile uint32_t *)RTC_TR_ADD)
#define RTC_DR              (*(volatile uint32_t *)RTC_DR_ADD)
#define RTC_AR              (*(volatile uint32_t *)RTC_AR_ADD)
#define RTC_TSTR            (*(volatile uint32_t *)RTC_TSTR_ADD)
#define RTC_TSDR            (*(volatile uint32_t *)RTC_TSDR_ADD)
#define RTC_CNT             (*(volatile uint32_t *)RTC_CNT_ADD)
#define RTC_VALID           (*(volatile uint32_t *)RTC_VALID_ADD)

#define RC_FREQ_DELTA       (*(volatile uint32_t *)(0x40000000U + 0x78U))
#define TRIM_RCLF           (*(volatile uint32_t *)(0x40000800U + 0x34U))

#endif // HARDWARE_DP32G030_RTC_H
