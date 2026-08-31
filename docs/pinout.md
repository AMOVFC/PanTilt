# Locked pinout — `pantiltslide_full_turnkey`

Authoritative source/destination map for the turnkey carrier board going to
PCBWay. `include/config.h` is locked to exactly this table; anything that
disagrees is a wiring error, not a preference.

The ESP32-S3 DevKitC-1 sits in two 22-pin sockets, **J1** and **J2**. Socket
pin numbers below are those sockets, not the module's own pads.

Verified against the schematic netlist: 30 named pins, no collisions, no
reserved GPIOs, and every board pin claimed by firmware.

## Steppers

Stepper letters match the TMC2209 UART addresses and the board silkscreen.

| Socket | GPIO | Signal | Destination | Drives |
|---|---|---|---|---|
| J1.4 | 4 | A_STEP | `U1`.10 STEP | slide |
| J1.5 | 5 | A_DIR | `U1`.9 DIR | slide |
| J1.6 | 6 | B_STEP | `U2`.10 STEP | pan |
| J1.7 | 7 | B_DIR | `U2`.9 DIR | pan |
| J1.12 | 8 | C_STEP | `U3`.10 STEP | tilt |
| J1.15 | 9 | C_DIR | `U3`.9 DIR | tilt |
| J2.4 | 1 | Z_STEP | `U4`.10 STEP | z |
| J2.5 | 2 | Z_DIR | `U4`.9 DIR | z |
| J1.16 | 10 | DRIVER_EN | `U1`–`U4`.16 EN | all, active LOW |

### Address straps

MS1/MS2 select the UART slave address, not microstepping — microstepping is
set over UART by the firmware. All four verified correct in the netlist.

| Driver | Stepper | MS1 (pin 15) | MS2 (pin 14) | Address |
|---|---|---|---|---|
| `U1` | A | GND | GND | 0 |
| `U2` | B | 3.3v | GND | 1 |
| `U3` | C | GND | 3.3v | 2 |
| `U4` | Z | 3.3v | 3.3v | 3 |

## TMC2209 UART

One half-duplex bus. `PDN` (module pin 11) is the chip's single `PDN_UART`
pad; module pin 12 is the same pad through the module's own 1k and is left
**NC**, so the board's resistors are the only ones in circuit.

| Socket | GPIO | Role | Path |
|---|---|---|---|
| J2.17 | 47 | TX | `R10` 100R → `R9` 1k → `UART` |
| J2.7 | 41 | RX | `R11` 100R → `UART` |

`UART` → `U1`.11, `U2`.11, `U3`.11, `U4`.11.

The 1k on TX is what lets a driver pull the line low against the ESP32's
idle-high output. RX needs no such protection — it only listens.

## I²C

| Socket | GPIO | Signal | Destination |
|---|---|---|---|
| J1.17 | 11 | MUX_SDA | `J20`.3 + `R1` 4.7k |
| J1.18 | 12 | MUX_SCL | `J20`.4 + `R2` 4.7k |
| J1.19 | 13 | OLED_SDA | `J23`.3 + `R3` 4.7k |
| J1.20 | 14 | OLED_SCL | `J23`.4 + `R4` 4.7k |

`J20` is a 12-pin TCA9548A breakout: pins 5–8 are its address straps, 9–12
the channel outputs, each with a 2.2k pull-up (`R5`–`R8`).

| Mux channel | Goes to |
|---|---|
| ch0 → `J20`.9/.10 | `J21` AS5600_Pan pins 3/4 |
| ch1 → `J20`.11/.12 | `J22` AS5600_Tilt pins 3/4 |

## Encoders — one per stepper

A/B only. The connectors leave each wheel's push switch as a no-connect, so
the transport buttons are separate parts.

| Socket | GPIO | Signal | Destination |
|---|---|---|---|
| J1.8 | 15 | ENC_A_A | `J29`.1 |
| J1.9 | 16 | ENC_A_B | `J29`.2 |
| J1.10 | 17 | ENC_B_A | `J30`.1 |
| J1.11 | 18 | ENC_B_B | `J30`.2 |
| J2.18 | 21 | ENC_C_A | `J31`.1 |
| J2.10 | 38 | ENC_C_B | `J31`.2 |
| J2.20 | 19 | ENC_Z_A | `J32`.1 |
| J2.19 | 20 | ENC_Z_B | `J32`.2 |

Encoder A defaults to velocity mode (a speed dial for the slide); B, C and Z
default to position mode, stepping their target one click at a time.

## Buttons and limits

| Socket | GPIO | Signal | Destination |
|---|---|---|---|
| J2.16 | 48 | BTN_SET_KEYFRAME | `J33`.1 |
| J1.13 | 3 | BTN_CLEAR_KEYFRAME | `J34`.1 |
| J2.15 | 45 | BTN_PLAY_PAUSE | `J35`.1 |
| J1.14 | 46 | BTN_RESET | `J36`.1 |
| J2.9 | 39 | LIMIT_A_MIN | `J26`.1 |
| J2.8 | 40 | LIMIT_A_MAX | `J27`.1 |
| J2.6 | 42 | LIMIT_Z_HOME | `J41`.1 (silkscreened Aux_Max) |

All active LOW to GND on internal pull-ups.

Stepper Z has **one** switch. Which end it represents is a mechanical choice,
not an electrical one, so it is runtime configuration: set **Homing** to
`limit_min` or `limit_max` in the web UI's Axes tab, no reflash.

It defaults to **`limit_min`** — a height column wants a known zero at the
bottom and travels upward from it. In config a single switch is stored as
`limit_min_pin == limit_max_pin`, and `Axis` reads the homing mode to decide
which end is real, so one switch can never block motion in both directions.

The board connector is silkscreened `Aux_Max`, which now disagrees with the
default. Worth renaming it to `Z_Home` while the schematic is still open.

## Power and unused

`3.3v` J1.1, J1.2 · `VCC_5V` J1.21 (from `J37`) · `GND` J1.22, J2.1, J2.21, J2.22

| Socket | GPIO | Why unused |
|---|---|---|
| J2.14 | 0 | BOOT strap — leave free |
| J2.11–13 | 37, 36, 35 | octal PSRAM on the N16R8 — never connect |
| J1.3 | — | CHIP_PU |
| J2.2, J2.3 | 43, 44 | U0TXD / U0RXD serial console |

GPIO0 is the only spare general-purpose pin left. Everything else is
allocated.

## Verified against the board

Cross-checked against the schematic netlist: **no firmware pin is missing from
the board, and no board pin is unclaimed by firmware.** `J33` Btn_SetKeyframe
is on GPIO_48, and GPIO_1 carries only `U4`'s STEP — the earlier collision
between the two is resolved.

### Two bring-up notes

**Flash over the DevKitC-1's UART port, not its native USB port.** GPIO19/20
carry Encoder Z and are also the S3's native USB D-/D+. Configuring them as
GPIO releases the USB pad automatically (the IDF HAL clears
`USB_SERIAL_JTAG_USB_PAD_ENABLE` for those two pins), so the encoder works —
but native USB stops. Do not plug a cable into that port with the encoder
wired.

GPIO48 also drives the DevKitC-1's onboard addressable RGB LED. Harmless as a
switch input — the WS2812's data pin is high-impedance — provided no LED
library is ever enabled.

## Mechanics behind the numbers

| Stepper | Axis | Drive | steps/unit at 1/16 |
|---|---|---|---|
| A | slide | GT2 belt, 20T pulley → 40 mm/rev | 80 steps/mm |
| B | pan | 2-stage belt 4:1 | 35.6 steps/deg |
| C | tilt | 2-stage belt 4:1 | 35.6 steps/deg |
| Z | z | leadscrew, 8 mm lead | 400 steps/mm |

Z is modelled as a linear axis with `belt_pitch_mm = 8.0` and
`pulley_teeth = 1`, because the linear model computes mm/rev as pitch × teeth.

## A note on what is and is not locked

The pin map above is compiled in as the *factory default*. It is not
hard-coded: every field here — step/dir pins, encoder pins and modes, button
pins and actions, limit pins, TMC UART pins — is runtime config persisted to
LittleFS and editable from the web UI.

That means one firmware image serves this board and the hybrid; they differ
only in their saved config. The defaults describe this board because this is
the one being built, so a fresh flash or a factory reset comes up working on
real hardware rather than needing a JSON import.
