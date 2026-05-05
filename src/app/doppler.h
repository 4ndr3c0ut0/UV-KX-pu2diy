/*
 * Doppler satellite tracking module
 *
 * Uses standard memory channels as satellite database.
 * Any channel whose name starts with "SAT" is treated as a satellite.
 * - Downlink = channel RX frequency
 * - Uplink = calculated from TX offset (like a repeater)
 * - CTCSS = channel TX CTCSS setting
 * - Doppler shift = calculated from frequency at runtime
 *
 * No dedicated EEPROM area needed. Edit satellites via normal channel menu or CHIRP.
 */

#ifndef APP_DOPPLER_H
#define APP_DOPPLER_H

#include <stdint.h>
#include <stdbool.h>

// Max satellite channels we track (indices into memory channels)
#define DOPPLER_MAX_SATS    40

typedef struct {
    uint32_t uplink;
    uint32_t downlink;
} satellite_freq_t;

void    DOPPLER_Init(void);             // scan channels for SAT* entries
void    DOPPLER_LoadSatellite(uint8_t satIdx);
void    DOPPLER_StartTracking(uint16_t duration_secs);
void    DOPPLER_StopTracking(void);
void    DOPPLER_Update(void);           // call from main loop on RTC tick
void    APP_RunDoppler(void);           // standalone app entry point

uint32_t isqrt32(uint32_t n);
uint16_t DOPPLER_CalcShift(uint32_t freq_hz10);

extern satellite_freq_t  gSatelliteFreq;
extern bool              gDopplerMode;
extern bool              gDopplerTracking;
extern uint8_t           gDopplerSatCount;   // number of SAT* channels found
extern uint8_t           gDopplerSatIndex;   // currently selected (index into sat list)
extern int32_t           gDopplerElapsed;
extern int32_t           gDopplerTimeDiff;
extern int32_t           gDopplerTimeDiff1;
extern uint16_t          gDopplerDuration;

// Info about current satellite (loaded from channel)
extern char              gSatName[16];
extern uint32_t          gSatDownlink;       // nominal downlink freq (Hz/10)
extern uint32_t          gSatUplink;         // nominal uplink freq (Hz/10)
extern uint16_t          gSatCtcssTx;        // CTCSS tone (Hz/10), 0=none
extern uint8_t           gSatTxPower;        // OUTPUT_POWER setting from channel
extern uint16_t          gSatDownShift;      // max Doppler shift downlink (Hz/10)
extern uint16_t          gSatUpShift;        // max Doppler shift uplink (Hz/10)
extern uint16_t          gSatTConst;         // time constant x10

#endif // APP_DOPPLER_H
