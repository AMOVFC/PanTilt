# Camera Slider — Original Prototype (3-Axis) — ESP32-S3 Firmware + Wiring

This is the earlier, simpler build this project started from: 3 motorized
axes instead of 4, TMC2209s on generic breakout boards instead of soldered
carriers, and one dedicated encoder per axis instead of the final rig's
shared jog/angle-encoder scheme. Same ESP32-S3 module as the
[4-axis final rig](../README.md), same TMC2209/AS5600/FastAccelStepper
architecture — this doc and `prototype/` are a sibling build, not a fork of
the final firmware, so cross-porting future changes between the two should
stay easy.

> The final rig's control scheme (shared angle encoder + axis-select
> button) is a placeholder there too — it's slated for its own rework — so
> this prototype does **not** try to match it. It uses one encoder per axis
> instead, which is both simpler to wire and the more obvious mapping for
> "3 encoders."

## Bill of materials (electronics)

| Qty | Part | Notes |
|---|---|---|
| 1 | ESP32-S3 DevKitC-1 N16R8 | 16MB flash / 8MB PSRAM, same as final rig |
| 3 | TMC2209 stepper driver breakout board | Generic module (BTT/Watterott-style), UART pin exposed on the header — connect by Dupont jumper, no bodge wire |
| 3 | NEMA17 stepper motor | Slide, pan, tilt |
| 3 | Rotary encoder (EC11 or similar, with integrated push button) | Slide, pan, tilt — one knob per axis |
| 2 | AS5600 magnetic angle sensor breakout | Pan, tilt |
| 1 | TCA9548A I2C multiplexer breakout | Required — two AS5600s share the fixed I2C address 0x36, can't coexist on one bus without it |
| 2 | Diametrically-magnetized magnet (for AS5600) | One per AS5600 shaft; must be diametric, not axial |
| 2 | Mechanical limit switch | Slide min / slide max only — no Z axis, so no Z switch |
| 1 | SSD1306 OLED (128x64, I2C) | Optional — status display, carried over from the final rig's code. Firmware runs fine with nothing plugged into the OLED bus. |
| — | Stepper motor power supply (12–24V, sized to the 3 motors) | Separate from the ESP32's own 5V/3.3V supply |
| — | ~1kΩ resistor | In-line on the UART TX line for the shared half-duplex bus (skip if your breakout already has one in-line with its UART pin) |

## Pin map

| Function | GPIO | | Function | GPIO |
|---|---|---|---|---|
| Slide STEP / DIR | 4 / 5 | | I2C bus A (mux) SDA / SCL | 11 / 12 |
| Pan STEP / DIR | 6 / 7 | | I2C bus B (OLED, optional) SDA / SCL | 13 / 14 |
| Tilt STEP / DIR | 8 / 9 | | Slide encoder A / B / push | 15 / 16 / 17 |
| Driver EN (shared, all 3) | 10 | | Pan encoder A / B / push | 18 / 21 / 38 |
| TMC UART RX / TX | 41 / 47 | | Tilt encoder A / B / push | 1 / 2 / 42 |
| Slide limit min / max | 39 / 40 | | Spare | 48 |

This is deliberately the final rig's pin map with the Z axis removed and
its freed pins (1, 2, 42 — previously Z STEP/DIR and the Z home switch)
reused for the new Tilt encoder. Everything else — STEP/DIR pins, the mux
I2C bus, the OLED I2C bus, the slide encoder, the slide limit switches, the
TMC UART bus — is numerically identical to the final rig, so a wiring
harness built for one maps directly onto the other wherever the signal
still exists.

Avoided, same reasoning as the final rig: GPIO 0/3/45/46 (boot strapping),
19/20 (native USB), 26–37 (octal PSRAM on N16R8), 43/44 (USB-serial
bridge). GPIO 1 and 2 are safe general-purpose pins on the S3 (unlike
classic ESP32, U0TXD/U0RXD live on 43/44 here, not 1/3).

## Wiring by subsystem

### Power

- Stepper motor power (12–24V, per your motors/driver boards) feeds
  **VMOT** on all 3 TMC2209 breakout boards directly from the supply — not
  through the ESP32.
- ESP32-S3 gets its own 5V (USB or a buck converter off the same supply).
- **Common ground is mandatory**: motor supply GND, ESP32 GND, and every
  breakout board's logic GND must all tie together, or UART/STEP/DIR
  signaling between the ESP32 and the drivers will be unreliable or
  silently wrong.
- TCA9548A, both AS5600 boards, and the OLED (if fitted) run on 3.3V from
  the ESP32.

### TMC2209 breakout boards (STEP/DIR/EN)

| Driver | STEP | DIR | Address (MS1/MS2) |
|---|---|---|---|
| Slide | GPIO4 | GPIO5 | MS1=LOW, MS2=LOW → addr 0 |
| Pan | GPIO6 | GPIO7 | MS1=HIGH, MS2=LOW → addr 1 |
| Tilt | GPIO8 | GPIO9 | MS1=LOW, MS2=HIGH → addr 2 |

`EN` on all 3 boards ties to the single shared GPIO10 line (active low —
firmware drives it low once at boot and never releases it, so holding
torque is never lost). In UART mode MS1/MS2 stop selecting microstepping
and become address straps instead — tie them to GND/3.3V per the table
above so the 3 drivers don't collide on the shared UART bus. Microstepping
and run current are set by firmware over UART at boot instead
(`TmcDrivers.cpp`), then read back and verified — a wrong address strap
shows up as "driver not responding," not a silent misconfiguration.

### TMC2209 UART bus (shared, half-duplex)

Since these breakout boards expose UART/PDN directly on a header pin
(unlike the final rig's bare stepstick carriers, which need a bodge wire
onto the driver IC), this is a straightforward 3-way Dupont bus:

- ESP32 **GPIO41 (RX)** → tied directly to all 3 drivers' UART pins.
- ESP32 **GPIO47 (TX)** → through a ~1kΩ resistor → the same shared bus
  node → all 3 drivers' UART pins. (Skip the discrete resistor only if the
  breakout board already has one in series with its UART pin — check the
  silkscreen/schematic before assuming.)

This is the standard TMC single-wire half-duplex scheme: TX and RX share
one physical wire per driver, with the resistor preventing bus contention
when the ESP32's TX driver and a TMC's TX driver are both technically
capable of driving the line.

### Rotary encoders (slide / pan / tilt)

Each encoder is A/B quadrature + an integrated push button, wired
identically to the ESP32's internal pull-ups (no external resistors
needed) — same as the final rig's jog/angle encoders:

| Encoder | A | B | Push | Default action |
|---|---|---|---|---|
| Slide | GPIO15 | GPIO16 | GPIO17 | Rotate: live jog velocity. Short press: BLE record-state resync. Long press: toggle recording. |
| Pan | GPIO18 | GPIO21 | GPIO38 | Rotate: nudge pan target angle. Short press: recenter pan to 0°. |
| Tilt | GPIO1 | GPIO2 | GPIO42 | Rotate: nudge tilt target angle. Short press: recenter tilt to 0°. |

Push-button pins are pulled up internally and read active-low, so each
switch just needs to short its pin to GND — no external resistor.

### AS5600 + TCA9548A (pan/tilt absolute angle)

Both AS5600 boards sit behind the TCA9548A because the AS5600's I2C
address (0x36) is fixed — two of them can't coexist on one bus without a
mux:

- TCA9548A **SDA/SCL** → ESP32 GPIO11/12, **VCC** → 3.3V, **GND** → GND.
- AS5600 #1 (pan) → TCA9548A channel **0**.
- AS5600 #2 (tilt) → TCA9548A channel **1**.
- Each AS5600 needs a diametrically-magnetized magnet mounted on-axis over
  the sensor, at the correct air gap per the AS5600 datasheet — verify the
  magnet type before gluing anything down; axial magnets don't work here
  and orientation can't compensate for the wrong magnetization.

Pan and tilt read their absolute angle from the AS5600 on every power-up,
so neither needs a homing move — same as the final rig.

### OLED (optional)

If fitted: SDA/SCL → GPIO13/14, VCC → 3.3V, GND → GND, on its own I2C bus
kept isolated from the mux bus. If not fitted, `Display::begin()` detects
the failed init and no-ops for the rest of the session — no firmware
change needed to build without one.

### Slide limit switches

Two mechanical switches (min/max), each wired one leg to its GPIO
(39/40), the other leg to GND. Pulled up internally, read active-low
(idle HIGH, triggered LOW) — no external wiring beyond the switch itself.

## What's different from the final 4-axis rig, and why

| | Final rig | Prototype |
|---|---|---|
| Axes | Slide, pan, tilt, Z | Slide, pan, tilt (no Z motor, no Z limit switch) |
| TMC2209s | 4, soldered carriers (bodge wire for UART) | 3, breakout boards (UART on a header pin) |
| Rotary control | 1 jog encoder (slide) + 1 shared angle encoder with axis-select button (pan/tilt) | 1 dedicated encoder per axis (slide/pan/tilt) — no select button |
| Programmed shots | `Shot`/`ShotSequencer`: keyframe playback with duration-matching + S-curve easing across all 4 axes | Not included — nothing yet needs synchronized multi-axis moves; the interface (`beginProgrammedMove`/`endProgrammedMove`/`isMoveComplete`) is still present on every axis class if this gets added later |
| Web config, BLE record trigger, OLED status, NVS-backed settings | Yes | Yes, unchanged in behavior (separate NVS namespace so the two firmwares' saved settings never collide on the same chip) |

Everything not in that table — `SlideAxis`, `RotaryAxis` (plus one added
method, `setTargetDeg()`), `Mux`, `DebouncedButton`, `BleRecorder`,
`Settings`, `WebConfig` — is either byte-identical to the final rig's
version or differs only in the descriptor rows that don't apply (no Z, no
shot tuning). If the final rig grows a 4th axis on this same encoder-per-
axis scheme later, or this prototype grows toward the final rig's feature
set, the two should reconcile with small, obvious diffs rather than a
rewrite.

## Before first hardware run

Same category of placeholders as the final rig — measure these on the
actual build before trusting any motion:

- **`tmc::*_RMS_MA`** — per-motor run current (`Settings.cpp`), currently a
  conservative 800mA placeholder for all three. Set to 70–85% of each
  motor's nameplate rating.
- **`tmc::R_SENSE_OHMS`** (`config.h`) — 0.11Ω default matches common
  BTT/Watterott breakout boards; check your specific board's sense
  resistor (silkscreen or datasheet) before trusting current math.
- **`calibration::PAN_ZERO_OFFSET_DEG` / `TILT_ZERO_OFFSET_DEG`** — the
  mounted magnet's angle relative to true mechanical zero, measured once
  per build.
- **Pan/tilt soft limits** (`PAN_MIN/MAX_DEG`, `TILT_MIN/MAX_DEG`) —
  placeholders. Neither axis has a limit switch, so these are the only
  thing preventing over-rotation and cable wind-up.
- **`SLIDE_HOME_TOWARD_MIN`** — flip if the slide drives away from its
  home switch instead of toward it.
- **Mechanical ratios** (`mech::SLIDE_BELT_PITCH_MM`, `SLIDE_PULLEY_TEETH`,
  `ROTARY_BELT_RATIO`) — set to match the as-built drivetrain; wrong values
  here make every reported position wrong by a constant factor.

All of the above are editable from the web UI (`http://192.168.4.1`, SSID
`CameraSliderProto`) except pin assignments, I2C addresses, and the TMC
sense resistor/address straps, which stay compile-time for the same reason
the final rig keeps them compile-time: editing them in software can't
rewire anything, it can only make firmware disagree with reality.

## Schematic

`schematic/pantilt_prototype.kicad_sch` is a wiring-reference schematic for this
build (KiCad 9, no PCB) — every connector, breakout board, and net label above
drawn out, cross-checked pin-by-pin against the pin map and wiring-by-subsystem
sections in this doc. It's a hand-wiring diagram, not something meant for
fabrication: connectors and breakouts stand in for the physical modules, wired
point-to-point the way you'd actually run Dupont jumpers on the bench.

## Build

```bash
cd prototype
pio run
```

Upload and monitor:

```bash
cd prototype
pio run --target upload && pio device monitor
```
