# Motorized 4-Axis Camera Slider — ESP32-S3 Firmware

Custom firmware for a DIY motorized camera slider built around a salvaged 2020
aluminum V-slot extrusion. Four motorized axes, live knob control, and a
programmed keyframe system for smooth multi-axis cinematic moves.

The camera is a phone running the Blackmagic Camera app; recording is triggered
wirelessly over BLE HID.

## Axes

| Axis | Drive | Reduction | Position sensing |
|---|---|---|---|
| Slide | GT2 belt along the 2020 rail, V-wheel carriage | 1:1 | 2x limit switches (min/max) |
| Pan | 2-stage GT2 belt (20T:20T jackshaft, then 20T:80T) | 4:1 | AS5600 absolute encoder — no homing |
| Tilt | Same 2-stage scheme as pan | 4:1 | AS5600 absolute encoder — no homing |
| Z (height) | Integrated leadscrew NEMA 17, anti-backlash nut | leadscrew lead | 1x limit switch, then step-counted |

Pan and tilt read absolute angle on every power-up, so they never need a homing
move. Slide and Z home against physical switches at startup.

## Firmware architecture

```
main.cpp          Non-blocking loop: no delay() anywhere. Owns the two knobs,
                  the two push-buttons, and dispatches to everything below.
config.h          Single source of truth: pin map, driver config, mechanical
                  ratios, motion limits, calibration values.
TmcDrivers.*      Boot-time UART configuration for all 4 TMC2209s.
SlideAxis.*       Homing + live velocity jog from the jog encoder.
RotaryAxis.*      Pan/tilt: angle setpoints, AS5600 absolute position,
                  idle-only drift correction.
ZAxis.*           Homing + step-counted position. No live jog (no spare knob).
Shot.*            Keyframe playback: duration-matched multi-axis moves with
                  jerk-limited S-curve easing.
Mux.*             TCA9548A channel select + AS5600 read, fused into one call.
Display.*         OLED status on its own isolated I2C bus.
BleRecorder.*     BLE HID record toggle + state resync.
DebouncedButton.h Short/long press discrimination, non-blocking.
```

Each axis class exposes the same small programmed-move interface
(`beginProgrammedMove` / `endProgrammedMove` / `isMoveComplete`), which is how
`ShotSequencer` drives all four without knowing their internals. Live control
and programmed shots are separate code paths — axes self-gate, so a jog input
during a shot can't fight the sequencer for the same stepper.

## Programmed shots

A shot is an ordered array of `Keyframe`s. Each keyframe holds a target for all
four axes, an optional duration, and an easing type.

**Duration matching.** Naive multi-axis moves look wrong on camera: short-travel
axes finish early and sit idle while others are still moving, producing a
visible settling moment mid-shot. Instead, each axis's natural travel time is
computed from its own speed/accel limits, the longest one (or the keyframe's
explicit duration, whichever is greater) becomes the move's governing duration,
and every other axis is time-dilated to match. All four start together and
arrive together.

**S-curve easing.** `EASE_IN_OUT` uses a quintic minimum-jerk profile
(`6x⁵ − 15x⁴ + 10x³`), which has zero velocity *and* zero acceleration at both
endpoints — acceleration ramps smoothly instead of snapping on the way a
trapezoid does. FastAccelStepper has no native jerk-limited ramp, so the curve is
sampled into timed waypoints and replayed as short moves whose target speeds
trace the curve. Waypoints are dispatched on a wall clock, not on per-axis
completion, so all axes stay locked to one shared timeline. The final waypoint
lands exactly on the keyframe target, so smoothness costs no precision.

`LINEAR` is a single sharp trapezoidal move, for fast repositioning between
takes rather than for filming.

Trigger a test shot over serial: `s` to run, `c` to cancel.

## TMC2209 drivers over UART

All four drivers share one half-duplex UART bus. Motion still runs on STEP/DIR
through FastAccelStepper — UART is used only at boot, to set run current and
microstepping in firmware and to verify each driver actually responded. This is
the same config/motion split Marlin uses.

Why it matters: in standalone mode, current is a trimpot you set with a
multimeter and microstepping is a jumper, so firmware's assumed steps-per-mm can
silently disagree with the hardware — the classic "it moves the wrong distance"
bug. Here `tmc::MICROSTEPS` is the single source of truth, written to the driver
and then **read back and verified** at startup. A mismatch is a loud error, not
a silent 4x position error.

In UART mode MS1/MS2 no longer select microstepping — they become address
straps:

| Driver | MS1 | MS2 | Address |
|---|---|---|---|
| Slide | LOW | LOW | 0 |
| Pan | HIGH | LOW | 1 |
| Tilt | LOW | HIGH | 2 |
| Z | HIGH | HIGH | 3 |

If UART wiring fails, motion still works (STEP/DIR is independent) but the
firmware prints a warning that position math is untrustworthy until fixed.

## Controls

| Input | Action |
|---|---|
| Jog encoder (rotate) | Live slide velocity — CW positive, CCW negative, center = stop |
| Jog encoder (short press) | Resync BLE record state to the phone, no keypress sent |
| Jog encoder (long press) | Toggle recording (sends BLE volume-up) |
| Angle encoder (rotate) | Adjust the selected rotary axis's target angle |
| Angle encoder (press) | Switch selection between pan and tilt |
| Serial `s` / `c` | Start / cancel the programmed test shot |

The Blackmagic Camera app's volume-button binding is a *toggle*, not distinct
start/stop commands, so firmware tracks `isRecording` as its own belief. The
short-press resync exists for when that belief drifts from reality.

## Pin map

| Function | GPIO | | Function | GPIO |
|---|---|---|---|---|
| Slide STEP / DIR | 4 / 5 | | I2C bus A (mux) SDA / SCL | 11 / 12 |
| Pan STEP / DIR | 6 / 7 | | I2C bus B (OLED) SDA / SCL | 13 / 14 |
| Tilt STEP / DIR | 8 / 9 | | Jog encoder A / B / push | 15 / 16 / 17 |
| Z STEP / DIR | 1 / 2 | | Angle encoder A / B / push | 18 / 21 / 38 |
| Driver EN (shared) | 10 | | Slide limit min / max | 39 / 40 |
| TMC UART RX / TX | 41 / 47 | | Z limit (home) | 42 |

Avoided: GPIO 0/3/45/46 (boot strapping), 19/20 (native USB), 26–37 (octal
PSRAM on N16R8), 43/44 (USB-serial bridge). GPIO48 is the only pin left free,
reserved for an optional Z max-travel switch.

Two separate I2C buses by necessity, not preference: the AS5600's address
(0x36) is fixed and non-configurable, so two of them can't share a bus without
the TCA9548A multiplexer. The OLED was kept on its own bus to stay independent
of the mux.

## Build

```bash
pio run
```

Upload and monitor:

```bash
pio run --target upload && pio device monitor
```

Current build: ~15% flash, ~15% RAM on a 16MB/8MB N16R8.

## Before first hardware run

Values that cannot be derived from the code and must be measured on the
physical build. Several are placeholders that will produce wrong motion if left
as-is:

- **`tmc::*_RMS_MA`** — per-motor run current, typically 70–85% of the motor's
  nameplate rating. Currently a conservative 800mA placeholder for all four.
- **`calibration::PAN_ZERO_OFFSET_DEG` / `TILT_ZERO_OFFSET_DEG`** — the mounted
  magnet's angle relative to true mechanical zero. Arbitrary per build,
  measured once.
- **Pan/tilt soft limits** (`PAN_MIN/MAX_DEG`, `TILT_MIN/MAX_DEG`) —
  placeholders. Neither axis has a limit switch, so these are the only thing
  preventing over-rotation and cable wind-up.
- **Homing directions** (`SLIDE_HOME_TOWARD_MIN`, `Z_HOME_DIR_FORWARD`) — flip
  if an axis drives away from its switch instead of toward it.

Confirmed against the as-built rig: `Z_LEAD_MM` = 8mm (giving 1600 steps/mm)
and `Z_MAX_MM` = 170mm. That max is the only thing stopping Z from driving off
the top of the rods — there's no switch at full extension behind it.

The AS5600 needs a **diametrically** magnetized magnet, not the common axial
type. Verify before gluing — orientation cannot compensate for the wrong
magnetization.

## Mechanical prerequisites

Software timing cannot fix mechanical looseness. Before trusting any programmed
shot, verify each axis in isolation: belt tension on all four axes (including
both jackshaft stages), backlash at direction reversals, and frame rigidity at
full slide extension — extrusion flex shows up as a slow settling wobble after a
move stops, which is a bracing fix, not a firmware one.

Record actual test footage of each axis moving alone at a slow, representative
shot speed and scrub frame-by-frame for stutter or settling before combining
axes. Multi-axis moves compound whatever imperfections already exist per-axis,
and none of this shows up in serial debugging — only on camera.

## Deliberate architecture decisions

Preserved so they don't get accidentally re-litigated:

- **No Marlin/Klipper/printer mainboard.** Those assume open-loop G-code-planned
  motion and don't natively support live encoder jog input or arbitrary I2C
  sensors without significant custom work.
- **UART TMC2209, not standalone.** Migrated deliberately before mechanical
  assembly — retrofitting PDN_UART wiring after the rig is built is far worse
  than doing it on the bench. Eliminates the trimpot/jumper desync bug class
  entirely.
- **Absolute encoders on pan/tilt, switches on slide/Z.** Pan and tilt know
  where they are at power-up; the axes that can't, home.
- **BLE HID instead of a wired shutter.** Uses the same volume-button mechanism
  as cheap Bluetooth remotes, which the Blackmagic Camera app already supports.
