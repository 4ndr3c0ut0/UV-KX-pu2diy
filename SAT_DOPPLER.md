# SAT DOPPLER — Satellite Tracking with Doppler Correction

Amateur satellite tracking module for the Quansheng UV-K5, with real-time automatic frequency correction due to the Doppler effect.

## Features

- Automatic Doppler correction for RX and TX during satellite passes
- Manual frequency adjustment in 1 kHz steps
- Supports FM, USB (SSB), and AM — respects the modulation configured in the channel
- Monitor (squelch override) with SIDE1
- Transmission with CTCSS and power configured in the channel
- No hardware modification or expanded EEPROM required

## How to Configure Satellites

Satellites are normal memory channels whose name starts with $. Configure via the radio menu or CHIRP:

1. Create a memory channel
2. Name it starting with $ (e.g., $SO-50, $ISS, $AO-91)
3. Set the RX frequency as the satellite downlink
4. Set the TX Offset as the difference between uplink and downlink
5. Set the offset direction (+ or -)
6. Set CTCSS TX if required (e.g., 67.0 Hz for SO-50)
7. Set the desired TX power (LOW recommended for satellites)
8. Set the modulation (FM for FM satellites, USB for linear)

### Example: SO-50

| Field | Value |
|-------|-------|
| Name | `$SO-50` |
| RX Freq | 436.795 MHz |
| TX Offset | 290.945 MHz |
| Offset Dir | - (negativo: 436.795 - 290.945 = 145.850 uplink) |
| CTCSS TX | 67.0 Hz |
| Power | LOW |
| Modulation | FM |
| Bandwidth | Wide |

### Example: ISS

| Field | Value |
|-------|-------|
| Name | `$ISS` |
| RX Freq | 437.800 MHz |
| TX Offset | 291.810 MHz |
| Offset Dir | - (437.800 - 291.810 = 145.990 uplink) |
| CTCSS TX | 67.0 Hz |
| Power | LOW |
| Modulation | FM |

### Example: RS-44 (linear/SSB)

| Field | Value |
|-------|-------|
| Nome | `$RS-44` |
| RX Freq | 435.640 MHz |
| TX Offset | 289.675 MHz |
| Offset Dir | - (435.640 - 289.675 = 145.965 uplink) |
| CTCSS TX | nenhum |
| Modulation | USB |

## How to Use

### Enter Doppler Mode

Press F + * on the main screen.

### Main Screen

```
┌────────────────────────────┐
│ SAT DOPPLER         [bat]  │
│          SO-50        1/5  │
│                            │
│ RX 436.79500      +10.9K   │
│ TX 145.85000       -3.6K   │
│                            │
│          TRK 285s          │
│ MON                        │
└────────────────────────────┘
```

### Keys

| Key | Function |
|-------|--------|
| **UP / DOWN** | Select satellite (navigate between $ channels) |
| **MENU** | Start tracking: enter duration in seconds, then MENU to confirm |
| **MENU** (during tracking) | Stop tracking |
| **PTT** | Transmit on Doppler-corrected uplink frequency |
| **SIDE1** | Toggle monitor (open squelch) |
| **SIDE2** | Turn on backlight |
| **1** | RX +1 kHz (manual adjustment) |
| **7** | RX -1 kHz (manual adjustment) |
| **3** | TX +1 kHz (manual adjustment) |
| **9** | TX -1 kHz (manual adjustment) |
| **EXIT** | Exit Doppler mode |

### Operation Flow

1. Press F+* to enter SAT DOPPLER
2. Use UP/DOWN to select the satellite
3. When the satellite appears on the horizon, press MENU
4. Enter the estimated pass duration in seconds (e.g., 480 for 8 minutes)
5. Press MENU to confirm and start automatic tracking
6. The radio adjusts RX and TX every second
7. Press PTT to transmit (with Doppler correction + CTCSS)
8. Tracking stops automatically at the end, or press MENU to stop

### Manual Adjustment (without tracking)

If you prefer manual Doppler correction:
- Use 1/7 to adjust RX in 1 kHz steps
- Use 3/9 to adjust TX in 1 kHz steps
- The shift indicator (e.g., +3.0K) shows the current offset


### How Doppler Calculation Works

The Doppler shift for LEO satellites follows an S-shaped (sigmoid) curve:

```
shift(t) = max_shift × (t - t_meio) / √((t - t_meio)² + T²)
```

- **max_shift** = frequency × orbital_velocity / speed_of_light
  - ~3.6 kHz at 145 MHz
  - ~10.9 kHz at 435 MHz
- **T** = time constant (~143 seconds for satellites at 500 km)
- **t_meio** = midpoint of the pass (TCA)

### RX vs TX Correction

- **RX (downlink)**: satellite approaches → received frequency increases → radio tunes higher
- **TX (uplink)**: satellite approaches → it sees our frequency higher → we transmit lower to compensate

RX and TX shifts differ because they operate on different bands (VHF vs UHF).

## Compilation

```bash
make clean
make all ENABLE_MESSENGER=0 ENABLE_MESSENGER_UART=0 \
         ENABLE_MESSENGER_NOTIFICATION=0 \
         ENABLE_MESSENGER_DELIVERY_NOTIFICATION=0 \
         ENABLE_FEAT_F4HWN_PMR=0 \
         ENABLE_FEAT_F4HWN_NARROWER=0 \
         ENABLE_FEAT_F4HWN_SLEEP=0
```

To disable Doppler: add ENABLE_DOPPLER=0.

## Requirements

- Quansheng UV-K5/K6/5R com EEPROM with standard 8 KB EEPROM
- Toolchain `arm-none-eabi-gcc`
- Python 3 with `crcmod` (to generate .packed.bin)
- No hardware modification required

## Credits
- Doppler concept based on LOSEHU (uv-k5-firmware-custom)
- Base firmware: Joaquim.org (F4HWN/Egzumer fork)
- Satellite data: AMSAT (amsat.org)

