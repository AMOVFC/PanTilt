# Integrated Controller — All Components Soldered (PCBWay assembly)

Draft schematic for a fully-integrated version of the [4-axis final rig](../pantiltslide/pantiltslide_full.kicad_sch): every IC soldered directly to the board instead of socketed breakout modules, sized for a PCBWay turnkey assembly order. Companion to `pantiltslide/pantiltslide_full.kicad_sch` (the carrier-board version) — that board still exists and is untouched.

**Status: schematic only, for review. Not yet ready for fab.** See "Known issue" below before opening in KiCad.

## What's integrated vs. what stays a connector

| Was a socket/module in the carrier board | Now | Why |
|---|---|---|
| ESP32-S3 DevKitC (22-pin sockets) | ESP32-S3-WROOM-1-N16R8 module, soldered | Module has its own certified antenna/crystal — no RF layout risk |
| 4× TMC2209 stepstick sockets | 4× TMC2209-LA (QFN28), full app circuit | Standard Trinamic reference design, verified against datasheet |
| 2× AS5600 breakout connectors | 2× AS5600 (SOIC-8), soldered | Simple support circuit, datasheet-verified pinout |
| TCA9548A breakout connector | TCA9548A (TSSOP24), soldered | Standard I2C mux app circuit |
| — (no regulation existed) | 24V→5V buck (LM2596S-5) → 5V→3.3V LDO (AMS1117-3.3) | Carrier board had no onboard regulation at all — this is genuinely new |
| — | USB-C connector, native USB on the S3 | Bench programming/power without the 24V rail connected |
| SSD1306 OLED connector | **Unchanged** — still a connector | It's a glass panel, not something an assembly house places |
| Motor/encoder/limit-switch connectors | **Unchanged**, same footprints | Preserves the wiring harness the mechanical build already uses |

All part choices (TMC2209-LA, ESP32-S3-WROOM-1, AS5600, TCA9548APWR, LM2596S-5, AMS1117-3.3) are mainstream, high-volume parts — good candidates for PCBWay's turnkey sourcing, not exotic.

## Pin map

Identical to the final rig's — see the main [README pin map](../README.md#pin-map). This schematic was built directly from that table, not from the carrier board's connectors (which had drifted: J33–J36 in the carrier board are leftover keyframe-button connectors that don't correspond to anything in the current firmware's pin map or the "trigger a shot over serial" design described in the main README. They're intentionally not carried over here.)

## Known issue — please verify in KiCad before trusting this

This file was generated programmatically. The first version I handed over placed net labels directly on pin coordinates with no wire connecting them — that's not a supported KiCad connection mechanism, which is why it didn't read as wired when opened. Fixed: every pin now has a real drawn wire stub out to its label, and every component is snapped to KiCad's 1.27mm connection grid (eliminated ~400 false "off-grid" warnings on its own).

After that fix, `kicad-cli sch erc` is clean for the power section and all simple connectors — independently re-verified against the file's raw coordinates. For the larger ICs (TMC2209 ×4, TCA9548A, ESP32-S3 module, USB-C), the CLI still reports some pins as "not connected" that are — by every check I can run against the file's own data (coordinates, wire endpoints, net membership) — correctly wired. I was not able to fully root-cause this within the tool; I also independently caught this same `kicad-cli` build silently swapping which pin got which net name in `sch export netlist`, and segfaulting on malformed input during testing, so I no longer trust its ERC output as the last word on this file.

**Please open this in KiCad and run Inspect → Electrical Rules Checker yourself** — that's the authoritative check, not the CLI. If the GUI still flags specific pins on the big ICs as unconnected, tell me which ones and I'll fix them directly rather than guessing further from the command line.

## Verified against

- TMC2209: Analog Devices datasheet rev 1.09 (charge pump caps, sense resistors, VREF/SPREAD/CLK strapping, MS1/MS2 address table — matches the existing project's own table)
- ESP32-S3-WROOM-1: Espressif module datasheet (full pin table cross-checked pin-by-pin against the firmware's GPIO map)
- AS5600: ams-OSRAM datasheet (confirmed SOIC-8, not the SOT23-6 I originally assumed — corrected before drawing anything)
- All footprints confirmed present in the local KiCad 9.0 install before use
