#!/usr/bin/env python3
"""Derive the carrier-board variants from pantiltslide_full.*

`pantiltslide_full` is the carrier board: everything is a module in a socket,
there is no regulation, and nothing on the board is decoupled or pulled up -
it inherits all of that from whatever modules get plugged into it.

    pwr        onboard 24 V -> 5 V buck and 5 V -> 3.3 V LDO, so the rig runs
               from a single 24 V brick.  I2C pull-ups.
    turnkey    no regulation (external 5 V on J37, as on the carrier board),
               but decoupled, pulled up, and socketed everywhere - the version
               to hand an assembly house.

Everything not listed above - every socket, connector, net name, footprint
position and routed track - is carried over unchanged.  UUIDs are derived
rather than random, so re-running reproduces the same files byte for byte.

Usage:
    python pantiltslide/tools/gen_variants.py                    # both
    python pantiltslide/tools/gen_variants.py --variant turnkey
    python pantiltslide/tools/gen_variants.py --sch-only

The PCB step needs KiCad's bundled Python (it imports pcbnew); the script
re-execs it automatically if the running interpreter has no pcbnew.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJ = HERE.parent                      # pantiltslide/
REPO = PROJ.parent

SRC_NAME = "pantiltslide_full"

SRC_SCH = PROJ / f"{SRC_NAME}.kicad_sch"
SRC_PCB = PROJ / f"{SRC_NAME}.kicad_pcb"
SRC_PRO = PROJ / f"{SRC_NAME}.kicad_pro"

# set by run(); the emitters below read it when they write instance data
DST_NAME = None
DST_SCH = DST_PCB = DST_PRO = None


def set_variant(name: str) -> None:
    global DST_NAME, DST_SCH, DST_PCB, DST_PRO
    DST_NAME = name
    DST_SCH = PROJ / f"{name}.kicad_sch"
    DST_PCB = PROJ / f"{name}.kicad_pcb"
    DST_PRO = PROJ / f"{name}.kicad_pro"

DONOR_SCH = REPO / "prototype" / "schematic" / "pantilt_prototype.kicad_sch"
HYBRID_PWR = REPO / "hybrid" / "schematic" / "power.kicad_sch"

KICAD_SHARE = Path(r"C:/Program Files/KiCad/10.0/share/kicad")
KICAD_PY = Path(r"C:/Program Files/KiCad/10.0/bin/python.exe")

NS = uuid.UUID("6f1f5f0e-3d9a-5c47-9a3f-2c1d0e7b4a10")   # stable namespace

# The ESP32 DevKit's 3V3 output pin used to feed every peripheral.  It now sits
# on its own net so the onboard LDO can drive the peripheral rail instead.
J1_3V3_LABEL_AT = (121.92, 78.74)


def uid(key: str) -> str:
    return str(uuid.uuid5(NS, key))


def n(v: float) -> float:
    """Kill float dust so derived coordinates land on the same text as literals."""
    return round(v, 2)


# --------------------------------------------------------------------------
# s-expression helpers
# --------------------------------------------------------------------------

def sexp_at(text: str, start: int) -> str:
    """Return the balanced s-expression starting at `start`.

    Parens inside quoted strings do not count.  Footprint `descr` fields are
    full of them ("Fuse holder (5x20mm)"), and treating one as structure walks
    the block boundary off the end of the real footprint.
    """
    depth = 0
    in_str = False
    esc = False
    for i in range(start, len(text)):
        c = text[i]
        if esc:
            esc = False
            continue
        if in_str:
            if c == chr(92):
                esc = True
            elif c == '"':
                in_str = False
            continue
        if c == '"':
            in_str = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise ValueError("unbalanced s-expression")


def lib_symbols_block(text: str) -> tuple[int, int, str]:
    i = text.find("(lib_symbols")
    blk = sexp_at(text, i)
    return i, i + len(blk), blk


def embedded_symbol(text: str, name: str) -> str:
    """Pull a `(symbol "Lib:Name" ...)` out of a schematic's lib_symbols."""
    _, _, blk = lib_symbols_block(text)
    m = re.search(r'\n\t\t\(symbol "%s"\n' % re.escape(name), blk)
    if not m:
        raise KeyError(f"{name} not found")
    return sexp_at(blk, m.start() + 1)


def library_symbol(lib: str, name: str) -> str:
    """Pull a symbol out of an installed .kicad_sym and put it in schematic form.

    Library entries are indented one tab and named bare ("LED"); a schematic's
    lib_symbols wants two tabs and a "Device:LED" style name.
    """
    src = (KICAD_SHARE / "symbols" / f"{lib}.kicad_sym").read_text(encoding="utf-8")
    m = re.search(r'\n\t\(symbol "%s"\n' % re.escape(name), src)
    if not m:
        raise KeyError(f"{lib}:{name} not found")
    blk = sexp_at(src, m.start() + 1)
    # Only the outer name is namespaced; the "R_0_1" body sub-symbols keep the
    # bare name they have in the library.
    blk = blk.replace('(symbol "%s"' % name, '(symbol "%s:%s"' % (lib, name), 1)
    return "\n".join("\t" + ln if ln else ln for ln in blk.split("\n"))


# --------------------------------------------------------------------------
# schematic item emitters
# --------------------------------------------------------------------------

FONT = "(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n\t\t\t\t)\n%s\t\t\t)"


def _prop(name: str, value: str, x: float, y: float, rot: int = 0,
          hide: bool = True, justify: str | None = None) -> str:
    just = f"\t\t\t\t(justify {justify})\n" if justify else ""
    return (
        f'\t\t(property "{name}" "{value}"\n'
        f"\t\t\t(at {x} {y} {rot})\n"
        + ("\t\t\t(hide yes)\n" if hide else "")
        + "\t\t\t(show_name no)\n"
          "\t\t\t(do_not_autoplace no)\n"
        f"\t\t\t{FONT % just}\n"
        "\t\t)\n"
    )


def symbol(lib_id: str, ref: str, value: str, x: float, y: float, *,
           rot: int = 0, footprint: str = "", pins: tuple = (),
           ref_off: tuple = (2.54, -1.27), val_off: tuple = (2.54, 1.27),
           justify: str = "left", dnp: bool = False,
           fields: dict | None = None, hide_value: bool = False,
           hide_ref: bool = False) -> str:
    x, y = n(x), n(y)
    u = uid(f"sym:{ref}")
    out = [
        "\t(symbol\n"
        f'\t\t(lib_id "{lib_id}")\n'
        f"\t\t(at {x} {y} {rot})\n"
        "\t\t(unit 1)\n"
        "\t\t(body_style 1)\n"
        "\t\t(exclude_from_sim no)\n"
        "\t\t(in_bom yes)\n"
        "\t\t(on_board yes)\n"
        "\t\t(in_pos_files yes)\n"
        f"\t\t(dnp {'yes' if dnp else 'no'})\n"
        f'\t\t(uuid "{u}")\n'
    ]
    out.append(_prop("Reference", ref, n(x + ref_off[0]), n(y + ref_off[1]), 0,
                     hide=hide_ref, justify=justify))
    out.append(_prop("Value", value, n(x + val_off[0]), n(y + val_off[1]), 0,
                     hide=hide_value, justify=justify))
    out.append(_prop("Footprint", footprint, x, y, 0, hide=True))
    out.append(_prop("Datasheet", "", x, y, 0, hide=True))
    out.append(_prop("Description", "", x, y, 0, hide=True))
    for k, v in (fields or {}).items():
        out.append(_prop(k, v, x, y, 0, hide=True))
    for p in pins:
        out.append(f'\t\t(pin "{p}"\n\t\t\t(uuid "{uid(f"pin:{ref}:{p}")}")\n\t\t)\n')
    out.append(
        "\t\t(instances\n"
        f'\t\t\t(project "{DST_NAME}"\n'
        f'\t\t\t\t(path "/{ROOT_UUID}"\n'
        f'\t\t\t\t\t(reference "{ref}")\n'
        "\t\t\t\t\t(unit 1)\n"
        "\t\t\t\t)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        "\t)\n"
    )
    return "".join(out)


def power(kind: str, ref: str, x: float, y: float, rot: int = 0) -> str:
    """power:GND / power:PWR_FLAG / power:+24V ... (single virtual pin)."""
    return symbol(f"power:{kind}", ref, kind, x, y, rot=rot, pins=("1",),
                  ref_off=(0, 3.81), val_off=(0, -3.81), justify="left",
                  hide_value=(kind == "PWR_FLAG"), hide_ref=True)


def wire(x1: float, y1: float, x2: float, y2: float) -> str:
    x1, y1, x2, y2 = n(x1), n(y1), n(x2), n(y2)
    return (
        "\t(wire\n"
        "\t\t(pts\n"
        f"\t\t\t(xy {x1} {y1}) (xy {x2} {y2})\n"
        "\t\t)\n"
        "\t\t(stroke\n"
        "\t\t\t(width 0)\n"
        "\t\t\t(type default)\n"
        "\t\t)\n"
        f'\t\t(uuid "{uid(f"wire:{x1},{y1},{x2},{y2}")}")\n'
        "\t)\n"
    )


def junction(x: float, y: float) -> str:
    x, y = n(x), n(y)
    return (
        "\t(junction\n"
        f"\t\t(at {x} {y})\n"
        "\t\t(diameter 0)\n"
        "\t\t(color 0 0 0 0)\n"
        f'\t\t(uuid "{uid(f"junc:{x},{y}")}")\n'
        "\t)\n"
    )


def glabel(name: str, x: float, y: float, rot: int = 0) -> str:
    x, y = n(x), n(y)
    just = "right" if rot == 180 else "left"
    return (
        f'\t(global_label "{name}"\n'
        "\t\t(shape input)\n"
        f"\t\t(at {x} {y} {rot})\n"
        "\t\t(effects\n"
        "\t\t\t(font\n"
        "\t\t\t\t(size 1.27 1.27)\n"
        "\t\t\t)\n"
        f"\t\t\t(justify {just})\n"
        "\t\t)\n"
        f'\t\t(uuid "{uid(f"glbl:{name}:{x},{y}")}")\n'
        '\t\t(property "Intersheetrefs" "${INTERSHEET_REFS}"\n'
        f"\t\t\t(at {x} {y} 0)\n"
        "\t\t\t(hide yes)\n"
        "\t\t\t(show_name no)\n"
        "\t\t\t(do_not_autoplace no)\n"
        "\t\t\t(effects\n"
        "\t\t\t\t(font\n"
        "\t\t\t\t\t(size 1.27 1.27)\n"
        "\t\t\t\t)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        "\t)\n"
    )


def note(text: str, x: float, y: float, size: float = 1.27) -> str:
    x, y = n(x), n(y)
    return (
        f'\t(text "{text}"\n'
        "\t\t(exclude_from_sim no)\n"
        f"\t\t(at {x} {y} 0)\n"
        "\t\t(effects\n"
        "\t\t\t(font\n"
        f"\t\t\t\t(size {size} {size})\n"
        "\t\t\t)\n"
        "\t\t\t(justify left bottom)\n"
        "\t\t)\n"
        f'\t\t(uuid "{uid(f"text:{x},{y}")}")\n'
        "\t)\n"
    )


# --------------------------------------------------------------------------
# the power section
# --------------------------------------------------------------------------
# Drawn in the empty band below the existing sheet content (A2, content ends
# at y=317).  Everything lands on the 1.27 mm connection grid.
#
#   +24V -+- D3 TVS -+- C1 -+- C2 -+- R1/D4 -+-> U5 LM2596S-5 --L1--+--> D2 -> VCC_5V
#                                                  D1 catch    C3 --+
#   VCC_5V -+- C4 -> U6 AMS1117-3.3 -+- C5 -+- C6 -+- R2/D5 -+-> 3.3v
#   ESP_3V3 --JP1 (0R, DNP)-- 3.3v
#
# Refs: U1-U4 are the stepstick sockets, F1 the existing 24 V fuse, J1..J38 the
# connectors, so the power section starts at U5 / C1 / D1 / L1 / R1 / JP1.

FP_C0805 = "Capacitor_SMD:C_0805_2012Metric"
FP_R0805 = "Resistor_SMD:R_0805_2012Metric"

Y24 = 340.36          # +24V rail
YSW = 345.44          # buck output / switch node row
Y5 = 365.76           # 5 V -> LDO row
YJP = 393.7           # fallback jumper row


def power_section() -> str:
    o = []
    a = o.append

    a(note("INTEGRATED POWER MANAGEMENT  -  24 V in, onboard 5 V buck + 3.3 V LDO",
           38.1, 330.2, size=2.0))

    # ---- 24 V rail --------------------------------------------------------
    a(glabel("+24V", 44.45, Y24, 180))
    a(wire(44.45, Y24, 87.63, Y24))
    a(junction(48.26, Y24))
    a(wire(48.26, Y24, 48.26, 335.28))
    a(power("PWR_FLAG", "#FLG01", 48.26, 335.28))

    # shunt parts on the 24 V rail: TVS clamp, bulk, HF bypass, power LED
    shunts = [
        (53.34, "D3", "Device:D_TVS", "SMBJ33A", "Diode_SMD:D_SMB", 270, ("1", "2")),
        (62.23, "C1", "Device:C_Polarized", "470uF/50V",
         "Capacitor_THT:CP_Radial_D10.0mm_P5.00mm", 0, ("1", "2")),
        (71.12, "C2", "Device:C", "100nF/50V", FP_C0805, 0, ("1", "2")),
    ]
    for x, ref, lib, val, fp, rot, pins in shunts:
        a(junction(x, Y24))
        a(wire(x, Y24, x, 342.9))
        a(symbol(lib, ref, val, x, 346.71, rot=rot, footprint=fp, pins=pins))
        a(wire(x, 350.52, x, 353.06))
        a(power("GND", f"#PWR{ref}", x, 353.06))

    # 24 V present LED: 10k keeps the 0805 resistor inside its 125 mW rating
    a(junction(80.01, Y24))
    a(wire(80.01, Y24, 80.01, 342.9))
    a(symbol("Device:R", "R1", "10k", 80.01, 346.71, footprint=FP_R0805,
             pins=("1", "2")))
    a(wire(80.01, 350.52, 80.01, 354.33))
    a(symbol("Device:LED", "D4", "LED_24V", 80.01, 358.14, rot=90,
             footprint="LED_SMD:LED_0805_2012Metric", pins=("1", "2")))
    a(wire(80.01, 361.95, 80.01, 364.49))
    a(power("GND", "#PWRD4", 80.01, 364.49))

    # ---- buck -------------------------------------------------------------
    a(symbol("Regulator_Switching:LM2596S-5", "U5", "LM2596S-5", 100.33, 342.9,
             footprint="Package_TO_SOT_SMD:TO-263-5_TabPin3",
             pins=("1", "2", "3", "4", "5"),
             ref_off=(0, -13.97), val_off=(0, -11.43), justify="left"))
    a(wire(87.63, YSW, 87.63, 350.52))                 # ~ON/OFF tied low
    a(power("GND", "#PWRU5A", 87.63, 350.52))
    a(wire(100.33, 350.52, 100.33, 353.06))
    a(power("GND", "#PWRU5B", 100.33, 353.06))

    a(wire(113.03, YSW, 120.65, YSW))                  # OUT -> catch diode -> L1
    a(junction(116.84, YSW))
    a(symbol("Device:D_Schottky", "D1", "SS34", 116.84, 349.25, rot=270,
             footprint="Diode_SMD:D_SMC", pins=("1", "2")))
    a(wire(116.84, 353.06, 116.84, 355.6))
    a(power("GND", "#PWRD1", 116.84, 355.6))
    a(symbol("Device:L", "L1", "100uH/3A", 124.46, YSW, rot=90,
             footprint="Inductor_SMD:L_Bourns-SRN8040_8x8.15mm", pins=("1", "2"),
             ref_off=(0, -3.81), val_off=(0, 3.81), justify="left"))

    a(wire(128.27, YSW, 147.32, YSW))                  # buck output node
    a(junction(133.35, YSW))
    a(symbol("Device:C_Polarized", "C3", "220uF/25V",
             133.35, 349.25, footprint="Capacitor_THT:CP_Radial_D8.0mm_P3.50mm",
             pins=("1", "2")))
    a(wire(133.35, 353.06, 133.35, 355.6))
    a(power("GND", "#PWRC3", 133.35, 355.6))
    a(wire(113.03, Y24, 139.7, Y24))                   # FB senses pre-D2
    a(wire(139.7, Y24, 139.7, YSW))
    a(junction(139.7, YSW))

    # OR-ing diode: the buck can push VCC_5V up, but USB or J37 can never push
    # current back into the buck output.
    a(symbol("Device:D_Schottky", "D2", "SS34", 151.13, YSW, rot=180,
             footprint="Diode_SMD:D_SMC", pins=("1", "2"),
             ref_off=(0, -3.81), val_off=(0, 3.81)))
    a(wire(154.94, YSW, 163.83, YSW))
    a(junction(160.02, YSW))
    a(wire(160.02, YSW, 160.02, Y24))
    a(power("PWR_FLAG", "#FLG02", 160.02, Y24))
    a(glabel("VCC_5V", 163.83, YSW, 0))

    # ---- 3.3 V LDO --------------------------------------------------------
    a(glabel("VCC_5V", 44.45, Y5, 180))
    a(wire(44.45, Y5, 67.31, Y5))
    a(junction(59.69, Y5))
    a(symbol("Device:C", "C4", "10uF", 59.69, 369.57, footprint=FP_C0805,
             pins=("1", "2")))
    a(wire(59.69, 373.38, 59.69, 375.92))
    a(power("GND", "#PWRC4", 59.69, 375.92))

    a(symbol("Regulator_Linear:AMS1117-3.3", "U6", "AMS1117-3.3", 74.93, Y5,
             footprint="Package_TO_SOT_SMD:SOT-223-3_TabPin2", pins=("1", "2", "3"),
             ref_off=(0, -8.89), val_off=(0, -6.35)))
    a(wire(74.93, 373.38, 74.93, 375.92))
    a(power("GND", "#PWRU6", 74.93, 375.92))

    a(wire(82.55, Y5, 115.57, Y5))
    for x, ref, val in ((88.9, "C5", "22uF"), (96.52, "C6", "100nF")):
        a(junction(x, Y5))
        a(symbol("Device:C", ref, val, x, 369.57, footprint=FP_C0805, pins=("1", "2")))
        a(wire(x, 373.38, x, 375.92))
        a(power("GND", f"#PWR{ref}", x, 375.92))

    a(junction(105.41, Y5))
    a(symbol("Device:R", "R2", "1k", 105.41, 369.57, footprint=FP_R0805,
             pins=("1", "2")))
    a(wire(105.41, 373.38, 105.41, 377.19))
    a(symbol("Device:LED", "D5", "LED_3V3", 105.41, 381.0, rot=90,
             footprint="LED_SMD:LED_0805_2012Metric", pins=("1", "2")))
    a(wire(105.41, 384.81, 105.41, 387.35))
    a(power("GND", "#PWRD5", 105.41, 387.35))
    a(glabel("3.3v", 115.57, Y5, 0))

    # ---- ground reference flag -------------------------------------------
    a(power("PWR_FLAG", "#FLG03", 33.02, 381.0))
    a(wire(33.02, 381.0, 33.02, 384.81))
    a(power("GND", "#PWRFLG", 33.02, 384.81))

    # ---- fallback jumper --------------------------------------------------
    a(glabel("ESP_3V3", 44.45, YJP, 180))
    a(wire(44.45, YJP, 63.5, YJP))
    a(symbol("Device:R", "JP1", "0R", 67.31, YJP, rot=90, footprint=FP_R0805,
             pins=("1", "2"), ref_off=(0, -3.81), val_off=(0, 3.81), dnp=True))
    a(wire(71.12, YJP, 85.09, YJP))
    a(glabel("3.3v", 85.09, YJP, 0))

    a(note("D2 lets the DevKit's USB 5 V (or J37) override the buck without back-feeding it.",
           166.37, 341.63))
    a(note("3.3v = peripheral rail only: mux, AS5600 heads, OLED, TMC2209 VIO.",
           120.65, 361.95))
    a(note("JP1: fit 0R and leave U6 unpopulated to power 3.3v from the DevKit's LDO instead.",
           90.17, 394.97))
    return "".join(o)


# --------------------------------------------------------------------------
# I2C pull-ups
# --------------------------------------------------------------------------
# The carrier board relies entirely on whatever pull-ups the plug-in breakouts
# happen to carry.  Four buses actually move traffic:
#
#   bus A   GPIO11/12   -> TCA9548A breakout (J20), on-board, short
#   bus B   GPIO13/14   -> SSD1306 OLED (J23), off-board on a short harness
#   CH0     mux ch 0    -> AS5600 pan  (J21), off-board at the joint
#   CH1     mux ch 1    -> AS5600 tilt (J22), off-board at the joint
#
# Mux channels 2-7 only reach the spare header J24, so they get nothing.
# The AS5600 heads sit at the end of the longest cables on the rig, which is
# where rise time actually gets tight - hence the stronger value there.

PULLUP_NETS = [
    ("GPIO_11", "4.7k"),           # bus A SDA
    ("GPIO_12", "4.7k"),           # bus A SCL
    ("GPIO_13", "4.7k"),           # bus B SDA
    ("GPIO_14", "4.7k"),           # bus B SCL
    ("MUX_CH0_SDA", "2.2k"),
    ("MUX_CH0_SCL", "2.2k"),
    ("MUX_CH1_SDA", "2.2k"),
    ("MUX_CH1_SCL", "2.2k"),
]


def pullups(first: int):
    """Designators run from `first` - the two variants number them differently."""
    return [("R%d" % (first + i), net, val)
            for i, (net, val) in enumerate(PULLUP_NETS)]


def i2c_pullup_section(refs, x0: float, y: float = 345.44) -> str:
    o = []
    a = o.append
    a(note("I2C PULL-UPS  -  bus A + bus B at 4.7k, the two AS5600 channels at 2.2k",
           x0 - 7.62, 330.2, size=2.0))

    xs = [x0 + 7.62 * i for i in range(len(refs))]
    a(glabel("3.3v", x0 - 7.62, y, 180))
    a(wire(x0 - 7.62, y, xs[-1], y))
    for x, (ref, net, val) in zip(xs, refs):
        if x != xs[-1]:
            a(junction(x, y))
        a(symbol("Device:R", ref, val, x, y + 3.81, footprint=FP_R0805,
                 pins=("1", "2")))
        a(wire(x, y + 7.62, x, y + 10.16))
        a(glabel(net, x, y + 10.16, 270))

    a(note("Every I2C device here is a plug-in module, so these are belt-and-braces:",
           x0 - 7.62, 391.16))
    a(note("lift one if a module turns out to carry a strong pull-up of its own.",
           x0 - 7.62, 394.97))
    return "".join(o)


# --------------------------------------------------------------------------
# decoupling (turnkey variant)
# --------------------------------------------------------------------------
# Nothing on this board is an IC - every active part is a module in a socket -
# so these are rail caps, not pin caps.  What they are FOR is still specific,
# and the `Decouples` field on each one records the pin it belongs beside so a
# layout pass has something to place against.  The rails are drawn with global
# labels here (that is how the whole sheet is drawn), so the field is the only
# thing carrying that intent - do not drop it.

FP_CP_D10 = "Capacitor_THT:CP_Radial_D10.0mm_P5.00mm"
FP_CP_D8 = "Capacitor_THT:CP_Radial_D8.0mm_P3.50mm"
FP_CP_D63 = "Capacitor_THT:CP_Radial_D6.3mm_P2.50mm"
FP_C0805 = "Capacitor_SMD:C_0805_2012Metric"

CAP_PITCH = 12.7        # room for a "470uF/50V" value string beside each cap

# ref, value, rail, the pin it belongs beside, footprint
DECOUPLING = [
    ("C1", "470uF/50V", "+24V", "J38 24V in", FP_CP_D10),
    ("C2", "100uF/50V", "+24V", "U1 VM", FP_CP_D8),
    ("C3", "100uF/50V", "+24V", "U2 VM", FP_CP_D8),
    ("C4", "100uF/50V", "+24V", "U3 VM", FP_CP_D8),
    ("C5", "100uF/50V", "+24V", "U4 VM", FP_CP_D8),
    ("C6", "100nF/50V", "+24V", "U1 VM", FP_C0805),
    ("C7", "100nF/50V", "+24V", "U2 VM", FP_C0805),
    ("C8", "100nF/50V", "+24V", "U3 VM", FP_C0805),
    ("C9", "100nF/50V", "+24V", "U4 VM", FP_C0805),
    ("C10", "100uF/16V", "VCC_5V", "J37 5V in", FP_CP_D63),
    ("C11", "100nF", "VCC_5V", "J37 5V in", FP_C0805),
    ("C12", "22uF", "3.3v", "J1 3V3", FP_C0805),
    ("C13", "100nF", "3.3v", "J1 3V3", FP_C0805),
    ("C14", "100nF", "3.3v", "J20 VCC", FP_C0805),
    ("C15", "100nF", "3.3v", "J23 VCC", FP_C0805),
    ("C16", "100nF", "3.3v", "J21 VCC", FP_C0805),
    ("C17", "100nF", "3.3v", "J22 VCC", FP_C0805),
]


def _cap_row(rail: str, caps, x0: float, y: float, o) -> None:
    a = o.append
    xs = [x0 + CAP_PITCH * i for i in range(len(caps))]
    a(glabel(rail, x0 - 6.35, y, 180))
    a(wire(x0 - 6.35, y, xs[-1], y))
    for x, (ref, val, _rail, owner, fp) in zip(xs, caps):
        if x != xs[-1]:
            a(junction(x, y))
        polar = fp.startswith("Capacitor_THT")
        a(symbol("Device:C_Polarized" if polar else "Device:C", ref, val,
                 x, y + 3.81, footprint=fp, pins=("1", "2"),
                 fields={"Decouples": owner}))
        a(wire(x, y + 7.62, x, y + 10.16))
        a(power("GND", "#PWR%s" % ref, x, y + 10.16))


def decoupling_section() -> str:
    o = []
    o.append(note("DECOUPLING  -  every cap carries a `Decouples` field naming the "
                  "pin it belongs beside", 40.64, 330.2, size=2.0))
    by_rail = {}
    for c in DECOUPLING:
        by_rail.setdefault(c[2], []).append(c)
    _cap_row("+24V", by_rail["+24V"], 46.99, 345.44, o)
    _cap_row("VCC_5V", by_rail["VCC_5V"], 46.99, 372.11, o)
    _cap_row("3.3v", by_rail["3.3v"], 81.28, 372.11, o)
    o.append(note("The stepsticks carry their own VM bulk, but supply-lead "
                  "inductance is what kills drivers -", 40.64, 391.16))
    o.append(note("C2-C5 belong hard against each socket's VM pin, C1 at the "
                  "screw terminal.", 40.64, 394.97))
    return "".join(o)


def power_flag_section() -> str:
    """Every rail here arrives on a connector, so ERC has nothing driving them."""
    o = []
    a = o.append
    a(note("PWR_FLAGs - all four rails arrive on connectors, nothing on this "
           "board drives them", 248.92, 366.71))
    for i, rail in enumerate(("+24V", "VCC_5V", "3.3v", "GND")):
        x = 248.92 + 15.24 * i
        a(power("PWR_FLAG", "#FLG%02d" % (i + 1), x, 372.11))
        a(wire(x, 372.11, x, 375.92))
        a(glabel(rail, x, 375.92, 270))
    return "".join(o)


# --------------------------------------------------------------------------
# schematic build
# --------------------------------------------------------------------------

SYMBOL_SOURCE = {
    "Device:C": "hybrid",
    "Device:C_Polarized": "hybrid",
    "Device:D_Schottky": "hybrid",
    "Device:L": "hybrid",
    "Device:R": "hybrid",
    "Regulator_Linear:AMS1117-3.3": "hybrid",
    "Regulator_Switching:LM2596S-5": "hybrid",
    "power:PWR_FLAG": "stock",
    "Device:D_TVS": "stock",
    "Device:LED": "stock",
}

PWR_LIB_SYMBOLS = [
    "Device:C", "Device:C_Polarized", "Device:D_Schottky", "Device:L", "Device:R",
    "Regulator_Linear:AMS1117-3.3", "Regulator_Switching:LM2596S-5",
    "power:PWR_FLAG", "Device:D_TVS", "Device:LED",
]

TURNKEY_LIB_SYMBOLS = [
    "Device:C", "Device:C_Polarized", "Device:R", "power:PWR_FLAG",
]

OLED_HEADER_FP = "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical"
OLED_SOCKET_FP = "Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical"


def build_schematic(cfg) -> None:
    src = SRC_SCH.read_text(encoding="utf-8")

    # 1. retarget the instance data at the new project name
    out = src.replace('(project "%s"' % SRC_NAME, '(project "%s"' % DST_NAME)

    # 2. take the DevKit's 3V3 output pins off the peripheral rail -- U6 drives
    #    it now, and two regulators must never sit on one net.
    if cfg["split_esp_3v3"]:
        x, y = J1_3V3_LABEL_AT
        pat = re.compile(r'\(global_label "3\.3v"\n\t\t\(shape input\)\n\t\t\(at %s %s '
                         % (re.escape(str(x)), re.escape(str(y))))
        out, n = pat.subn(lambda m: m.group(0).replace('"3.3v"', '"ESP_3V3"'), out)
        if n != 1:
            raise SystemExit("expected 1 J1 3.3v label at %s, found %d"
                             % (J1_3V3_LABEL_AT, n))

    # 2b. the OLED lands on a male header on the carrier board, so a module
    #     with pins soldered on cannot plug into it.  Make it a socket.
    if cfg["oled_socket"]:
        out, n = re.subn(re.escape(OLED_HEADER_FP), OLED_SOCKET_FP, out)
        if n != 1:
            raise SystemExit("expected 1 J23 header footprint, found %d" % n)

    # 3. cache the symbols the power section needs
    hyb = HYBRID_PWR.read_text(encoding="utf-8")
    proto = DONOR_SCH.read_text(encoding="utf-8")
    have = set(re.findall(r'\n\t\t\(symbol "([^"]+)"', lib_symbols_block(out)[2]))
    add = []
    for name in cfg["lib_symbols"]:
        origin = SYMBOL_SOURCE[name]
        if name in have:
            continue
        if origin == "hybrid":
            add.append(embedded_symbol(hyb, name))
        elif origin == "proto":
            add.append(embedded_symbol(proto, name))
        else:
            lib, bare = name.split(":")
            add.append(library_symbol(lib, bare))
    start, end, blk = lib_symbols_block(out)
    insert_at = start + blk.rstrip().rfind(")")          # just before lib_symbols' ")"
    out = out[:insert_at] + "".join("\t\t" + s + "\n" for s in add) + "\t" + out[insert_at:]

    # 4. the power section itself, ahead of the trailing (sheet_instances ...)
    tail = out.rfind("\t(sheet_instances")
    if tail < 0:
        raise SystemExit("no (sheet_instances) block")
    out = out[:tail] + "".join(s() for s in cfg["sections"]) + out[tail:]

    DST_SCH.write_text(out, encoding="utf-8")
    print("wrote", DST_SCH.name)


def build_project() -> None:
    pro = SRC_PRO.read_text(encoding="utf-8").replace(SRC_NAME, DST_NAME)
    DST_PRO.write_text(pro, encoding="utf-8")
    print("wrote", DST_PRO.name)


# --------------------------------------------------------------------------
# PCB build
# --------------------------------------------------------------------------
# The power parts go in a 47 mm column added to the right of the original
# outline, beside the 24 V terminal and the fuse.  Nothing already on the
# board moves, and no existing track is touched except the two noted below.

BOARD_NEW_RIGHT = 222.0

# Shelf-packed into the new column, in signal-flow order, on courtyard extents
# with a 3 mm gap - non-overlapping but not hand-optimised.  Layout is the
# user's job from here; this only guarantees nothing lands on top of anything.
PACK_X0, PACK_Y0, PACK_W, PACK_GAP = 177.0, 22.0, 42.0, 3.0

PWR_PLACEMENT = [
    ("D3",  "Diode_SMD:D_SMB"),                        # 24 V clamp
    ("C1",  "Capacitor_THT:CP_Radial_D10.0mm_P5.00mm"),
    ("C2",  "Capacitor_SMD:C_0805_2012Metric"),
    ("R1",  "Resistor_SMD:R_0805_2012Metric"),         # 24 V present LED
    ("D4",  "LED_SMD:LED_0805_2012Metric"),
    ("U5",  "Package_TO_SOT_SMD:TO-263-5_TabPin3"),    # buck
    ("L1",  "Inductor_SMD:L_Bourns-SRN8040_8x8.15mm"),
    ("D1",  "Diode_SMD:D_SMC"),
    ("C3",  "Capacitor_THT:CP_Radial_D8.0mm_P3.50mm"),
    ("D2",  "Diode_SMD:D_SMC"),                        # OR into VCC_5V
    ("C4",  "Capacitor_SMD:C_0805_2012Metric"),        # 3.3 V LDO
    ("U6",  "Package_TO_SOT_SMD:SOT-223-3_TabPin2"),
    ("C5",  "Capacitor_SMD:C_0805_2012Metric"),
    ("C6",  "Capacitor_SMD:C_0805_2012Metric"),
    ("R2",  "Resistor_SMD:R_0805_2012Metric"),         # 3.3 V present LED
    ("D5",  "LED_SMD:LED_0805_2012Metric"),
    ("JP1", "Resistor_SMD:R_0805_2012Metric"),         # DevKit 3V3 fallback
] + [(ref, FP_R0805) for ref, _n, _v in pullups(3)]

# The turnkey variant's parts belong scattered across the board, beside the
# sockets they serve - so they are parked OUTSIDE the outline rather than in a
# tidy column that would imply they live there.  See each cap's `Decouples`.
TURNKEY_PACK = (180.0, 20.0, 56.0, 3.0)

TURNKEY_PLACEMENT = ([(ref, fp) for ref, _v, _r, _o, fp in DECOUPLING]
                     + [(ref, FP_R0805) for ref, _n, _v in pullups(1)])

# The two segments that fed the whole 3.3v tree out of the DevKit's 3V3 pins.
# J1.1/J1.2 are ESP_3V3 now, so leaving this copper in place would short two
# nets together.  The rest of the 3.3v tree is untouched and needs a feed from
# U6 -- that is the one piece of re-routing this variant forces.
STALE_3V3_TRACKS = [
    ((124.68, 61.56), (112.50, 73.74)),
    ((123.81, 87.59), (112.50, 76.28)),
]


def netlist_map():
    """{(ref, pad): net} and {ref: value}, straight out of KiCad's own netlist."""
    import tempfile
    tmp = Path(tempfile.gettempdir()) / (DST_NAME + ".net")
    cli = KICAD_PY.parent / "kicad-cli.exe"
    subprocess.run([str(cli), "sch", "export", "netlist", "--format", "kicadsexpr",
                    "--output", str(tmp), str(DST_SCH)], check=True,
                   stdout=subprocess.DEVNULL)
    text = tmp.read_text(encoding="utf-8")

    values = dict(re.findall(r'\(ref "([^"]+)"\)\s*\n\s*\(value "([^"]*)"\)', text))
    nets = {}
    body = text[text.find("(nets"):]
    for m in re.finditer(r'\(net\n?\s*\(code', body):
        blk = sexp_at(body, m.start())
        name = re.search(r'\(name "([^"]+)"\)', blk).group(1)
        for ref, pad in re.findall(r'\(ref "([^"]+)"\)\s*\n?\s*\(pin "([^"]+)"\)', blk):
            nets[(ref, pad)] = name
    return nets, values


def build_pcb(cfg) -> None:
    import pcbnew

    nets, values = netlist_map()
    board = pcbnew.LoadBoard(str(SRC_PCB))
    mm = pcbnew.FromMM

    def net_for(name):
        n = board.FindNet(name)
        if n is None:
            n = pcbnew.NETINFO_ITEM(board, name)
            board.Add(n)
        return n

    # widen the outline so the power section has somewhere to live
    if cfg["board_right"]:
        for d in board.GetDrawings():
            if d.GetLayerName() == "Edge.Cuts" and d.GetShapeStr() == "Rect":
                e = d.GetEnd()
                d.SetEnd(pcbnew.VECTOR2I(mm(cfg["board_right"]), e.y))

    # J1's 3V3 pins leave the peripheral rail
    if cfg["split_esp_3v3"]:
        j1 = next(f for f in board.GetFootprints() if f.GetReference() == "J1")
        for pad in j1.Pads():
            if pad.GetNumber() in ("1", "2"):
                pad.SetNet(net_for("ESP_3V3"))

    # Everything that loads a footprint has to happen before any Remove():
    # board.Remove() invalidates pcbnew's footprint-library plugin handle for
    # the rest of the session.
    fp_root = KICAD_SHARE / "footprints"

    def load(fpid):
        lib, name = fpid.split(":")
        fp = pcbnew.FootprintLoad(str(fp_root / (lib + ".pretty")), name)
        if fp is None:
            raise SystemExit("footprint %s not found" % fpid)
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        return fp

    x0, y0, width, gap = cfg["pack"]
    cx, cy, row_h = x0, y0, 0.0
    decouples = cfg["decouples"]
    for ref, fpid in cfg["placement"]:
        fp = load(fpid)
        fp.SetReference(ref)
        fp.SetValue(values.get(ref, ""))
        if ref in decouples:
            fp.SetField("Decouples", decouples[ref])
            # metadata for the layout pass, not silkscreen
            fld = fp.GetField("Decouples")
            fld.SetVisible(False)
            fld.SetLayer(pcbnew.F_Fab)

        court = fp.GetCourtyard(pcbnew.F_CrtYd)
        bb = court.BBox() if court.OutlineCount() else fp.GetBoundingBox(False, False)
        w, h = bb.GetWidth() / 1e6, bb.GetHeight() / 1e6
        if cx + w > x0 + width:
            cx, cy, row_h = x0, cy + row_h + gap, 0.0
        fp.SetPosition(pcbnew.VECTOR2I(mm(cx) - bb.GetX(), mm(cy) - bb.GetY()))
        cx += w + gap
        row_h = max(row_h, h)

        fp.SetPath(pcbnew.KIID_PATH("/" + uid("sym:" + ref)))
        # nets must be assigned before the board takes ownership of the object
        for pad in fp.Pads():
            nn = nets.get((ref, pad.GetNumber()))
            if nn:
                pad.SetNet(net_for(nn))
        board.Add(fp)

    # The OLED header becomes a socket.  Same pads in the same places, so the
    # copper already routed to J23 stays valid.
    stale_fp = None
    if cfg["oled_socket"]:
        old = next(f for f in board.GetFootprints() if f.GetReference() == "J23")
        keep = {p.GetNumber(): p.GetNet() for p in old.Pads()}
        new = load(OLED_SOCKET_FP)
        new.SetReference("J23")
        new.SetValue(old.GetValue())
        new.SetPosition(old.GetPosition())
        new.SetOrientation(old.GetOrientation())
        new.SetLayer(old.GetLayer())
        new.SetPath(old.GetPath())
        for pad in new.Pads():
            if pad.GetNumber() in keep:
                pad.SetNet(keep[pad.GetNumber()])
        board.Add(new)
        stale_fp = old

    if stale_fp is not None:
        board.Remove(stale_fp)

    removed = 0
    for want in cfg["stale_tracks"]:
        for t in list(board.GetTracks()):
            a, b = t.GetStart(), t.GetEnd()
            ends = {(round(a.x / 1e6, 2), round(a.y / 1e6, 2)),
                    (round(b.x / 1e6, 2), round(b.y / 1e6, 2))}
            if ends == set(want):
                board.Remove(t)
                removed += 1
    if removed != len(cfg["stale_tracks"]):
        raise SystemExit("expected %d stale tracks, removed %d"
                         % (len(cfg["stale_tracks"]), removed))

    board.Save(str(DST_PCB))
    restamp_uuids(DST_PCB, [r for r, _f in cfg["placement"]]
                  + (["J23"] if cfg["oled_socket"] else []))
    print("wrote", DST_PCB.name)


def restamp_uuids(path: Path, refs) -> None:
    """Make the footprints we just added reproducible.

    pcbnew mints a fresh KIID for every pad and graphic each time a footprint
    comes out of a library, and then writes the board sorted by those KIIDs -
    so two runs would otherwise differ in a few hundred UUIDs and in where the
    new blocks landed in the file.  Pull our own footprints out, give them
    derived UUIDs, and put them back in reference order after the last one we
    did not touch.
    """
    text = path.read_text(encoding="utf-8")
    wanted = set(refs)
    mine, keep_end = [], None
    spans = []
    for m in re.finditer(r"\n\t\(footprint ", text):
        start = m.start() + 1
        blk = sexp_at(text, start)
        rm = re.search(r'\(property "Reference" "([^"]+)"', blk)
        ref = rm.group(1) if rm else ""
        if ref in wanted:
            i = [0]

            def sub(mm, ref=ref, i=i):
                i[0] += 1
                return '(uuid "%s")' % uid("fp:%s:%d" % (ref, i[0]))

            mine.append((ref, re.sub(r'\(uuid "[0-9a-f-]+"\)', sub, blk)))
            spans.append((start, start + len(blk)))
        else:
            keep_end = start + len(blk)
    if not mine:
        return

    out, pos = [], 0
    for s, e in spans:                       # drop ours where pcbnew put them
        out.append(text[pos:s])
        pos = e
        while pos < len(text) and text[pos] == "\n":
            pos += 1
        out.append("")
    out.append(text[pos:])
    stripped = "".join(out)

    def key(item):
        m = re.match(r"([A-Za-z#]+)(\d*)", item[0])
        return (m.group(1), int(m.group(2) or 0))

    # blk already carries its leading tab: sexp_at() was handed the index of
    # the tab, not of the paren.
    body = "".join(blk + "\n" for _r, blk in sorted(mine, key=key))
    last = None
    for m in re.finditer(r"\n\t\(footprint ", stripped):
        last = m.start() + 1 + len(sexp_at(stripped, m.start() + 1)) + 1
    if last is None:
        raise SystemExit("no footprint blocks left to anchor against")
    path.write_text(stripped[:last] + body + stripped[last:], encoding="utf-8")


# --------------------------------------------------------------------------

VARIANTS = {
    # onboard 24V->5V buck + 5V->3V3 LDO, I2C pull-ups
    "pwr": dict(
        name="pantiltslide_full_pwr",
        sections=[lambda: power_section(),
                  lambda: i2c_pullup_section(pullups(3), 248.92)],
        lib_symbols=PWR_LIB_SYMBOLS,
        split_esp_3v3=True,
        oled_socket=False,
        placement=PWR_PLACEMENT,
        pack=(PACK_X0, PACK_Y0, PACK_W, PACK_GAP),
        board_right=BOARD_NEW_RIGHT,
        stale_tracks=STALE_3V3_TRACKS,
        decouples={},
    ),
    # no regulation: external 5V as on the carrier board, but decoupled,
    # pulled up, and socketed everywhere so an assembly house can finish it
    "turnkey": dict(
        name="pantiltslide_full_turnkey",
        sections=[lambda: decoupling_section(),
                  lambda: i2c_pullup_section(pullups(1), 180.34),
                  lambda: power_flag_section()],
        lib_symbols=TURNKEY_LIB_SYMBOLS,
        split_esp_3v3=False,
        oled_socket=True,
        placement=TURNKEY_PLACEMENT,
        pack=TURNKEY_PACK,
        board_right=None,
        stale_tracks=[],
        decouples={ref: owner for ref, _v, _r, owner, _f in DECOUPLING},
    ),
}


def run(key: str, sch_only: bool) -> None:
    cfg = VARIANTS[key]
    set_variant(cfg["name"])
    build_schematic(cfg)
    build_project()
    if sch_only:
        return
    build_pcb(cfg)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", choices=sorted(VARIANTS) + ["all"], default="all")
    ap.add_argument("--sch-only", action="store_true")
    args = ap.parse_args()

    keys = sorted(VARIANTS) if args.variant == "all" else [args.variant]
    if not args.sch_only:
        try:
            import pcbnew                                # noqa: F401
        except ImportError:
            sys.exit(subprocess.call([str(KICAD_PY), __file__] + sys.argv[1:]))
    for k in keys:
        run(k, args.sch_only)


ROOT_UUID = re.search(r'\(uuid "([0-9a-f-]+)"\)',
                      SRC_SCH.read_text(encoding="utf-8")[:400]).group(1)

if __name__ == "__main__":
    main()
