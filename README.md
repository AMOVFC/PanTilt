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

## The board

**`pantiltslide/pantiltslide_full_turnkey`** is the production board. It is the
only one being manufactured, and the firmware's factory defaults describe it.

A DevKitC-1 drops into two 22-pin sockets; four TMC2209 SilentStepSticks, a
TCA9548A breakout, the AS5600 heads, OLED, four encoders, four buttons and
three limit switches all plug in over connectors. PCBWay build it turnkey.

The full source/destination pin map is **[docs/pinout.md](docs/pinout.md)** —
that table and `include/config.h` are locked to each other.

### Superseded

These exist in history and in the tree, but are not being built. Nothing in the
firmware targets them.

| Variant | Directory | Why it stopped |
|---|---|---|
| Prototype | [`prototype/`](prototype/README.md) | 3-axis hand-wired origin of the project |
| Integrated | [`integrated/`](integrated/README.md) | everything soldered incl. 4x QFN28; never routed |
| Hybrid | [`hybrid/`](hybrid/README.md) | soldered MCU + power, plug-in drivers; superseded by turnkey |

## Manufacturing

**PCBWay have confirmed they will fabricate the PCB *and* source and assemble
all components, provided the total comes in under USD $150.**

Motors, PSU, AS5600 modules, OLED, encoders and mechanical parts sit outside
that budget -- they wire in over connectors rather than being assembled onto
the board.

## Flashing

Install [PlatformIO](https://platformio.org/) (the VS Code extension, or
`pip install platformio`). Then, from the repo root:

```bash
pio run -t upload
```

and watch it boot:

```bash
pio device monitor
```

The web UI is compiled into the image and the filesystem formats itself on
first boot, so there is no `uploadfs` step and nothing else to copy across.

### Which USB port

**No external programmer is needed.** The USB-to-UART bridge is already on the
DevKitC-1. It has two USB sockets side by side (Micro-USB on v1.0/v1.1):

| Silkscreen | Goes to | Use it? |
|---|---|---|
| `UART` | on-board bridge -> GPIO43/44 | **yes** -- flash and monitor here |
| `USB` | native USB -> GPIO19/20 | no -- Encoder Z lives on those pins |

Plug an ordinary USB cable from your machine into the socket marked **`UART`**.
The bridge drives DTR/RTS into EN and GPIO0, so auto-reset works and you should
not need to touch the buttons.

The board wires Encoder Z to GPIO19/20, which are also the S3's native USB
D-/D+. As soon as the firmware configures those as GPIO the USB pad is
released and native USB stops enumerating — so the native port cannot flash
this build, and you should not plug a cable into it while the encoder is
wired. The `UART` port goes through the on-board bridge to GPIO43/44 and is
unaffected.

If upload fails to find the board, hold **BOOT**, tap **RESET**, release
**BOOT**, and re-run. Specify the port explicitly if you have several devices:

```bash
pio run -t upload --upload-port COM5
```

### First boot

The board comes up as its own access point:

| | |
|---|---|
| SSID | `CamSlider` |
| Password | `sliderpad` |
| Web UI | `http://192.168.4.1` or `http://camslider.local` |

The OLED's bottom line shows the address it is actually reachable at. Point it
at your own network later from **System → Network**; the access point stays as
a fallback, so a mistyped password cannot lock you out of the machine.

### Flashing with the board assembled

`J37` feeds 5 V into the DevKit through `VCC_5V`, so plugging USB in as well
ties two 5 V sources together on the same rail. Power from one at a time.

For a first flash, the least surprising order is:

1. DevKit out of the socket (or the board unpowered) -> plug the `UART` port ->
   `pio run -t upload`
2. Confirm it boots and the access point appears
3. Unplug USB, seat the DevKit, then bring up the board

After that you can flash it in-socket, with the 5 V supply off. Pull **24 V**
either way until you have checked driver currents.

## Firmware

See **[docs/firmware.md](docs/firmware.md)** for the full bring-up order and
the design notes that matter on the bench, and
**[docs/pinout.md](docs/pinout.md)** for the locked pin map.

Everything describing a particular machine — pin map, mechanics, speeds, soft
limits, control bindings, network — is runtime configuration persisted to the
ESP32's filesystem and edited from the built-in web UI. `include/config.h`
holds factory defaults only, so reflashing never discards a working setup.

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
