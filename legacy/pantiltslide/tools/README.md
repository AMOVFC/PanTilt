# pantiltslide schematic tooling

`pantiltslide_full.kicad_sch` is **generated**. Don't hand-edit it and expect
edits to survive — rebuild it instead:

```
cd pantiltslide
python tools/build.py
```

| file | role |
|---|---|
| `build.py` | one-command reproducible rebuild + verification gates |
| `gen_wiring.py` | declares every added part and its net map |
| `verify_wiring.py` | checks nets; self-tests its own coordinate math first |
| `verify_footprints.py` | checks every symbol has a footprint that actually resolves |
| `base.kicad_sch` | **build input** — the hand-drawn ESP32 section, never modified |
| `_body.generated.txt` | intermediate output, safe to delete |

`base.kicad_sch` lives in here rather than beside the project on purpose: it is
an input to the build, not a schematic to open. The project proper is
`pantiltslide_full.{kicad_pro,kicad_sch,kicad_pcb,kicad_prl}` one level up.
The TMC2209 symbol comes from `../New_Library.kicad_sym`.

To change wiring, edit the net maps near the bottom of `gen_wiring.py`, update
the `EXPECTED` table in `verify_wiring.py` to match, and re-run `build.py`.
The two are deliberately written out separately so a typo in one doesn't
silently agree with the other.

## The coordinate convention (read this before touching geometry)

**`.kicad_sym` is Y-up. `.kicad_sch` is Y-down.** Converting a library pin's
local position to its position on the sheet:

```
sheet_x = origin_x + local_x
sheet_y = origin_y - local_y     # <-- NOTE THE MINUS
```

plus the symbol instance's own `(mirror ...)` and rotation, which
`verify_wiring.py:sheet_pos()` handles in the general case.

Getting that sign wrong does **not** produce an obvious failure. It mirrors
each multi-pin part vertically, so the schematic still renders as a tidy
component with tidy labels — but the labels are attached to the wrong pins.
On this board that silently swapped stepper coil phases and left pins
dangling. It was found by eye in KiCad, not by any automated check.

## Label orientation (the other easy thing to get wrong)

A global label is a pentagon whose point touches the wire and whose body
extends **away** from the pin. Two fields control that, and both must match
the side the label sits on:

| label sits | pin angle | label angle | justify |
|---|---|---|---|
| left of pin | 0 | 180 | right |
| right of pin | 180 | 0 | **left** |

Wrong *angle* mirrors the label. Wrong *justify* anchors the text on the
wrong edge so it grows back across the pin numbers. An earlier revision had
both wrong on every right-hand label and hid it behind a 25mm stub; the stub
is now 7.62mm because correct orientation needs no compensation.

Note this was originally "confirmed" with a probe that tested
`'justify' in str(effects)` — true for both `left` and `right`, so it proved
nothing. Read the actual value.

## Why `verify_wiring.py` self-tests

The first version of that check recomputed pin positions with the *same*
wrong formula the generator used, so it confirmed the generator agreed with
itself and reported "ALL CLEAR" on a broken file. That is a consistency
check, not a correctness check, and it is worse than no check at all because
it is reassuring.

It now first resolves several known pin→label pairs in `base.kicad_sch` — a
file KiCad wrote, not this tooling — and aborts if they don't come out right.
That ground-truth set deliberately
includes `J2`, which carries `(mirror y)`, so the mirror path is exercised
too. Verify the guard still bites by flipping the sign in `sheet_pos()`; the
self-test must fail.

## Still worth doing by hand

Rendering is the check that actually caught the original bug:

```
kicad-cli sch export pdf --output review.pdf pantiltslide_full.kicad_sch
```

Open it and confirm pin numbers line up with the labels next to them. No
static check substitutes for looking at it.

`TMC2209.pretty/TMC2209_SilentStepStick_Socket_18P.kicad_mod` is **not**
generated — `build.py` never touches the footprint, so hand edits to it
survive rebuilds. Its 16-pin header geometry is verified against Watterott's
reference design; the DIAG/INDEX pads (17/18) were adjusted by hand.

## Pin map (4 axes)

| GPIO | use | | GPIO | use |
|---|---|---|---|---|
| 4 / 5 | U1 slide STEP/DIR | | 15 / 16 | J29 slide encoder A/B |
| 6 / 7 | U2 pan STEP/DIR | | 17 / 18 | J30 pan encoder A/B |
| 8 / 9 | U3 tilt STEP/DIR | | 21 / 38 | J31 tilt encoder A/B |
| 47 / 48 | U4 aux STEP/DIR | | 19 / 20 | J32 aux encoder A/B |
| 10 | driver EN (all 4) | | 1 | J33 set keyframe |
| 41 / 42 | driver UART TX/RX | | 3 | J34 clear keyframe |
| 11 / 12 | I2C-A mux SDA/SCL | | 45 | J35 play/pause |
| 13 / 14 | I2C-B OLED SDA/SCL | | 46 | J36 reset |
| 39 / 40 | slide limit min/max | | 0 | spare |

Unusable on this module: **GPIO 33–37** are consumed by the N16R8's octal
PSRAM. GPIO 19/20 are the native-USB pins — using them for the aux encoder
gives up the USB-OTG port, but programming still works over the DevKitC's
separate UART bridge. GPIO 0 is left free because holding it at reset enters
download mode, which is a poor trait for a panel button.

The four buttons are all active-low to GND on internal pull-ups, so the kit
needs no resistors. GPIO 45/46 are strapping pins that must read LOW at
reset; a button-to-GND is safe on both because their default state is an
internal pull-down and pressing one during power-up only reinforces that.

The earlier general-purpose jog and setpoint encoders (`J24`, `J25`) are
gone — the four per-axis encoders supersede them, and their pins were needed.

## TMC2209 UART bus — firmware must change

All four drivers share one half-duplex UART bus:

| net | ESP32 | driver pin | note |
|---|---|---|---|
| `GPIO_41` | TX | 12 `UART` | through the module's onboard 1k |
| `GPIO_42` | RX | 11 `PDN` | direct to PDN_UART |

Slave addresses are strapped on the board, so a kit builder cannot create a
bus conflict:

| driver | axis | MS1 (pin 15) | MS2 (pin 14) | address |
|---|---|---|---|---|
| U1 | slide | GND | GND | 0 |
| U2 | pan | +3.3V | GND | 1 |
| U3 | tilt | GND | +3.3V | 2 |
| U4 | aux | +3.3V | +3.3V | 3 |

**This invalidates the pin-strapping comment in `include/config.h`.** MS1/MS2
select microstepping only in standalone mode; once UART is in use they are
address pins, and microstepping comes from the driver's `MRES` register.
A TMC2209 does not power up at 1/16 — firmware must set it explicitly.
Until it does, `SLIDE_MICROSTEPPING` / `ROTARY_MICROSTEPPING = 16` will not
match the hardware and **all position math will be wrong**, which is the
exact failure the comment in `config.h` warns about.

Pins left as no-connects: 13 `SPRD` (stealthChop via the module's pulldown),
17 `INDEX`, 18 `DIAG`. Bringing `DIAG` to a GPIO later would enable
StallGuard homing — worth considering for the pan/tilt axes, which have no
limit switches.

## Footprints, and what `build.py` patches into the base

`base.kicad_sch` was drawn without footprints, and two of its parts carry
reference designators KiCad rejects. `build.py` fixes both while copying, so
the base file itself stays untouched:

- **Footprints assigned** to all 17 base parts (`BASE_FOOTPRINTS`). Without
  these, F8 refuses them with `Cannot add <ref> (no footprint assigned)` and
  their nets never reach the board.
- **`5Vin` → `J37`, `24Vin` → `J38`** (`BASE_RENAMES`), with the readable name
  moved into Value. A refdes must start with a letter; these didn't, so KiCad
  treated them as unannotated — reporting them as `5Vin1`/`24Vin1` and raising
  an annotation error.
- **J1/J2 get a `PinSocket_1x22` each** rather than one combined DevKitC
  footprint, because two symbols cannot share a single footprint. Sockets also
  suit a plug-together kit. The old combined `ESP32-S3-DevKitC` footprint left
  in the PCB is an orphan — delete it.
- **F1** gets a 5x20mm clip holder; that is a mechanical preference, so swap it
  for a blade holder or PTC if the enclosure wants something else.

`verify_footprints.py` resolves every `LIBRARY:NAME` against the real
fp-lib-table. It exists because an invented library name
(`TerminalBlock_MaiXu:` — the vendor is part of the footprint *name*, the
library is just `TerminalBlock`) is invisible until F8 rejects eight parts.
Confirm the gate still bites by reintroducing that typo; it must fail.

## Next step: sync the PCB

Run **Tools → Update PCB from Schematic (F8)**. A partial sync has already
landed (17 footprints, 62 nets); the parts that previously errored out should
come across now that they have valid footprints and refdes.

Two things to expect:

- A leftover **`REF**`** footprint — the old combined `ESP32-S3-DevKitC`
  module, which no schematic symbol maps to any more. Delete it, or let F8
  remove it via *Delete footprints with no symbol*.
- The **pre-existing routing is not trustworthy**. It was drawn before any
  netlist existed, so every track was laid with an empty `(net "")`. Tracks
  that happen to sit under a pad may now silently adopt that pad's net.
  Re-run DRC with *Check footprint courtyards* and *Check net conflicts*
  after the sync, and treat any track you did not deliberately re-draw as
  unverified.
