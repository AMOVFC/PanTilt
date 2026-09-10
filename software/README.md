# Camera slider firmware — ESP32-S3

Firmware for the [`../hardware/pantilt-controller`](../hardware/) board. Four
axes (slide / pan / tilt / Z), TMC2209 drivers on a shared UART, four
quadrature encoders, four transport buttons, an OLED, a BLE record trigger for
the Blackmagic Camera app, and a web UI that configures all of it.

Everything that describes *your* machine — pin map, mechanics, speeds, soft
limits, control bindings, network — is **runtime config** stored on the ESP32's
LittleFS and edited from the web UI. [`include/config.h`](include/config.h)
holds factory defaults only, so reflashing never discards a working setup.

## Quick start

PlatformIO project — build and flash from **this folder**:

```bash
cd software
pio run -t upload
pio device monitor
```

The web UI is compiled into the image and LittleFS formats itself on first
boot; there is no separate `uploadfs` step.

Plug USB into the DevKitC-1 socket marked **`UART`** (not `USB` — Encoder Z
lives on the native-USB pins GPIO19/20). No external programmer needed. If
upload can't find the board: hold **BOOT**, tap **RESET**, release **BOOT**,
retry.

### First boot

The board comes up as its own access point:

| | |
|---|---|
| SSID | `CamSlider` |
| Password | `sliderpad` |
| Web UI | `http://192.168.4.1` or `http://camslider.local` |

Point it at your own network from **System → Network**; the AP stays as a
fallback so a wrong password can't lock you out. The OLED's bottom line always
shows the address it's actually reachable at.

> **Read [`docs/firmware.md`](docs/firmware.md) before energising motors.**
> Nothing homes at power-on, unwired limit switches read as *not triggered*,
> and the shipped soft limits are placeholders. That doc has the full bench
> bring-up order.

## Architecture

| File | What it owns |
|---|---|
| `include/config.h` | Factory defaults only — never read at run time |
| `src/Settings.*` | The live config: JSON API, validation, LittleFS persistence |
| `src/Axis.*` | One axis — stepper, TMC2209, limits, homing, AS5600 drift correction |
| `src/Motion.*` | The four axes + the shared enable line, I²C bus and UART bus |
| `src/Sequencer.*` | Keyframes and time-synced coordinated playback |
| `src/CurveSequence.*` | Per-axis Bezier channels on one shared clock |
| `src/Inputs.*` | Encoders, buttons, and the remappable action dispatch |
| `src/QuadEncoder.*` | Interrupt-driven quadrature decode (not PCNT — see the header) |
| `src/Mux.*` | TCA9548A channel select + AS5600 reads |
| `src/Display.*` | OLED status |
| `src/BleRecorder.*` | BLE HID record toggle |
| `src/WebUI.*` | WiFi, HTTP/WebSocket API, command marshalling onto the main loop |
| `include/web_index.h` | The web UI itself, embedded in flash |

## The web UI

- **Control** — live position/target/speed per axis, press-and-hold jog,
  go-to, per-axis home and zero, driver enable, BLE record toggle.
  `Space` plays the sequence, `Esc` fires the e-stop.
- **Sequence** — record four-axis poses, play them back as coordinated moves
  with per-leg travel and hold times; slow legs drag the whole leg so axes
  stay in sync. Ease in/out. Stored on the board, export/import as JSON.
- **Axes** — pins, direction, mechanics (belt/pulley or gear ratio),
  microstepping, speeds, soft limits, homing strategy, feedback sensor,
  TMC2209 current. Card header shows the resulting steps-per-unit.
- **Controls** — remap every encoder (velocity dial or position nudge) and
  every button (short / long press → firmware action list).
- **System** — driver + sensor diagnostics, network, raw pin map,
  config export/import, factory reset.

## Pin map

[`docs/pinout.md`](docs/pinout.md) is the single authority and `config.h` is
locked to it. Short form:

| Function | GPIO |
|---|---|
| Stepper A/B/C/Z STEP·DIR | 4·5 / 6·7 / 8·9 / 1·2 |
| Driver EN (shared, active-low) | 10 |
| TMC2209 UART TX / RX | 47 / 41 |
| Mux I²C SDA/SCL · OLED I²C SDA/SCL | 11·12 / 13·14 |
| Encoders A/B/C/Z (A,B pairs) | 15,16 / 17,18 / 21,38 / 19,20 |
| Limits: slide min / max, Z max | 39 / 40 / 42 |
| Buttons: set-kf / clear-kf / play / reset | 48 / 3 / 45 / 46 |

## Build

`platformio.ini`, env `esp32-s3-n16r8` (16 MB flash, 8 MB octal PSRAM):

| Flag | Effect |
|---|---|
| `-DENABLE_BLE_HID=1` | BLE HID record trigger. Set `0` to drop the BLE stack (~700 kB flash). |
| `-DWS_MAX_QUEUED_MESSAGES=8` | Short WebSocket queue — drop stale telemetry rather than buffer it. |
| `-DBOARD_HAS_PSRAM` / `-DCORE_DEBUG_LEVEL=1` | standard |

Libraries (pulled by PlatformIO): FastAccelStepper, TMCStepper, Adafruit
SSD1306 + GFX, ArduinoJson, AsyncTCP + ESPAsyncWebServer, ESP32 BLE Keyboard.

## Notes that matter on the bench

- **Encoder Z is on GPIO19/20 = native USB.** Works because the DevKit
  enumerates over its UART bridge, but native USB is unavailable on this build.
- **Microstepping comes from UART, not MS1/MS2 straps** — on this board those
  pins are the driver's UART address. If UART fails, each axis lands on a
  different microstep resolution and positions go silently wrong per axis.
  Do not proceed past a `no reply` in System → Stepper drivers.
- **Encoders decode in ISRs, not PCNT** — FastAccelStepper's MCPWM+PCNT
  backend already owns all four pulse-counter units. See `include/QuadEncoder.h`.
- **Open-loop except pan/tilt.** DIAG is a no-connect, so no StallGuard / no
  sensorless homing; lost steps on slide and Z aren't detectable. Pan and tilt
  are checked against their AS5600 while idle and resynced on drift.
- **Button "set keyframe" shares GPIO48 with the DevKit RGB LED** — fine as a
  switch input, but don't enable an LED library on this board.

Full detail and the step-by-step bring-up: [`docs/firmware.md`](docs/firmware.md).
