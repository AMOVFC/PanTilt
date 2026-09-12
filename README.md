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

## Where things live

| Path | What it is |
|---|---|
| **[`hardware/`](hardware/)** | The production PCB — `pantilt-controller`, a KiCad 10 project. Carrier for an ESP32-S3 DevKitC-1 + 4× TMC2209 sockets + I²C mux + encoders + OLED, with fused inputs and TVS protection on the 24 V / 5 V rails. Board renders, full BOM with LCSC part numbers, and the DFM review scripts are in there. |
| **[`software/`](software/)** | The firmware — a PlatformIO project for the ESP32-S3. Motion, TMC2209 UART control, encoders, sequencer, and a self-hosted web UI. Build and flash from inside `software/`. |
| **[`legacy/`](legacy/)** | Every earlier design generation — nothing the current firmware or board depends on. |

### Legacy generations

| Variant | Path | Why it stopped |
|---|---|---|
| Prototype | [`legacy/prototype/`](legacy/prototype/README.md) | 3-axis hand-wired origin of the project, with its own firmware |
| Integrated | [`legacy/integrated/`](legacy/integrated/README.md) | everything soldered incl. 4× QFN28; never routed |
| Hybrid | [`legacy/hybrid/`](legacy/hybrid/README.md) | soldered MCU + power, plug-in drivers |
| Turnkey / -mini / full / pwr | [`legacy/pantiltslide/`](legacy/pantiltslide/README.md) | earlier carrier boards; `hardware/pantilt-controller` descends from turnkey-mini |
| TMC socket generator | [`legacy/tmc2209-socket-gen/`](legacy/tmc2209-socket-gen/) | script + library for the custom TMC2209 socket footprint |

## The board in one paragraph

An ESP32-S3 DevKitC-1 drops into two 22-pin sockets. Four TMC2209
SilentStepSticks and a TCA9548A breakout plug into pin sockets; two AS5600
encoder heads, four panel encoders, four buttons and the limit switches plug in
over JST-XH; the OLED and the six spare mux channels break out on 1×4 pin
headers. Both supply rails are fused — a 4 A MINI blade fuse on the 24 V motor
rail, a 1.5 A Nano² on the 5 V logic rail — with a TVS crowbar downstream of
each fuse and a bleeder across the 24 V bulk caps so hand-driving an axis while
the board is off can't leave the capacitors charged. Details and BOM:
[`hardware/README.md`](hardware/README.md).

## The firmware in one paragraph

Motion runs on STEP/DIR through FastAccelStepper; the TMC2209s are configured
and health-checked over one shared half-duplex UART, so run current and
microstepping are set by firmware rather than by trimpots and jumpers. Anything
machine-specific — pin map, mechanics, speeds, soft limits, control bindings,
network — is runtime config on the ESP32's filesystem, edited from a web UI
that is compiled into the image. `software/include/config.h` holds factory
defaults only. Bring-up order and bench notes:
[`software/docs/firmware.md`](software/docs/firmware.md).

## Manufacturing

PCBWay fabricate the bare 4-layer PCB. Components are **hand-assembled** —
PCBWay only warranty parts they also place, so the BOM is kept to common,
reusable Uniroyal / Yageo passives and widely-stocked LCSC parts. Motors, PSU,
AS5600 modules, OLED, encoders and mechanical parts wire in over connectors.

## Quick start

```bash
cd software
pio run -t upload && pio device monitor
```

Flash through the DevKitC-1's **`UART`** USB socket (not `USB`). The board then
comes up as a WiFi AP — SSID `CamSlider`, password `sliderpad`, UI at
`http://192.168.4.1`. Full instructions and the mandatory pre-motor bring-up
sequence: [`software/README.md`](software/README.md).
