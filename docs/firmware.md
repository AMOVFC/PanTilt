# Camera slider firmware — flashing and bring-up

Firmware for the ESP32-S3 on `pantiltslide`. Four axes (slide / pan / tilt / Z), TMC2209 drivers over a shared UART, four encoders, four transport
buttons, an OLED, BLE record trigger, and a web UI for configuring all of it.

Everything that describes *your* machine — pin map, mechanics, speeds, limits,
control bindings, network — is runtime config stored on the ESP32's
filesystem and edited from the web UI. `include/config.h` only holds the
factory defaults used on a blank board.

## Flash it

```bash
pio run -t upload
```

then watch the console:

```bash
pio device monitor
```

The filesystem is formatted automatically on first boot; there is no separate
`uploadfs` step, and the web UI is compiled into the firmware image.

To build without the BLE HID stack (saves ~700 kB of flash, removes the
WiFi/BLE coexistence question), set `-DENABLE_BLE_HID=0` in `platformio.ini`.

## First boot

The board comes up as a WiFi access point:

| | |
|---|---|
| SSID | `CamSlider` |
| Password | `sliderpad` |
| Web UI | `http://192.168.4.1` or `http://camslider.local` |

The OLED's bottom line shows the address it is actually reachable at. From the
web UI's **System → Network** tab you can point it at your own WiFi instead;
the access point stays as a fallback so a wrong password can never lock you
out of the machine.

## Bring-up order

Work down this list. Each step depends on the one above it, and each one is
checkable from the web UI without moving a motor.

1. **Drivers answer.** System → *Stepper drivers*. All four should read `ok`.
   A `no reply` is almost always a module that is not fully seated, or MS1/MS2
   straps that disagree with the UART address configured for that axis
   (U1 = 0, U2 = 1, U3 = 2, U4 = 3).
2. **Magnets.** System → *Angle sensors* → **Check magnets**. Pan and tilt
   should read `good`. `too far` / `too close` means the AS5600-to-magnet gap
   needs adjusting; fix it now, because pan and tilt take their absolute
   position from these at every power-up.
3. **Inputs.** Controls → *Live input test*. Turn each knob and press each
   button, and confirm the right row moves. This is also where you notice a
   swapped A/B pair — the count runs the wrong way, which you fix with the
   **Invert** checkbox rather than by re-crimping.
4. **Current, before any motion.** Axes → set *Run current* per axis to
   something conservative (start at 600–800 mA) and check *Sense resistor* in
   System → Hardware matches your modules (0.11 Ω on most SilentStepSticks).
   Current is set over UART, so there is no VREF pot to turn.
5. **First movement.** Control tab, jog speed slider at 10–20 %, and jog each
   axis a short distance. If an axis runs backwards, tick *Invert direction*
   on that axis rather than swapping motor wires.
6. **Slide homing.** Press **Home** on the slide axis and watch it find the
   min switch and back off. Then measure your usable travel and set the slide's
   *Soft limit max* to it.
7. **Rotary ranges.** Jog pan and tilt to their real mechanical stops and set
   the soft limits from what you read. The shipped values (`±170°` pan,
   `-45°/+90°` tilt) are placeholders — pan in particular has no limit switch,
   so these numbers are the only thing stopping the cabling from winding up.

Nothing homes by itself at power-on. That is deliberate: unexpected motion
when a machine is energised is how people get hurt and how camera rigs get
broken. Axes with an AS5600 (pan, tilt) do know where they are immediately,
because the sensor is absolute; the slide starts un-homed and says so.

## The web UI

**Control** — live position, target and speed per axis, press-and-hold jog,
go-to-position, per-axis home and zero, driver enable, BLE record toggle.
`Space` plays the sequence, `Esc` fires the e-stop.

**Sequence** — record a list of four-axis poses and play them back as
coordinated moves. Each keyframe has a travel time (into it) and a hold time
(after arriving). If an axis cannot make a leg in the requested time, the
whole leg is slowed so the axes stay in sync — nothing is left behind. *Ease
in/out* stretches the acceleration ramp across the entire move instead of
snapping to cruise speed, which is the difference between a camera move and a
lurch. Sequences are stored on the board and can be exported/imported as JSON.

**Axes** — everything about an axis: pins, direction, mechanics (belt pitch
and pulley teeth, or gear ratio), microstepping, speeds, soft limits, homing
strategy, feedback sensor, and TMC2209 current settings. The header of each
card shows the resulting steps-per-unit so you can sanity-check the mechanics
you typed in.

**Controls** — remap anything. Each encoder picks an axis and a mode:
*velocity* turns it into a speed dial (returning to the detent it started on
stops the axis), *position* nudges the target one step per click. Each button
gets a short-press and a long-press action from the firmware's own action
list, so the two can never drift apart.

**System** — driver and sensor diagnostics, network setup, the raw pin map,
config export/import, and factory reset.

## Pin map

The locked source/destination table lives in **[pinout.md](pinout.md)** — that
is the single authority, and `include/config.h` is locked to match it. Summary:

| Function | GPIO |
|---|---|
| Slide STEP / DIR (addr 0) | 4 / 5 |
| Pan STEP / DIR (addr 1) | 6 / 7 |
| Tilt STEP / DIR (addr 2) | 8 / 9 |
| Z STEP / DIR (addr 3) | 1 / 2 |
| Driver EN (shared, active low) | 10 |
| TMC2209 UART TX / RX | 47 / 41 |
| Mux I2C SDA / SCL (bus A) | 11 / 12 |
| OLED I2C SDA / SCL (bus B) | 13 / 14 |
| Jog encoder A / B / push | 15 / 16 / 17 |
| Angle encoder A / B / push | 18 / 21 / 38 |
| Limit slide min / max | 39 / 40 |
| Limit Z home | 42 |

GPIO 22–25 do not exist on this part, 26–32 are the SPI flash bus and 33–37
are the octal PSRAM bus; the config API rejects all of them. Free for future
use: GPIO3, 45, 46, 48.

## Things worth knowing

**Two encoders, not four.** The board carries a jog wheel and an angle wheel,
each with a push switch. Encoder and button slots 3 and 4 exist in the config
model but have no connector, so they ship disabled. GPIO19/20 are the native
USB D-/D+ and belong to the USB-C port.

**Microstepping comes from the UART now, not the straps.** On this board MS1
and MS2 select each driver's UART slave address, so they no longer select
microstepping. The firmware sets `mstep_reg_select` and pushes the microstep
count over the wire — which is why changing *Microsteps* in the web UI takes
effect immediately, and why the value in the UI is the value actually in effect.

**Encoders are decoded in interrupts, not by the PCNT peripheral.** On an
ESP32-S3 FastAccelStepper's first four steppers use the MCPWM+PCNT backend and
consume all four pulse-counter units. A PCNT-based encoder library would
silently reconfigure the same units, corrupting both the encoder counts and
the steppers' step counting. See `include/QuadEncoder.h`.

**Open-loop everywhere except pan and tilt.** DIAG is a no-connect on this
board, so there is no StallGuard and no sensorless homing. Lost steps on the
slide and Z axes are simply not detectable; pan and tilt are checked against
their AS5600 while idle and resynced if they drift.

**`hardware/final_wiring_diagram_v3.svg` is stale.** It predates the four-axis
/ UART revision of the board. `pantiltslide/tools/gen_wiring.py` is the
current source of truth for wiring.

## Source layout

| File | What it owns |
|---|---|
| `include/config.h` | Factory defaults only — never read at run time |
| `Settings.{h,cpp}` | The live config, its JSON API and validation, LittleFS persistence |
| `Axis.{h,cpp}` | One axis: stepper, TMC2209, limits, homing, AS5600 drift correction |
| `Motion.{h,cpp}` | The four axes plus the shared enable line, I2C bus and UART bus |
| `Sequencer.{h,cpp}` | Keyframes and coordinated playback |
| `Inputs.{h,cpp}` | Encoders, buttons, and the remappable action dispatch |
| `QuadEncoder.{h,cpp}` | Interrupt-driven quadrature decoding |
| `Mux.{h,cpp}` | TCA9548A channel select + AS5600 reads |
| `Display.{h,cpp}` | OLED status |
| `BleRecorder.{h,cpp}` | BLE HID record toggle |
| `WebUI.{h,cpp}` | WiFi, HTTP/WebSocket API, command marshalling onto the main loop |
| `include/web_index.h` | The web UI itself, embedded in flash |
