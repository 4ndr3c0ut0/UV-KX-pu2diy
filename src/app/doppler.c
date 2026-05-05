/*
 * Doppler satellite tracking — channel-based satellite database.
 * Scans memory channels for names starting with "SAT".
 */

#ifdef ENABLE_DOPPLER

#include "app/doppler.h"
#include "driver/eeprom.h"
#include "radio.h"
#include "dcs.h"
#include "settings.h"
#include "frequencies.h"
#include "dp32g030/rtc.h"
#include <string.h>

// List of memory channel indices that are satellites
static uint8_t satChannels[DOPPLER_MAX_SATS];

satellite_freq_t  gSatelliteFreq;
bool              gDopplerMode     = false;
bool              gDopplerTracking = false;
uint8_t           gDopplerSatCount = 0;
uint8_t           gDopplerSatIndex = 0;
int32_t           gDopplerElapsed  = 0;
int32_t           gDopplerTimeDiff  = 0;
int32_t           gDopplerTimeDiff1 = 0;
uint16_t          gDopplerDuration = 600;

char              gSatName[16];
uint32_t          gSatDownlink = 0;
uint32_t          gSatUplink   = 0;
uint16_t          gSatCtcssTx  = 0;
uint8_t           gSatTxPower  = 2;  // default HIGH
uint16_t          gSatDownShift = 0;
uint16_t          gSatUpShift   = 0;
uint16_t          gSatTConst    = 1430;

// ---- Utility ----

uint32_t isqrt32(uint32_t n)
{
    if (n < 2) return n;
    uint32_t x = n, y = 1;
    while (x > y) { x = (x + y) >> 1; y = n / x; }
    return x;
}

uint16_t DOPPLER_CalcShift(uint32_t freq_hz10)
{
    // max Doppler shift = freq × v_orbital / c
    // v_orbital ≈ 7500 m/s, c = 299792458 m/s
    // In Hz/10 units: freq_hz10 / 39972
    return (uint16_t)(freq_hz10 / 39972);
}

// Estimate T constant from frequency band (rough altitude guess)
static uint16_t estimateTConst(uint32_t freq)
{
    (void)freq;
    // Most amateur LEO sats are at 400-600km
    // ISS/CSS ~420km → T≈120s, others ~500km → T≈143s
    // Default to 143s (×10 = 1430)
    return 1430;
}

// ---- Channel scanning ----

void DOPPLER_Init(void)
{
    gDopplerSatCount = 0;
    char name[16];

    for (uint16_t ch = 0; ch < 200 && gDopplerSatCount < DOPPLER_MAX_SATS; ch++)
    {
        if (!RADIO_CheckValidChannel(ch, false, 0))
            continue;

        SETTINGS_FetchChannelName(name, ch);
        if (name[0] == '$')
        {
            satChannels[gDopplerSatCount++] = (uint8_t)ch;
        }
    }

    if (gDopplerSatCount > 0)
        DOPPLER_LoadSatellite(0);
}

// Load satellite data from a memory channel
void DOPPLER_LoadSatellite(uint8_t satIdx)
{
    if (satIdx >= gDopplerSatCount)
        return;

    gDopplerSatIndex = satIdx;
    uint8_t ch = satChannels[satIdx];

    // Read channel name
    SETTINGS_FetchChannelName(gSatName, ch);

    // Read RX frequency and TX offset (first 8 bytes at base)
    uint8_t data0[8];
    uint16_t base = (uint16_t)ch * 16;
    EEPROM_ReadBuffer(base, data0, 8);

    uint32_t rxFreq = (uint32_t)data0[0] | ((uint32_t)data0[1] << 8) |
                      ((uint32_t)data0[2] << 16) | ((uint32_t)data0[3] << 24);
    uint32_t offset = (uint32_t)data0[4] | ((uint32_t)data0[5] << 8) |
                      ((uint32_t)data0[6] << 16) | ((uint32_t)data0[7] << 24);

    gSatDownlink = rxFreq;

    // Read second 8 bytes (base+8) for CTCSS and offset direction
    uint8_t data1[8];
    EEPROM_ReadBuffer(base + 8, data1, 8);

    // data1[3] bits 3:0 = TX_OFFSET_FREQUENCY_DIRECTION (0=off, 1=add, 2=sub)
    uint8_t offsetDir = data1[3] & 0x0F;

    if (offsetDir == 1)
        gSatUplink = rxFreq + offset;
    else if (offsetDir == 2)
        gSatUplink = rxFreq - offset;
    else
        gSatUplink = rxFreq;

    // data1[2] bits 7:4 = TX CodeType (0=none, 1=CTCSS, 2=DCS)
    // data1[1] = TX Code index
    uint8_t txCodeType = (data1[2] >> 4) & 0x0F;
    uint8_t txCode = data1[1];

    if (txCodeType == 1 && txCode < 50)
    {
        gSatCtcssTx = CTCSS_Options[txCode];
    }
    else
    {
        gSatCtcssTx = 0;
    }

    // data1[4] bits 4:2 = OUTPUT_POWER (0-7)
    if (data1[4] != 0xFF)
        gSatTxPower = (data1[4] >> 2) & 7u;
    else
        gSatTxPower = 2;  // default HIGH

    // Calculate Doppler parameters
    gSatDownShift = DOPPLER_CalcShift(gSatDownlink);
    gSatUpShift   = DOPPLER_CalcShift(gSatUplink);
    gSatTConst    = estimateTConst(gSatDownlink);

    // Set nominal frequencies
    gSatelliteFreq.downlink = gSatDownlink;
    gSatelliteFreq.uplink   = gSatUplink;

    gDopplerTracking = false;
    gDopplerElapsed  = 0;
    gDopplerTimeDiff  = 0;
    gDopplerTimeDiff1 = 0;
}

// ---- Doppler calculation ----

static uint32_t doppler_calc(uint32_t f_nominal, uint16_t max_shift,
                             int32_t dt, uint16_t t_const_x10, bool invert)
{
    int32_t dt_x10 = dt * 10;
    uint32_t dt2 = (uint32_t)(dt_x10 * dt_x10);
    uint32_t t2  = (uint32_t)t_const_x10 * (uint32_t)t_const_x10;
    uint32_t denom = isqrt32(dt2 + t2);
    if (denom == 0) return f_nominal;
    int32_t shift = (int32_t)max_shift * dt_x10 / (int32_t)denom;
    if (invert)
        return (uint32_t)((int32_t)f_nominal + shift);
    else
        return (uint32_t)((int32_t)f_nominal - shift);
}

// ---- Tracking ----

void DOPPLER_StartTracking(uint16_t duration_secs)
{
    gDopplerDuration = duration_secs;
    gDopplerTracking = true;
    gDopplerElapsed  = 0;
}

void DOPPLER_StopTracking(void)
{
    gDopplerTracking = false;
    gDopplerElapsed  = 0;
    gSatelliteFreq.uplink   = gSatUplink;
    gSatelliteFreq.downlink = gSatDownlink;
    gDopplerTimeDiff  = 0;
    gDopplerTimeDiff1 = 0;
}

void DOPPLER_Update(void)
{
    if (!gDopplerMode || !gDopplerTracking)
        return;

    gDopplerElapsed++;

    int32_t t_mid = (int32_t)gDopplerDuration / 2;
    int32_t dt = gDopplerElapsed - t_mid;

    gDopplerTimeDiff  = -gDopplerElapsed;
    gDopplerTimeDiff1 = (int32_t)gDopplerDuration - gDopplerElapsed;

    if (gDopplerElapsed >= (int32_t)gDopplerDuration)
    {
        DOPPLER_StopTracking();
        return;
    }

    gSatelliteFreq.downlink = doppler_calc(gSatDownlink, gSatDownShift, dt, gSatTConst, false);
    gSatelliteFreq.uplink   = doppler_calc(gSatUplink,   gSatUpShift,   dt, gSatTConst, true);
}

// RTC interrupt handler
void HandlerRTC(void)
{
    RTC_IF = 0xFFU;
    extern void RTC_SetTick(void);
    RTC_SetTick();
}

#endif // ENABLE_DOPPLER
