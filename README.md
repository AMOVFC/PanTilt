# Motorized 4-Axis Camera Slider — ESP32-S3

DIY motorized camera slider built around a salvaged 2020 aluminium V-slot
extrusion. Four motorized axes, live knob control, and a programmed keyframe
system for smooth multi-axis cinematic moves. The camera is a phone running the
Blackmagic Camera app; recording is triggered wirelessly over BLE HID.

## Axes

| Axis | Drive | Reduction | Position sensing |
|---|---|---|---|
| Slide | GT2 belt along the 2020 rail, V-wheel carriage | 1:1 | 2× limit switches (min/max) |
| Pan | 2-stage GT2 belt (20T:20T jackshaft, then 20T:80T) | 4:1 | AS5600 absolute encoder — no homing |
| Tilt | Same 2-stage scheme as pan | 4:1 | AS5600 absolute encoder — no homing |
| Z (height) | Integrated leadscrew NEMA 17, anti-backlash nut | 8 mm lead | 1× limit switch, then step-counted |

Pan and tilt read absolute angle on every power-up, so they never need a homing
move. Slide and Z home against physical switches.

## Board versions

Three hardware variants exist in parallel. They share one firmware — the pin
map and mechanics are runtime configuration, not compile-time constants, so the
same binary serves all three.

| Version | Directory | What it is | Status |
|---|---|---|---|
| **Prototype** | [`prototype/`](prototype/README.md) | 3-axis hand-wired build on breakouts, one encoder per axis. The build this project started from. | Working reference |
| **Carrier** | [`pantiltslide/`](pantiltslide/) | 4-axis board of sockets and connectors: ESP32 DevKitC, 4× TMC2209 stepsticks, TCA9548A breakout all plug in. No onboard regulation. | Schematic + PCB exist |
| **Integrated** | [`integrated/`](integrated/README.md) | Everything soldered: ESP32-S3-WROOM-1, 4× TMC2209-LA (QFN28), TCA9548A, 24 V→5 V buck, 3.3 V LDO, USB-C. | Schematic + placement done, **not routed** |

A fourth option is on the table: **integrated ESP32 with TMC2209 stepsticks
still socketed.** That keeps the regulation, USB-C and soldered ESP32 of the
integrated board while leaving the four drivers as plug-in modules — replaceable
after a blown driver, and cheaper to assemble than four QFN28 packages with
their full application circuits.

## Manufacturing constraint

**PCBWay have confirmed they will fabricate the PCB *and* source and assemble
all components, provided the total comes in under USD $150.**

That budget covers everything: bare board, assembly, and every part on the BOM.
It is the governing constraint on which board version gets built — not board
area, not layer count.

If the fully-integrated version cannot land under $150, the fallback order is:

1. Integrated ESP32 + socketed TMC2209 stepsticks (the mix)
2. Carrier board

Motors, PSU, AS5600 modules, OLED, encoders and mechanical parts sit outside
this budget — they are wired in over connectors, not assembled onto the board.

**Nothing has been costed yet.** A real BOM with distributor part numbers and a
total needs to happen before a board is finalised.

## Firmware

See **[docs/firmware.md](docs/firmware.md)** for flashing, bring-up order, the
pin map, and the design notes that matter on the bench.

```bash
pio run -t upload
```

Everything describing a particular machine — pin map, mechanics, speeds, soft
limits, control bindings, network — is runtime configuration persisted to the
ESP32's filesystem and edited from a built-in web UI. `include/config.h` holds
factory defaults only, so reflashing does not discard a working setup, and one
firmware image covers all three board versions.

Read the bring-up order before energising motors. Nothing homes at power-on,
and the shipped soft limits are placeholders.

## TMC2209 drivers over UART

All four drivers share one half-duplex UART bus. Motion runs on STEP/DIR
through FastAccelStepper; UART sets run current and microstepping, and verifies
each driver actually responded.

Why it matters: in standalone mode current is a trimpot and microstepping is a
jumper, so firmware's assumed steps-per-mm can silently disagree with the
hardware — the classic "it moves the wrong distance" bug. Over UART the
firmware is the single source of truth.

In UART mode MS1/MS2 stop selecting microstepping and become address selects:

| Driver | MS1 | MS2 | Address |
|---|---|---|---|
| Slide | LOW | LOW | 0 |
| Pan | HIGH | LOW | 1 |
| Tilt | LOW | HIGH | 2 |
| Z | HIGH | HIGH | 3 |

**On stepstick-based builds this costs bench work.** The Jeanoko carrier boards
do not break out the TMC2209's `PDN_UART` pad, so each of the four drivers needs
a wire soldered directly to that pad on the module. All four tie together into
one bus: RX connects directly, TX through a ~1 kΩ resistor. The integrated board
removes this problem entirely by routing PDN_UART on the PCB.

## Repository layout

```
src/, include/     Firmware (see docs/firmware.md for what owns what)
docs/firmware.md   Flashing, bring-up order, pin map, design notes
prototype/         3-axis hand-wired build — schematic, wiring, its own firmware
pantiltslide/      4-axis carrier board — KiCad project
integrated/        4-axis fully-integrated board — KiCad project
hardware/          Wiring diagram (stale; superseded by the board projects)
```
