# Integrated Controller — All Components Soldered (PCBWay assembly)

Fully-integrated version of the [4-axis final rig](../pantiltslide/pantiltslide_full.kicad_sch): every IC soldered directly to the board instead of socketed breakout modules, sized for a PCBWay turnkey assembly order. Companion to `pantiltslide/pantiltslide_full.kicad_sch` (the carrier-board version) — that board still exists and is untouched.

**Status: schematic + PCB placement complete and verified. Not routed, so not yet ready for fab.** See "PCB status" below.

## What's integrated vs. what stays a connector

| Was a socket/module in the carrier board | Now | Why |
|---|---|---|
| ESP32-S3 DevKitC (22-pin sockets) | ESP32-S3-WROOM-1-N16R8 module, soldered | Module has its own certified antenna/crystal — no RF layout risk |
| 4× TMC2209 stepstick sockets | 4× TMC2209-LA (QFN28), full app circuit | Standard Trinamic reference design, verified against datasheet |
| 2× AS5600 breakout connectors | Still connectors — now 4-pin JST XH (`J7`, `J8`) | An AS5600 needs a <3 mm air gap to a magnet on the rotating shaft, so it can't sit on the controller board. Off-the-shelf AS5600 breakout modules stay at the pan/tilt joints, wired back over XH. |
| TCA9548A breakout connector | TCA9548A (TSSOP24), soldered | Standard I2C mux app circuit |
| — (no regulation existed) | 24V→5V buck (LM2596S-5) → 5V→3.3V LDO (AMS1117-3.3) | Carrier board had no onboard regulation at all — this is genuinely new |
| — | USB-C connector, native USB on the S3 | Bench programming/power without the 24V rail connected |
| SSD1306 OLED connector | **Unchanged** — still a connector | It's a glass panel, not something an assembly house places |
| Motor/encoder/limit-switch connectors | Still connectors, now **JST XH** | Polarised and latching — see the Connectors table below |

All part choices (TMC2209-LA, ESP32-S3-WROOM-1, AS5600, TCA9548APWR, LM2596S-5, AMS1117-3.3) are mainstream, high-volume parts — good candidates for PCBWay's turnkey sourcing, not exotic.

## Pin map

Identical to the final rig's — see the main [README pin map](../README.md#pin-map). This schematic was built directly from that table, not from the carrier board's connectors (which had drifted: J33–J36 in the carrier board are leftover keyframe-button connectors that don't correspond to anything in the current firmware's pin map or the "trigger a shot over serial" design described in the main README. They're intentionally not carried over here.)

## Verification status

**ERC: 0 violations. Netlist verified pin-by-pin against the intended design.**

Nets use **global labels**, matching the convention in `pantiltslide_full.kicad_sch`.

### Sheet organisation

Drawn on an A1 sheet in four labelled blocks — power in / regulation, MCU + USB, I2C mux + off-board connectors, and the four TMC2209 drivers in a 2x2 grid.

Power rails are drawn as **GND / +3V3 / +5V / +24V symbols**, not text labels. 158 of the 331 net connections on this sheet are power (90 of them GND alone); as labels they buried the actual signal flow in a wall of repeated text. Only real signals carry label text now — 173 labels instead of 331.

### What was wrong in the first two drafts, and the actual root cause

KiCad **negates Y** when it places a library symbol: a pin at library coordinate `(lx, ly)` lands at screen `(sym_x + lx, sym_y − ly)`. The generator was *adding* `ly`, which mirrors every symbol vertically. Consequences:

- On vertically symmetric parts (resistors, capacitors) the label still landed on a real pin — but **the wrong one**, silently swapping pin 1 and pin 2.
- On asymmetric ICs (TMC2209, TCA9548A, ESP32-S3, USB-C) labels landed in empty space, so those pins read as unconnected.

I had earlier blamed this on `kicad-cli` being buggy. That was wrong — it was this bug, and the tool was reporting it accurately. Ground truth was confirmed from this repo's own `pantiltslide_full.kicad_sch`: J21 placed at `(430,40)` has pin 1 at `y=37.46` (`40 − 2.54`) and pin 4 at `y=45.08` (`40 − (−5.08)`).

Also fixed along the way:
- Net labels had no wire attaching them to pins (label-on-pin is not a connection mechanism in KiCad) — every pin now has a real drawn wire stub to its label.
- Everything snapped to the 1.27 mm connection grid.
- **USB-C `B6`/`B7` were left unconnected.** These are *not* redundant stacked pins — D+ and D− appear on both sides of a Type-C connector so the plug works either way up. Tying only the A-side would have produced a port that works in one orientation and is dead when flipped. `A6+B6` and `A7+B7` are now tied.

### How it was verified

- `kicad-cli sch erc` → **0 violations**.
- Netlist exported and diffed against the intended design: **75/80 nets byte-identical**. The 5 differences were each confirmed benign — `#PWR`/`#FLG` symbols are virtual and by design never exported as components, and the USB-C `A12/B1/B12` (GND) and `A9/B4/B9` (VBUS) pads share exact coordinates with `A1`/`A4`, so KiCad correctly folds them into those nets.
- Spot-checked against the firmware pin map: `GPIO4 → U4.STEP`, `GPIO10 →` all four driver `EN` pins, `TMC_UART` = 1 kΩ series resistor + `IO41` + all four `PDN_UART` pins, `I2C_PAN_SDA` = pull-up + mux channel 0 + AS5600 `SDA`.

The two hand-authored symbols (AS5600, ESP32-S3-WROOM-1) ship as an editable `New_Library.kicad_sym` with a `sym-lib-table`, so they open normally in the Symbol Editor rather than existing only as embedded copies.

Layout still needs a human pass — see "PCB status" below.

## PCB status

One board: `schematic/pantiltslide_integrated.*`

| | |
|---|---|
| Size | 150 × 120 mm |
| Layers | 4 (F.Cu / GND / 3V3 / B.Cu) |
| Footprints | 80 |
| ERC | **0** |
| Schematic parity | **0** |
| DRC (excl. routing) | 7 (4 cosmetic silk, 2 antenna-overhang silk, 1 benign keepout layer-set) |
| **Routing** | **not routed — 253 ratsnest connections** |

**The board is placed and netted, not routed.** Every pad carries its correct net so the ratsnest is complete and correct, but no copper tracks exist yet. Routing is the next step — either by hand in KiCad, or by exporting Specctra DSN (File → Export → Specctra DSN, GUI only — `kicad-cli` has no DSN export) and running Freerouting.

## Connectors

Everything that leaves the board is **JST XH** — polarised so a harness can't be plugged in backwards, latching so it won't shake loose on a moving rig, and rated 3 A/contact (comfortably above the TMC2209's 2 A RMS ceiling). The sole exception is the 24 V input, which keeps a screw terminal for bare supply leads.

| Ref | Function | Connector |
|---|---|---|
| `J1` | 24 V motor supply in | **Screw terminal**, 5.0 mm |
| `J2` | USB-C (power + programming) | USB-C receptacle |
| `J3`–`J6` | Motors: slide / pan / tilt / Z | XH 4-pin |
| `J7`, `J8` | AS5600 modules: pan / tilt | XH 4-pin — `3V3, GND, SDA, SCL` |
| `J9` | SSD1306 OLED | XH 4-pin — `3V3, GND, SDA, SCL` |
| `J10`, `J11` | Encoders: jog / angle | XH 4-pin — `A, B, push, GND` |
| `J12`–`J14` | Limit switches: slide min / max / Z home | XH 2-pin |

Every connector prints its **function on the front silkscreen** (`Motor_Tilt`, `AS5600_Pan`, `Limit_Z_Home` …) directly below its reference designator, so the board can be wired without the schematic to hand. pcbnew's default leaves the Value field hidden and on `F.Fab`, which is a documentation layer and never reaches the physical board — both were changed for all 14 connectors, and the placement packer reserves vertical room for the two text rows.

The AS5600s are **off-the-shelf breakout modules** mounted at the pan and tilt joints, not parts on this board — the sensor has to sit within a few mm of a diametrically-magnetised magnet on the rotating shaft. Two things to check on whichever module you buy:

- **Pin order.** `J7`/`J8` are wired `1=3V3, 2=GND, 3=SDA, 4=SCL`. Most AS5600 breakouts use that order, but confirm before crimping — a swapped 3V3/GND will destroy the module.
- **The `DIR` pin.** On the bare IC, `DIR` sets count direction and must not float. Most breakouts tie it low or provide a jumper; if yours leaves it floating, ground it, or pan/tilt direction will be indeterminate.

Placement is programmatic: components are clustered by subsystem with guaranteed non-overlapping courtyards, but it is not hand-optimised. **Expect to move things before routing**, particularly:

- The 24 V switching node around `U1`/`L1`/`D1` wants tight loop area — keep `C1`/`C2` right at the regulator.
- Each TMC2209's `C_VS` bulk cap should sit hard against its `VS` pin.
- Sense resistors want a short, dedicated return to the driver's GND pad.

### Deliberate PCB decisions

- **ESP32 module overhangs the top edge.** Its footprint carries a 21 mm antenna keepout; letting it hang off the edge keeps the antenna clear instead of sterilising 21 mm of interior copper. The keepout is present on all four copper layers in the board file.
- **4 layers**, with In1 a solid ground plane directly under the signal layers — chosen because four switching stepper drivers, a 24 V buck, and an RF module on one board make return-path integrity the dominant risk.
- **Design rules live in the `.kicad_pro`**, set to PCBWay's standard process (0.15 mm track/clearance, 0.2 mm min hole, 0.45/0.13 mm vias). This matters: KiCad reads DRC constraints from the *project* file, not the `.kicad_pcb`, so limits set only on the board object are ignored at DRC time.
- **Every bypass cap is WIRED to the pin it serves**, not hung on a shared rail symbol. A cap drawn as `+24V -> cap -> GND` is electrically right but tells layout nothing, since all 26 look identical; drawing its rail side onto the chip's own supply pin makes the association part of the schematic and impossible to lose. The GND side still uses a GND symbol (ground is a plane, not a routed net). This is a drawing change only -- the netlist is byte-identical, verified by diffing before/after: **0 pins changed nets**.
- **Every bypass cap also carries a `Decouples` field** naming the pin it serves (`C15` -> `U4 +24V`). A rail-to-GND cap is indistinguishable from every other one in the netlist, so without this a layout pass has no way to know `C15` belongs beside `U4` rather than `U7`. The field is hidden and on `F.Fab` -- it is metadata, not silkscreen. `tools/check_decoupling.py` re-measures every cap's distance to its owner and flags anything over 10mm; run it after any placement change.
- **Zones are defined but unfilled.** `pcbnew`'s zone filler segfaults outside the GUI; KiCad fills them on open, or press `B`.

### Remaining DRC items (all benign)

- `lib_footprint_mismatch` on the ESP32 — the library defines its keepout across all 32 possible copper layers and KiCad correctly trims it to this board's 4. Not a defect.
- `silk_edge_clearance` — the ESP32's silk outline crosses the edge it deliberately overhangs.
- `silk_overlap` / `silk_over_copper` — cosmetic; normal silkscreen cleanup during layout.

## Reproducing / regenerating

This board is generated, not hand-drawn. `tools/run_all.py` rebuilds everything from source:

```bash
python integrated/tools/run_all.py
```

`gen_sch.py` builds the schematic, `export_design.py` dumps the netlist (taken from KiCad's own netlist export, so the PCB can never disagree with the schematic), `build_pcb.py` builds the board via the `pcbnew` Python API, and `write_pro.py` writes the design rules. Editing by hand in KiCad from here on is fine — the generators were scaffolding, not a pipeline you have to keep using.

## Before ordering

- **Route the board.** Nothing is routed yet.
- The TMC2209 sense resistors are `0.11 Ω` placeholders matching the firmware's `tmc::R_SENSE_OHMS`; confirm against the parts actually ordered.
- Fuse rating (`2 A`) is a placeholder — size it to your motor supply.
- The board ships a `sym-lib-table` pointing at a shared `New_Library.kicad_sym` (AS5600 + ESP32-S3-WROOM-1). If you move folders, that relative path needs updating.

## Verified against

- TMC2209: Analog Devices datasheet rev 1.09 (charge pump caps, sense resistors, VREF/SPREAD/CLK strapping, MS1/MS2 address table — matches the existing project's own table)
- ESP32-S3-WROOM-1: Espressif module datasheet (full pin table cross-checked pin-by-pin against the firmware's GPIO map)
- AS5600: ams-OSRAM datasheet (confirmed SOIC-8, not the SOT23-6 I originally assumed — corrected before drawing anything)
- All footprints confirmed present in the local KiCad 9.0 install before use
