# Hybrid Controller — integrated MCU + power, plug-in drivers and mux

Third board variant, sitting between [`integrated/`](../integrated/README.md)
(everything soldered) and [`pantiltslide/`](../pantiltslide/) (everything
socketed).

| Soldered down | Plugs in |
|---|---|
| ESP32-S3-WROOM-1-N16R8 | 4× TMC2209 SilentStepStick |
| 24 V→5 V buck (LM2596S-5) | TCA9548A breakout (8-pin header, `J200`) |
| 5 V→3.3 V LDO (AMS1117-3.3) | AS5600 heads, OLED, encoders, limit switches, motors |
| USB-C, fuse, bulk caps | |

**Status: schematic draft, ERC-checked. No PCB yet.**

## Why this split

The `$150` PCBWay turnkey budget is dominated by fabrication and assembly, not
parts — the whole BOM is only ~$25 at LCSC prices. What costs money and carries
risk is fine-pitch placement.

| | Placements | QFN/TSSOP | Parts @ LCSC |
|---|---|---|---|
| Integrated | 87 | 5 | ~$25 |
| **Hybrid** | **66** | **0** | **~$13** |
| Carrier | ~44 | 0 | ~$8 |

Nothing on this board needs precision placement. It keeps what is genuinely
worth soldering — onboard regulation, USB-C, and a routed `PDN_UART` bus that
removes the "solder a wire to each stepstick's PDN pad" bench job — while a
blown driver stays a $5 plug-in swap instead of QFN rework on a finished board.

## Sheets

| Sheet | Source | Notes |
|---|---|---|
| root | `pantiltslide_hybrid.kicad_sch` | generated |
| `power`, `usb`, `esp32` | copied from `integrated/` | byte-identical, unmodified |
| `stepper drivers` | `tmc2209_driver_stepstick.kicad_sch` | copied from `integrated/` |
| `I2C-IO` | `i2c_io_plugin.kicad_sch` | generated — the only genuinely new sheet |

The reused sheets are **copies**, not references, so this variant is
self-contained and nothing done here can reach back into `integrated/`. The
tradeoff: a later fix to `integrated/esp32.kicad_sch` does not propagate — re-copy
it deliberately.

## Regenerating

```bash
python gen_hybrid.py
```

UUIDs are derived deterministically, so regenerating produces a byte-identical
file and an empty diff. Hand-editing the generated sheets in Eeschema will be
overwritten — change `gen_hybrid.py` instead.

Artifacts (`erc.json`, `net.net`, `hybrid_draft.pdf`, `preview/`) are gitignored:

```bash
kicad-cli sch erc --format json --severity-error --severity-warning \
  --output erc.json pantiltslide_hybrid.kicad_sch
kicad-cli sch export pdf --output hybrid_draft.pdf pantiltslide_hybrid.kicad_sch
```

## A defect this variant fixes

The driver sheet exposes 14 hierarchical labels (`STEP0-3`, `DIR0-3`,
`DIAG0-3`, `EN`, `UART`), but the integrated root's sheet symbol carries **no
matching pins** — so on that board the drivers are not electrically connected to
the MCU. This root declares all 14 pins and wires them to the ESP32's global
labels.

| ERC | Integrated | Hybrid |
|---|---|---|
| `hier_label_mismatch` (drivers) | 14 | 0 |
| `pin_not_driven` (drivers) | 9 | 0 |
| `isolated_pin_label` | 21 | 0 |
| inherited (power PWR_FLAG, USB lib symbol) | 4 | 4 |
| **total** | **48** | **4** |

The remaining 4 come from the donor sheets, not from this variant. Netlist
verified end-to-end: `GPIO4/5→A100`, `GPIO6/7→A110`, `GPIO8/9→A120`,
`GPIO1/2→A130`, `GPIO10` (EN) reaching all four sticks plus the MCU, and
`TMC_UART` tying the four `PDN_DIAG` headers through `R10`.

`DIAG0-3` are deliberately no-connected at board level — they are broken out to
the `PDN_DIAG` headers inside the driver sheet for optional stall detection.

## Open questions before this goes to PCB

1. **Pull-ups.** Fitted 4× 4.7 k: two on the mux input (`GPIO11/12`), two on
   bus B (`GPIO13/14`). The AS5600 and OLED breakouts carry their own on the
   module side. Channel-side pull-ups are not fitted.
2. **The 4th axis is wired as Z on `GPIO1/2`**, per `include/config.h`. That
   matches the firmware; it does *not* match the old carrier board's `GPIO47/48`.
3. **Encoders are the shared jog + angle pair** (`GPIO15/16/17`,
   `GPIO18/21/38`), carried over from the integrated sheet — not one encoder per
   axis. `prototype/README.md` calls this scheme a placeholder due for rework.

`J200`'s pinout deliberately matches the carrier board's `J20`, so the same
breakout and the same wiring harness work on either board.
