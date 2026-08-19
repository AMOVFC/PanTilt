# pantiltslide schematic tooling

`pantiltslide_full.kicad_sch` is **generated**. Don't hand-edit it and expect
edits to survive — rebuild it instead:

```
cd pantiltslide
python tools/build.py
```

| file | role |
|---|---|
| `build.py` | one-command reproducible rebuild + verification gate |
| `gen_wiring.py` | declares every added part and its net map |
| `verify_wiring.py` | checks the result; self-tests its own coordinate math first |
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

## TMC2209 UART bus — firmware must change

All three drivers share one half-duplex UART bus:

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

## Next step: sync the PCB

`pantiltslide_full.kicad_pcb` has no net assignments at all — every pad,
track, and via carries an empty `(net "")`. The layout was never driven by a
netlist, so its routing is not electrically verified by anything. Open the
project and run **Tools → Update PCB from Schematic (F8)** to push this
schematic into it.
