/*
 * Doppler satellite tracking application
 * Standalone module — uses memory channels as satellite database.
 * Channels with names starting with "SAT" are treated as satellites.
 */

#ifdef ENABLE_DOPPLER

#include "app/doppler.h"
#include "driver/rtc.h"
#include "driver/bk4819.h"
#include "driver/bk4819-regs.h"
#include "driver/eeprom.h"
#include "driver/keyboard.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/backlight.h"
#include "audio.h"
#include "radio.h"
#include "frequencies.h"
#include "misc.h"
#include "ui/gui.h"
#include "helper/battery.h"
#include "printf.h"
#include <string.h>

// ---- State ----
static bool     dopplerRunning = false;
static bool     isTransmitting = false;
static bool     monitorMode = false;
static uint32_t currentRxFreq = 0;
static bool     inputtingDuration = false;
static uint16_t inputDuration = 0;
static uint8_t  inputDurDigits = 0;

// Keyboard
static KEY_Code_t kbdCurrent = KEY_INVALID;
static KEY_Code_t kbdPrev = KEY_INVALID;
static uint8_t    kbdCounter = 0;

// Register backup for TX
static uint16_t regVault[128];

static void RegBackup(void)
{
    for (int i = 0; i < 128; i++)
        regVault[i] = BK4819_ReadRegister(i);
}

static void RegRestore(void)
{
    for (int i = 0; i < 128; i++)
        BK4819_WriteRegister(i, regVault[i]);
}

// Restore squelch from current VFO settings
static void RestoreSquelch(void)
{
    BK4819_SetupSquelch(
        gRxVfo->SquelchOpenRSSIThresh,
        gRxVfo->SquelchCloseRSSIThresh,
        gRxVfo->SquelchOpenNoiseThresh,
        gRxVfo->SquelchCloseNoiseThresh,
        gRxVfo->SquelchCloseGlitchThresh,
        gRxVfo->SquelchOpenGlitchThresh);
}

// ---- Radio helpers ----

static void SetRxFreq(uint32_t f)
{
    gRxVfo->pRX->Frequency = f;
    BK4819_SetFrequency(f);
    BK4819_PickRXFilterPathBasedOnFrequency(f);
    uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
    BK4819_WriteRegister(BK4819_REG_30, 0);
    BK4819_WriteRegister(BK4819_REG_30, reg);
    currentRxFreq = f;
}

static void ToggleTX(bool on)
{
    if (isTransmitting == on) return;
    isTransmitting = on;

    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, on);

    if (on)
    {
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
        AUDIO_AudioPathOff();

        uint32_t f = gSatelliteFreq.uplink;
        BK4819_SetFrequency(f);
        BK4819_PickRXFilterPathBasedOnFrequency(f);
        RegBackup();

        BK4819_WriteRegister(BK4819_REG_47, 0x6040);
        BK4819_WriteRegister(BK4819_REG_7E, 0x302E);
        BK4819_WriteRegister(BK4819_REG_50, 0x3B20);
        BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
        BK4819_WriteRegister(BK4819_REG_52, 0x028F);
        BK4819_WriteRegister(BK4819_REG_30, 0x0000);
        BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
        BK4819_WriteRegister(BK4819_REG_51, 0x9033);

        if (gSatCtcssTx == 0)
            BK4819_ExitSubAu();
        else
            BK4819_SetCTCSSFrequency(gSatCtcssTx);

        FREQUENCY_Band_t band = FREQUENCY_GetBand(f);
        uint8_t txp[3];
        EEPROM_ReadBuffer(0x1ED0 + (band * 16) + (gSatTxPower * 3), txp, 3);
        BK4819_SetupPowerAmplifier(txp[2], f);
        BK4819_DisableDTMF();
        BK4819_DisableScramble();
    }
    else
    {
        BK4819_GenTail(4);
        BK4819_WriteRegister(BK4819_REG_51, 0x904A);
        BK4819_SetupPowerAmplifier(0, 0);
        RegRestore();
        BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
        BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);

        // Restore full radio state for RX
        currentRxFreq = gSatelliteFreq.downlink;
        gRxVfo->pRX->Frequency = currentRxFreq;
        RADIO_SetupRegisters(true);
    }
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, !on);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, on);
}

// Check BK4819 interrupts for squelch open/close
static void CheckSquelch(void)
{
    if (monitorMode)
        return;  // monitor overrides squelch

    // Check if interrupt pending
    if (!(BK4819_ReadRegister(BK4819_REG_0C) & 1u))
        return;

    // Read and clear interrupt status
    BK4819_WriteRegister(BK4819_REG_02, 0);
    uint16_t intBits = BK4819_ReadRegister(BK4819_REG_02);

    if (intBits & BK4819_REG_3F_SQUELCH_LOST)
    {
        // Signal present — open audio
        if (gRxVfo->Modulation == MODULATION_FM)
            BK4819_SetAF(BK4819_AF_FM);
        else
            BK4819_SetAF(BK4819_AF_BASEBAND2);
        AUDIO_AudioPathOn();
        gEnableSpeaker = true;
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
    }

    if (intBits & BK4819_REG_3F_SQUELCH_FOUND)
    {
        // Signal gone — mute audio
        AUDIO_AudioPathOff();
        gEnableSpeaker = false;
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    }
}

// ---- Keyboard ----

static KEY_Code_t PollKey(void)
{
    KEY_Code_t k = KEYBOARD_Poll();
    if (k == KEY_INVALID && !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT))
        k = KEY_PTT;
    return k;
}

static KEY_Code_t ReadKey(void)
{
    kbdPrev = kbdCurrent;
    kbdCurrent = PollKey();

    if (isTransmitting && kbdCurrent != KEY_PTT)
        ToggleTX(false);

    if (kbdCurrent == KEY_INVALID) { kbdCounter = 0; return KEY_INVALID; }

    if (kbdCurrent == kbdPrev)
    {
        kbdCounter++;
        if (kbdCounter == 3 || (kbdCounter > 16 && (kbdCounter % 3) == 0))
        {
            SYSTEM_DelayMs(20);
            return kbdCurrent;
        }
        SYSTEM_DelayMs(20);
        return KEY_INVALID;
    }
    kbdCounter = 1;
    SYSTEM_DelayMs(20);
    return KEY_INVALID;
}

// ---- Rendering ----

static void RenderMain(void)
{
    UI_SetFont(UI_FONT_5_TR);
    UI_DrawString(UI_TEXT_ALIGN_LEFT, 0, 0, 6, true, false, false, "SAT DOPPLER");
    UI_DrawBatteryIcon(BATTERY_VoltsToPercent(gBatteryVoltageAverage), 114, 0);

    if (gDopplerSatCount == 0)
    {
        UI_SetFont(UI_FONT_8_TR);
        UI_DrawString(UI_TEXT_ALIGN_CENTER, 0, 127, 28, true, false, false, "NO SAT CHANNELS");
        UI_SetFont(UI_FONT_5_TR);
        UI_DrawString(UI_TEXT_ALIGN_CENTER, 0, 127, 40, true, false, false, "NAME CHANNELS $*");
        return;
    }

    // Satellite name
    UI_SetFont(UI_FONT_8_TR);
    UI_DrawString(UI_TEXT_ALIGN_CENTER, 0, 127, 20, true, false, false, gSatName + 1);

    UI_SetFont(UI_FONT_5_TR);
    if (gDopplerSatCount > 1)
        UI_DrawStringf(UI_TEXT_ALIGN_RIGHT, 0, 127, 20, true, false, false,
            "%u/%u", gDopplerSatIndex + 1, gDopplerSatCount);

    // Frequencies + Doppler shifts on same lines
    UI_SetFont(UI_FONT_8_TR);
    UI_DrawStringf(UI_TEXT_ALIGN_LEFT, 0, 0, 33, true, false, false,
        "RX %u.%05u", gSatelliteFreq.downlink / 100000, gSatelliteFreq.downlink % 100000);
    UI_DrawStringf(UI_TEXT_ALIGN_LEFT, 0, 0, 43, true, false, false,
        "TX %u.%05u", gSatelliteFreq.uplink / 100000, gSatelliteFreq.uplink % 100000);

    // RX Doppler shift
    UI_SetFont(UI_FONT_5_TR);
    {
        int32_t dsRx = (int32_t)gSatelliteFreq.downlink - (int32_t)gSatDownlink;
        if (dsRx != 0)
        {
            int32_t da = dsRx < 0 ? -dsRx : dsRx;
            UI_DrawStringf(UI_TEXT_ALIGN_RIGHT, 0, 127, 33, true, false, false,
                "%c%ld.%01ldK", dsRx >= 0 ? '+' : '-', (long)(da / 100), (long)((da % 100) / 10));
        }
    }
    // TX Doppler shift
    {
        int32_t dsTx = (int32_t)gSatelliteFreq.uplink - (int32_t)gSatUplink;
        if (dsTx != 0)
        {
            int32_t da = dsTx < 0 ? -dsTx : dsTx;
            UI_DrawStringf(UI_TEXT_ALIGN_RIGHT, 0, 127, 43, true, false, false,
                "%c%ld.%01ldK", dsTx >= 0 ? '+' : '-', (long)(da / 100), (long)((da % 100) / 10));
        }
    }

    // Bottom status line
    if (isTransmitting)
        UI_DrawStringf(UI_TEXT_ALIGN_CENTER, 0, 127, 55, true, false, false, ">>> TX >>>");
    else if (inputtingDuration)
        UI_DrawStringf(UI_TEXT_ALIGN_CENTER, 0, 127, 55, true, false, false,
            inputDurDigits > 0 ? "%us M=GO" : "SEC? 0-9");
    else if (!gDopplerTracking)
    {
        int32_t rxOff = (int32_t)gSatelliteFreq.downlink - (int32_t)gSatDownlink;
        if (rxOff != 0)
            UI_DrawStringf(UI_TEXT_ALIGN_CENTER, 0, 127, 55, true, false, false, "MAN 1/7 3/9");
        else
            UI_DrawStringf(UI_TEXT_ALIGN_CENTER, 0, 127, 55, true, false, false, "M=TRK");
    }
    else if (gDopplerTimeDiff1 > 0)
        UI_DrawStringf(UI_TEXT_ALIGN_CENTER, 0, 127, 55, true, false, false, "TRK %lds", (long)gDopplerTimeDiff1);
    else
        UI_DrawStringf(UI_TEXT_ALIGN_CENTER, 0, 127, 55, true, false, false, "ENDED");

    // Duration input (overwrites status line)
    if (inputtingDuration && inputDurDigits > 0)
    {
        UI_SetFont(UI_FONT_8_TR);
        UI_DrawStringf(UI_TEXT_ALIGN_CENTER, 0, 127, 55, true, false, false, "%u SEC", inputDuration);
    }

    if (monitorMode)
    {
        UI_SetFont(UI_FONT_5_TR);
        UI_DrawString(UI_TEXT_ALIGN_LEFT, 0, 0, 55, true, false, false, "MON");
    }
}

static void Render(void)
{
    UI_ClearDisplay();
    UI_SetBlackColor();
    RenderMain();
    UI_UpdateDisplay();
}

// ---- Key handler ----

static void HandleKey(KEY_Code_t key)
{
    if (inputtingDuration)
    {
        if (key <= KEY_9 && inputDurDigits < 4)
        {
            inputDuration = inputDuration * 10 + (key - KEY_0);
            inputDurDigits++;
        }
        else if (key == KEY_EXIT)
            inputtingDuration = false;
        else if (key == KEY_MENU && inputDuration > 0)
        {
            inputtingDuration = false;
            DOPPLER_StartTracking(inputDuration);
        }
        return;
    }

    switch (key)
    {
    // Manual Doppler tuning (1 kHz step, only when not auto-tracking)
    case KEY_1:  // RX up 1 kHz
        if (!gDopplerTracking && gDopplerSatCount > 0)
        {
            gSatelliteFreq.downlink += 100;  // 100 units = 1 kHz
            SetRxFreq(gSatelliteFreq.downlink);
        }
        break;
    case KEY_7:  // RX down 1 kHz
        if (!gDopplerTracking && gDopplerSatCount > 0)
        {
            gSatelliteFreq.downlink -= 100;
            SetRxFreq(gSatelliteFreq.downlink);
        }
        break;
    case KEY_3:  // TX up 1 kHz
        if (!gDopplerTracking && gDopplerSatCount > 0)
            gSatelliteFreq.uplink += 100;
        break;
    case KEY_9:  // TX down 1 kHz
        if (!gDopplerTracking && gDopplerSatCount > 0)
            gSatelliteFreq.uplink -= 100;
        break;
    case KEY_UP:
        if (gDopplerSatIndex + 1 < gDopplerSatCount)
        {
            DOPPLER_LoadSatellite(gDopplerSatIndex + 1);
            SetRxFreq(gSatelliteFreq.downlink);
        }
        break;
    case KEY_DOWN:
        if (gDopplerSatIndex > 0)
        {
            DOPPLER_LoadSatellite(gDopplerSatIndex - 1);
            SetRxFreq(gSatelliteFreq.downlink);
        }
        break;
    case KEY_MENU:
        if (gDopplerTracking)
            DOPPLER_StopTracking();
        else
        {
            inputtingDuration = true;
            inputDuration = 0;
            inputDurDigits = 0;
        }
        break;
    case KEY_PTT:
        if (gDopplerSatCount > 0)
            ToggleTX(true);
        break;
    case KEY_SIDE1:
        monitorMode = !monitorMode;
        if (monitorMode)
        {
            // Open squelch, enable audio, green LED on
            BK4819_SetupSquelch(0, 0, 0, 0, 0, 0);
            if (gRxVfo->Modulation == MODULATION_FM)
                BK4819_SetAF(BK4819_AF_FM);
            else
                BK4819_SetAF(BK4819_AF_BASEBAND2);
            AUDIO_AudioPathOn();
            BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
            gEnableSpeaker = true;
        }
        else
        {
            // Restore normal radio state with proper squelch
            BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
            AUDIO_AudioPathOff();
            gEnableSpeaker = false;
            gRxVfo->pRX->Frequency = currentRxFreq;
            RADIO_SetupRegisters(true);
            if (gRxVfo->Modulation == MODULATION_FM)
                BK4819_SetAF(BK4819_AF_FM);
            else
                BK4819_SetAF(BK4819_AF_BASEBAND2);
        }
        break;
    case KEY_SIDE2:
        BACKLIGHT_TurnOn();
        break;
    case KEY_EXIT:
        dopplerRunning = false;
        break;
    default:
        break;
    }
}

// ---- Public entry point ----

void APP_RunDoppler(void)
{
    DOPPLER_Init();
    RTC_INIT();

    isTransmitting = false;
    monitorMode = false;
    inputtingDuration = false;
    dopplerRunning = true;
    kbdCurrent = KEY_INVALID;
    kbdPrev = KEY_INVALID;
    kbdCounter = 0;

    if (gDopplerSatCount > 0)
    {
        currentRxFreq = gSatelliteFreq.downlink;

        // Set satellite frequency in the active VFO so RADIO_SetupRegisters uses it
        gRxVfo->pRX->Frequency = currentRxFreq;
        // Keep modulation and bandwidth from the channel as configured by the user

        // Full radio setup: squelch, AF, interrupts, FUNCTION_FOREGROUND
        RADIO_SetupRegisters(true);
        // SetupRegisters leaves AF muted; set appropriate AF mode
        if (gRxVfo->Modulation == MODULATION_FM)
            BK4819_SetAF(BK4819_AF_FM);
        else
            BK4819_SetAF(BK4819_AF_BASEBAND2);  // USB/SSB
    }

    Render();

    while (dopplerRunning)
    {
        // Process BK4819 squelch interrupts
        CheckSquelch();

        if (RTC_HasTick())
        {
            DOPPLER_Update();
            if (!isTransmitting && gDopplerTracking && gSatelliteFreq.downlink != 0)
            {
                int32_t diff = (int32_t)gSatelliteFreq.downlink - (int32_t)currentRxFreq;
                if (diff > 100 || diff < -100)
                    SetRxFreq(gSatelliteFreq.downlink);
            }
        }

        KEY_Code_t key = ReadKey();
        if (key != KEY_INVALID)
            HandleKey(key);

        Render();
        SYSTEM_DelayMs(10);
    }

    ToggleTX(false);
    RTC_Stop();
    DOPPLER_StopTracking();
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
    gDopplerMode = false;
}

#endif // ENABLE_DOPPLER
