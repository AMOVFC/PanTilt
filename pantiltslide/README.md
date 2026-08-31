# Carrier boards — everything plugs in

Three projects live in this folder. They share `New_Library.kicad_sym`,
`TMC2209.pretty` and the same philosophy: nothing on the board needs precision
placement, every active part is a module in a socket.

| project | 5 V / 3.3 V | decoupling | I2C pull-ups | for |
|---|---|---|---|---|
| `pantiltslide_full` | **external** — regulated 5 V into `J37` | none | none | the board as you routed it |
| `pantiltslide_full_pwr` | **onboard** LM2596S-5 buck → AMS1117-3.3 | the regulators' own | 8 | one 24 V brick runs the rig |
| `pantiltslide_full_turnkey` | external, same as the carrier board | 17 caps | 8 | hand to PCBWay, plug modules in |

Both variants are derived copies: same sockets, same connectors, same net
names, same footprint positions, same routed tracks. Regenerate either with
[`tools/gen_variants.py`](tools/gen_variants.py).

---

# `pantiltslide_full_turnkey`

Everything the carrier board leaves to chance, made explicit, with no
regulation added. The intent is a board an assembly house can finish and you
can populate by plugging modules in.

## What was already fine

Nearly everything. Of the 28 parts on the carrier board, 27 are already
sockets or connectors an assembly house can place: both 1×22 DevKit sockets,
all four TMC2209 stepstick sockets, both 1×12 mux sockets, every JST XH/EH
connector, both screw terminals, the fuse clip.

**The one gap was `J23`, the OLED** — a male pin header. An SSD1306 module
comes with male pins, so it could not plug into it; you would have needed a
female-to-female jumper. It is a `PinSocket_1x04` here. Same pads in the same
places, so the copper already routed to it is untouched.

## What stays on a crimped cable, and why

Motors, limit switches, encoders, buttons and the two AS5600 heads keep their
JST XH/EH connectors. Those are not modules that plug into a board — they are
parts bolted to a moving rig, and the JST choice is deliberate: polarised so a
harness cannot go in backwards, latching so it cannot shake loose. Swapping
them for 2.54 mm sockets would let you use bare DuPont jumpers, at the cost of
both those properties. Crimping the harnesses is the one job this board does
not remove. Say the word if you would rather trade that for DuPont.

## Decoupling

17 caps. Nothing on this board is an IC, so these are rail caps — but what
each one is *for* is still specific, and every cap carries a **`Decouples`
field** naming the pin it belongs beside (`C3` → `U2 VM`). The rails here are
drawn as global labels, which tells a layout pass nothing about association,
so that field is the only thing carrying the intent. It is hidden and on
`F.Fab` — metadata, not silkscreen.

| | caps | why |
|---|---|---|
| `+24V` | `C1` 470 µF at `J38`, `C2`–`C5` 100 µF one per driver, `C6`–`C9` 100 nF one per driver | the stepsticks carry their own VM bulk, but what kills drivers is the inductance of the supply leads between the PSU and the board |
| `VCC_5V` | `C10` 100 µF, `C11` 100 nF at `J37` | the 5 V arrives on wires from an external brick |
| `3.3v` | `C12` 22 µF + `C13` 100 nF at `J1`, then 100 nF each at `J20`, `J21`, `J22`, `J23` | the DevKit's LDO feeds every peripheral through header pins |

**Inrush.** ~470 µF of bulk on a screw terminal will spark when you connect a
live 24 V supply. Normal for a stepper board, worth knowing before it startles
you.

## I2C pull-ups

`R1`–`R8`, same scheme as the `pwr` variant — see below. Only the designators
differ (`R1`–`R8` here, `R3`–`R10` there, where `R1`/`R2` are LED ballast).

## Status

|  | carrier | turnkey |
|---|---|---|
| footprints | 28 | 53 |
| tracks | 469 | 469 — none touched |
| ERC errors | 4 | **0** |
| DRC | 1 clearance | the same 1, nothing new |
| schematic parity | 0 | 0 |
| unrouted | 1 (`TILT_M1A`) | 51 — that one plus 25 new parts |

**This board is not fab-ready yet, and cannot be made so automatically.** The
outline is unchanged and the 25 new parts are parked *outside* it, because
unlike the power section they do not belong in one tidy block — a decoupling
cap that is not beside the pin it serves is decorative. Placing them is a human
pass: read each cap's `Decouples` field, put it against that pin, grow the
outline if you need to, re-route. `integrated/tools/check_decoupling.py` is the
pattern for verifying the result (it measures cap-to-owner distance and flags
anything over 10 mm).

## What it adds

LCSC single-unit prices, checked 2026-08-28:

| | each | qty | |
|---|---|---|---|
| 470 µF 50 V | $0.12 | 1 | $0.12 |
| 100 µF 50 V | $0.06 | 4 | $0.24 |
| 100 µF 16 V | $0.03 | 1 | $0.03 |
| 100 nF 0805 | $0.015 | 10 | $0.15 |
| 22 µF 0805 | $0.015 | 1 | $0.02 |
| 0805 resistors | $0.003 | 8 | $0.02 |
| | | **total** | **~$0.58** |

Under a dollar in parts. What it actually costs is 25 placements on the
assembly quote and a layout pass.

---

# `pantiltslide_full_pwr`

The carrier board has no regulation at all. On the bench that means two
supplies: 24 V into `J38` for the motors and a separate regulated 5 V into
`J37` for the DevKit. This variant runs the whole rig from one 24 V brick.

```
J38 24V ─ F1 ─┬─ D3 TVS ─┬─ C1 470µF ─┬─ C2 ─┬─ R1/D4 ─┬─ U5 LM2596S-5
              │                                         │
              └──────────────────── U1..U4 VM ──────────┘
                                                         │
                        L1 100µH ── D2 SS34 ──► VCC_5V ──┴─► J1.21 (DevKit 5V)
                        D1 SS34 catch                        │
                        C3 220µF                             └─► U6 AMS1117-3.3 ─► 3.3v
```

| ref | part | job |
|---|---|---|
| `U5` | LM2596S-5, TO-263-5 | 24 V → 5 V, ~1 A available after the drivers |
| `L1`, `D1`, `C3` | 100 µH / SS34 / 220 µF | the buck's switching loop — keep it tight |
| `D2` | SS34 | ORs the buck into `VCC_5V` |
| `U6` | AMS1117-3.3, SOT-223 | 5 V → 3.3 V for the peripheral rail |
| `C1`, `C2` | 470 µF 50 V, 100 nF | 24 V bulk, right at `U5`'s VIN |
| `D3` | SMBJ33A | clamps 24 V spikes; reversed supply crowbars it and blows `F1` |
| `D4`/`R1`, `D5`/`R2` | LED + 10 k / 1 k | 24 V present, 3.3 V present |
| `JP1` | 0 Ω, **DNP** | fallback link, `ESP_3V3` ↔ `3.3v` |

### D2, and why the buck sits behind a diode

`VCC_5V` has three possible sources: the buck, `J37`, and the DevKit's own USB
port. `D2` means the buck can push the rail up but nothing can push current
back into its output, so plugging USB in while 24 V is live is safe — which is
exactly what you do when reflashing on the rig. The cost is one Schottky drop:
the rail sits at ~4.7 V on buck power, still comfortably above the DevKit
LDO's dropout.

### The one net that changed: `ESP_3V3`

On the carrier board the DevKit's 3V3 output pins (`J1.1`, `J1.2`) feed every
peripheral. Here `U6` drives that rail instead, so those two pins had to come
off it: two regulators must never share a net. They are now `ESP_3V3`, and the
DevKit's LDO only powers the ESP32. `JP1` is the escape hatch — fit the 0 Ω,
leave `U6` unpopulated, and the board is back to the old topology.

**This is the one thing that costs re-routing.** `J1.1`/`J1.2` were the source
of the whole `3.3v` tree, so the two segments that fed it out of those pins are
deleted here — otherwise they would short `ESP_3V3` to `3.3v`. The rest of the
tree is untouched and needs a feed from `U6`. DRC shows it as one dangling
track plus the ratsnest.

## Status

|  | carrier | pwr |
|---|---|---|
| outline | 76 × 113 mm | 123 × 113 mm (47 mm column added on the right) |
| footprints | 28 | 53 |
| tracks | 469 | 467 |
| ERC errors | 4 | **0** |
| DRC | 1 clearance | the same 1, plus 1 dangling track |
| schematic parity | 0 | 0 |
| unrouted | 1 | 53 |

The 25 new footprints are shelf-packed into the new column with 3 mm gaps: no
courtyard overlaps, no shorts, but not hand-optimised. When you rearrange:

- Keep `C1`/`C2` hard against `U5`'s VIN pin, and `U5`/`L1`/`D1` in as small a
  loop as the parts allow — that loop is the board's main EMI source.
- `U5`'s tab (pin 3) is its heatsink. At 24 V → 5 V it burns ~1 W; give it a
  copper pour, not a track. `LM2596T-5.0` in TO-220-5 is a drop-in if you would
  rather bolt a heatsink to it — same symbol, different footprint.
- There are still no copper zones on this board. A ground pour is worth adding
  before the switching regulator goes in.
- The eight pull-ups want to end up near what they pull: `R7`–`R10` beside
  `J20`, not in the power column where the packer left them.

## What it costs

LCSC single-unit prices, checked 2026-08-27:

| ref | part | each |
|---|---|---|
| `L1` | SRN8040-101M 100 µH | $0.73 |
| `U5` | LM2596S-5.0 | $0.53 |
| `C1` | 470 µF 50 V | $0.12 |
| `D3` | SMBJ33A | $0.03 |
| `C3` | 220 µF 25 V | $0.03 |
| `D1`, `D2` | SS34 ×2 | $0.06 |
| `U6` | AMS1117-3.3 | $0.04 |
| rest | 4 caps, 3 resistors, 2 LEDs, 8 pull-ups | ~$0.13 |
| | **total** | **~$1.68** |

`L1` is the one to watch: `SRN8040-101M` had 253 in stock. Any shielded 100 µH
with ≥1.5 A saturation works, and the LM2596 datasheet allows 33–100 µH, so
there is room to substitute.

---

# I2C pull-ups (both variants)

The carrier board has no pull-ups anywhere — every I2C device is a plug-in
module, so the bus works only if the modules happen to carry their own. They
usually do, and "usually" is the problem: a module that doesn't leaves you
soldering resistors onto header pins on a finished board. Fitting them costs
about 3 cents.

Four buses actually move traffic:

| bus | net | to | value |
|---|---|---|---|
| A | `GPIO_11` / `GPIO_12` | TCA9548A breakout `J20`, on-board | 4.7 k |
| B | `GPIO_13` / `GPIO_14` | SSD1306 OLED `J23` | 4.7 k |
| mux ch 0 | `MUX_CH0_SDA/SCL` | AS5600 pan `J21`, at the joint | 2.2 k |
| mux ch 1 | `MUX_CH1_SDA/SCL` | AS5600 tilt `J22`, at the joint | 2.2 k |

Mux channels 2–7 only reach the spare header `J24`, so they get nothing — an
unused channel with pull-ups is just idle current.

**Why 2.2 k on the mux channels.** The AS5600 heads sit at the end of the
longest cables on the rig, and the TCA9548A's pass gates put that cable
capacitance on its own isolated segment. That is the one place where rise time
gets tight, and it is the segment whose pull-up you cannot move closer to the
load.

These are deliberately belt-and-braces: in parallel with a module's own 10 k
they land near 3.2 k / 1.8 k, still well inside the ~3 mA an I2C device has to
sink. If a module turns out to carry a strong pull-up of its own, lift the
board resistor rather than the other way round.

---

# ERC

Both variants: **0 errors**. The carrier board's 4 `power_pin_not_driven`
errors are gone — `PWR_FLAG`s (and, on the `pwr` board, the real regulator
outputs) drive the rails now.

Both gain the same 8 warnings: `Pins of type Bidirectional and Power output
are connected`. Those are the TMC2209 `MS1`/`MS2` address straps, typed
bidirectional in `New_Library:tmc2209` and tied to `GND`/`3.3v` to set the
UART addresses. They only appear because the rails finally have a real driver.
Fixing it means retyping those two pins as inputs in the shared symbol
library, which would also touch the carrier board — left alone deliberately.

The remaining warnings (145 off-grid endpoints, 20 isolated labels, 3
duplicate net names) are inherited from `pantiltslide_full` unchanged.

# Regenerating

```bash
python pantiltslide/tools/gen_variants.py
```

Reads `pantiltslide_full.*`, writes both variants. UUIDs are derived, so
re-running produces the same files and an empty diff. Note that
`pantiltslide_full.kicad_sch` is itself generated (see `tools/README.md`) — if
you rebuild it, re-run this too.

The script asserts on everything it depends on: the `3.3v` label at `J1`, the
`J23` header footprint, and the two tracks it deletes. If the base board moves
under it, it stops rather than producing a quietly wrong board.

# Before ordering

- **Turnkey:** place the 25 new parts against the pins their `Decouples` field
  names, grow the outline, re-route.
- **Pwr:** route the power section, and add the `3.3v` feed from `U6`.
- `F1` is a 2 A placeholder — size it to the motor supply.
- On the `pwr` board, `D3` is a **unidirectional** SMBJ33A. A bidirectional
  `SMBJ33CA` will clamp spikes but will not crowbar a reversed supply.
- Neither board has copper zones yet.
