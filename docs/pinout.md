# Locked pinout — hybrid controller rev A

Authoritative source/destination map for the board going to PCBWay. The
firmware in `include/config.h` is locked to exactly this table; anything that
disagrees is a wiring error, not a preference.

Module pin numbers are ESP32-S3-WROOM-1-N16R8 physical pads, taken from the
schematic netlist (`U4`).

## Signals

| Module pin | GPIO | Signal | Destination | Dest. pin | Dir |
|---|---|---|---|---|---|
| 4  | 4  | SLIDE_STEP | `U3` TMC2209 (addr 0) | 10 `STEP` | out |
| 5  | 5  | SLIDE_DIR  | `U3` TMC2209 (addr 0) | 9 `DIR`   | out |
| 6  | 6  | PAN_STEP   | `U5` TMC2209 (addr 1) | 10 `STEP` | out |
| 7  | 7  | PAN_DIR    | `U5` TMC2209 (addr 1) | 9 `DIR`   | out |
| 12 | 8  | TILT_STEP  | `U6` TMC2209 (addr 2) | 10 `STEP` | out |
| 17 | 9  | TILT_DIR   | `U6` TMC2209 (addr 2) | 9 `DIR`   | out |
| 39 | 1  | Z_STEP     | `U7` TMC2209 (addr 3) | 10 `STEP` | out |
| 38 | 2  | Z_DIR      | `U7` TMC2209 (addr 3) | 9 `DIR`   | out |
| 18 | 10 | DRIVER_EN  | `U3`,`U5`,`U6`,`U7`   | 16 `EN`   | out, active LOW |
| 24 | 47 | TMC_UART_TX | `R10` 1 kΩ → `TMC_UART` bus | `U3/U5/U6/U7` 12 `UART` | out |
| 34 | 41 | TMC_UART_RX | `TMC_UART` bus (direct) | `U3/U5/U6/U7` 11 `PDN` | in/out |
| 19 | 11 | I2C_A_SDA  | `J200` TCA9548A mux + `R200` 4.7 k | 3 | bidir |
| 20 | 12 | I2C_A_SCL  | `J200` TCA9548A mux + `R201` 4.7 k | 4 | out |
| 21 | 13 | I2C_B_SDA  | `J203` OLED + `R202` 4.7 k | 3 | bidir |
| 22 | 14 | I2C_B_SCL  | `J203` OLED + `R203` 4.7 k | 4 | out |
| 8  | 15 | ENC_JOG_A  | `J204` Enc_Jog | 1 | in, pull-up |
| 9  | 16 | ENC_JOG_B  | `J204` Enc_Jog | 2 | in, pull-up |
| 10 | 17 | ENC_JOG_SW | `J204` Enc_Jog | 4 | in, pull-up |
| 11 | 18 | ENC_ANGLE_A  | `J205` Enc_Angle | 1 | in, pull-up |
| 23 | 21 | ENC_ANGLE_B  | `J205` Enc_Angle | 2 | in, pull-up |
| 31 | 38 | ENC_ANGLE_SW | `J205` Enc_Angle | 4 | in, pull-up |
| 32 | 39 | LIMIT_SLIDE_MIN | `J206` Limit_Min | 1 | in, pull-up, active LOW |
| 33 | 40 | LIMIT_SLIDE_MAX | `J207` Limit_Max | 1 | in, pull-up, active LOW |
| 35 | 42 | LIMIT_Z_HOME    | `J208` Limit_Z_Home | 1 | in, pull-up, active LOW |
| 13 | 19 | USB_D−     | `J3` USB-C | A7 / B7 | bidir |
| 14 | 20 | USB_D+     | `J3` USB-C | A6 / B6 | bidir |
| 27 | 0  | BOOT       | `SW4` + `R11` 10 k pull-up | — | in, strap |
| 3  | EN | CHIP_EN    | `SW3` + `R9` 10 k + `C12` | — | in |

**Unused and free:** GPIO3 (pad 15), GPIO45 (pad 26), GPIO46 (pad 16),
GPIO48 (pad 25). GPIO35/36/37 (pads 28–30) are the octal PSRAM bus on an
N16R8 and must stay unconnected. GPIO43/44 (pads 37/36) are the USB-serial
console.

## Mux channels

`J200` is an off-the-shelf TCA9548A breakout, not a soldered part.

| J200 pin | Net | Goes to |
|---|---|---|
| 1 | +3V3 | — |
| 2 | GND | — |
| 3 | GPIO11 (SDA in) | ESP32 pad 19 |
| 4 | GPIO12 (SCL in) | ESP32 pad 20 |
| 5 | I2C_PAN_SDA (ch0) | `J201` AS5600_Pan pin 3 |
| 6 | I2C_PAN_SCL (ch0) | `J201` AS5600_Pan pin 4 |
| 7 | I2C_TILT_SDA (ch1) | `J202` AS5600_Tilt pin 3 |
| 8 | I2C_TILT_SCL (ch1) | `J202` AS5600_Tilt pin 4 |

## Driver address straps

MS1/MS2 select the UART slave address, not microstepping. Microstepping is set
over UART by the firmware.

| Driver | Axis | MS1 (pin 15) | MS2 (pin 14) | Address |
|---|---|---|---|---|
| `U3` | Slide | GND | GND | 0 |
| `U5` | Pan | 3V3 | GND | 1 |
| `U6` | Tilt | GND | 3V3 | 2 |
| `U7` | Z | 3V3 | 3V3 | 3 |

## Motor connectors

| Connector | Driver | Phases |
|---|---|---|
| Motor_Slide | `U3` | SLIDE_M1A/M1B (pins 4/3), SLIDE_M2A/M2B (pins 5/6) |
| Motor_Pan | `U5` | PAN_M1A/M1B, PAN_M2A/M2B |
| Motor_Tilt | `U6` | TILT_M1A/M1B, TILT_M2A/M2B |
| Motor_Z | `U7` | Z_M1A/M1B, Z_M2A/M2B |

## Faults in the current schematic

Checked against `hybrid/schematic/` as of the `fullpcb filled out` commit.
These are why ERC reports 135 violations and why the netlist shows GPIO4–10
reaching nothing.

**1. Underscored net names do not connect.** The driver sheet uses `GPIO_4`,
`GPIO_5`, `GPIO_10`, `GPIO_41`, `GPIO_42`, `GPIO_47`, `GPIO_48`. Every other
sheet uses `GPIO4`, `GPIO5`, … with no underscore. KiCad treats these as
different nets, so all four drivers are currently orphaned from the MCU — no
STEP, no DIR, no EN, no UART.

**2. A blind find-and-replace of `GPIO_` → `GPIO` makes it worse.** It would
create two hard conflicts:

| Pin | Driver sheet wants it for | Already used for | Result |
|---|---|---|---|
| GPIO47 | `U7` STEP (Z) | TMC UART TX (`R10`) | two outputs shorted |
| GPIO42 | `U3/U5/U6/U7` PDN (UART RX) | `J208` Z home limit switch | UART bus fights a switch |

**3. The fix.** On the driver sheet only:

| `U7` (Z) pin | Change from | Change to |
|---|---|---|
| 10 `STEP` | `GPIO_47` | `GPIO1` |
| 9 `DIR` | `GPIO_48` | `GPIO2` |

| All four drivers | Change from | Change to |
|---|---|---|
| 12 `UART` | `GPIO_41` | `TMC_UART` |
| 11 `PDN` | `GPIO_42` | `TMC_UART` |
| 16 `EN` | `GPIO_10` | `GPIO10` |
| 10 `STEP` / 9 `DIR` | `GPIO_4`…`GPIO_9` | `GPIO4`…`GPIO9` |

Pins 11 and 12 both land on the same `TMC_UART` node — that is the standard
single-wire half-duplex arrangement, and `R10` (1 kΩ) is already in series
from GPIO47.

**4. No motor connectors exist.** The driver sheet defines the phase nets
(`SLIDE_M1A`, `PAN_M1A`, `AUX_M1A`, …) but the netlist contains no connector
for any of them. Four 4-pin connectors need adding, one per driver. Rename the
`AUX_*` nets to `Z_*` while you are there.

**5. `U7`'s phase nets are still named `AUX_*`.** Cosmetic, but the firmware,
this document and the other three drivers all call that axis Z.

## Mechanics behind the numbers

| Axis | Drive | steps/unit at 1/16 |
|---|---|---|
| Slide | GT2 belt, 20 T pulley → 40 mm/rev | 80 steps/mm |
| Pan | 2-stage belt 4:1 | 35.6 steps/deg |
| Tilt | 2-stage belt 4:1 | 35.6 steps/deg |
| Z | leadscrew, 8 mm lead | 400 steps/mm |

Z is modelled as a linear axis with `belt_pitch_mm = 8.0` and
`pulley_teeth = 1`, because the linear model computes mm/rev as
pitch × teeth. Z has a single home switch, expressed in config as
`limit_min_pin == limit_max_pin`; the firmware treats a matching pair as
"no max switch" so a triggered home cannot block motion in both directions.
