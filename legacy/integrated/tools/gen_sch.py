# -*- coding: utf-8 -*-
"""Generator for pantiltslide_integrated.kicad_sch (KiCad 9 native format, v20250114)."""
import uuid, re, os

def U():
    return str(uuid.uuid4())

def esc(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')

KICAD_SYM_DIR = r"C:\Program Files\KiCad\9.0\share\kicad\symbols"

# ---------------------------------------------------------------------------
# Pin tables: sym_name -> pin_number(str) -> (pin_name, x, y, rot)
# rot convention (matches real kicad pin "at ... rot"):
#   0=points -x(left) 90=points -y(down) 180=points +x(right) 270=points +y(up)
# ---------------------------------------------------------------------------
PINTABLES = {}

PINTABLES["R"] = {"1": ("~", 0, 3.81, 270), "2": ("~", 0, -3.81, 90)}
PINTABLES["C"] = {"1": ("~", 0, 3.81, 270), "2": ("~", 0, -3.81, 90)}
PINTABLES["C_Polarized"] = {"1": ("~", 0, 3.81, 270), "2": ("~", 0, -3.81, 90)}
PINTABLES["L"] = {"1": ("1", 0, 3.81, 270), "2": ("2", 0, -3.81, 90)}
PINTABLES["Fuse"] = {"1": ("~", 0, 3.81, 270), "2": ("~", 0, -3.81, 90)}
PINTABLES["D_Schottky"] = {"1": ("K", -3.81, 0, 0), "2": ("A", 3.81, 0, 180)}
PINTABLES["SW_Push"] = {"1": ("1", -5.08, 0, 0), "2": ("2", 5.08, 0, 180)}

# verified against this project's own existing embedded copies
PINTABLES["Screw_Terminal_01x02"] = {
    "1": ("Pin_1", -5.08, 0, 0),
    "2": ("Pin_2", -5.08, -2.54, 0),
}
PINTABLES["Conn_01x04_Pin"] = {
    "1": ("Pin_1", 5.08, 2.54, 180),
    "2": ("Pin_2", 5.08, 0, 180),
    "3": ("Pin_3", 5.08, -2.54, 180),
    "4": ("Pin_4", 5.08, -5.08, 180),
}

PINTABLES["GND"]  = {"1": ("~", 0, 0, 270)}
PINTABLES["+3V3"] = {"1": ("~", 0, 0, 90)}
PINTABLES["+5V"]  = {"1": ("~", 0, 0, 90)}
PINTABLES["+24V"] = {"1": ("~", 0, 0, 90)}

PINTABLES["LM2596S-5"] = {
    "1": ("VIN", -12.7, 2.54, 0),
    "2": ("OUT", 12.7, -2.54, 180),
    "3": ("GND", 0, -7.62, 90),
    "4": ("FB", 12.7, 2.54, 180),
    "5": ("~{ON}/OFF", -12.7, -2.54, 0),
}
PINTABLES["AMS1117-3.3"] = {
    "1": ("GND", 0, -7.62, 90),
    "2": ("VO", 7.62, 0, 180),
    "3": ("VI", -7.62, 0, 0),
}
PINTABLES["TMC2209-LA"] = {
    "1":  ("OB2", 15.24, -10.16, 180),
    "2":  ("~{EN}", -15.24, -7.62, 0),
    "3":  ("GND", 0, -25.4, 90),
    "4":  ("CPO", 15.24, 10.16, 180),
    "5":  ("CPI", 15.24, 5.08, 180),
    "6":  ("VCP", 5.08, 27.94, 270),
    "7":  ("SPREAD", -15.24, -2.54, 0),
    "8":  ("5VOUT", 15.24, 20.32, 180),
    "9":  ("MS1/AD0", -15.24, 2.54, 0),
    "10": ("MS2/AD1", -15.24, 0, 0),
    "11": ("DIAG", -15.24, -15.24, 0),
    "12": ("INDEX", -15.24, -17.78, 0),
    "13": ("CLK", -15.24, 12.7, 0),
    "14": ("~{PD}/UART", -15.24, 7.62, 0),
    "15": ("VCC_IO", -2.54, 27.94, 270),
    "16": ("STEP", -15.24, 20.32, 0),
    "17": ("VREF", 15.24, 15.24, 180),
    "18": ("GND", 0, -25.4, 90),
    "19": ("DIR", -15.24, 17.78, 0),
    "20": ("STDBY", -15.24, -10.16, 0),
    "21": ("OA2", 15.24, -2.54, 180),
    "22": ("VS", 0, 27.94, 270),
    "23": ("BRA", 15.24, -15.24, 180),
    "24": ("OA1", 15.24, 0, 180),
    "25": ("NC", 2.54, -25.4, 90),
    "26": ("OB1", 15.24, -7.62, 180),
    "27": ("BRB", 15.24, -17.78, 180),
    "28": ("VS", 0, 27.94, 270),
    "29": ("GND", 0, -25.4, 90),
}
PINTABLES["TCA9548APWR"] = {
    "1":  ("A0", -10.16, -12.7, 0),
    "2":  ("A1", -10.16, -10.16, 0),
    "3":  ("~{RESET}", -10.16, 5.08, 0),
    "4":  ("SD0", 10.16, 15.24, 180),
    "5":  ("SC0", 10.16, 17.78, 180),
    "6":  ("SD1", 10.16, 10.16, 180),
    "7":  ("SC1", 10.16, 12.7, 180),
    "12": ("GND", 0, -25.4, 90),
    "21": ("A2", -10.16, -7.62, 0),
    "22": ("SCL", -10.16, 17.78, 0),
    "23": ("SDA", -10.16, 15.24, 0),
    "24": ("VCC", 0, 22.86, 270),
}
PINTABLES["USB_C_Receptacle_USB2.0_14P"] = {
    "S1": ("SHIELD", -7.62, -22.86, 90),
    "A1": ("GND", 0, -22.86, 90),
    "A4": ("VBUS", 15.24, 15.24, 180),
    "A5": ("CC1", 15.24, 10.16, 180),
    "B5": ("CC2", 15.24, 7.62, 180),
    "A7": ("D-", 15.24, 2.54, 180),
    "A6": ("D+", 15.24, -2.54, 180),
}
PINTABLES["AS5600"] = {
    "1": ("VDD5V", -7.62, 3.81, 0),
    "2": ("VDD3V3", -7.62, 1.27, 0),
    "3": ("OUT", -7.62, -1.27, 0),
    "4": ("GND", -7.62, -3.81, 0),
    "5": ("PGO", 7.62, -3.81, 180),
    "6": ("SDA", 7.62, -1.27, 180),
    "7": ("SCL", 7.62, 1.27, 180),
    "8": ("DIR", 7.62, 3.81, 180),
}
_esp_left = [
    ("1", "GND"), ("2", "3V3"), ("3", "EN"), ("4", "IO4"), ("5", "IO5"),
    ("6", "IO6"), ("7", "IO7"), ("8", "IO15"), ("9", "IO16"), ("10", "IO17"),
    ("11", "IO18"), ("12", "IO8"), ("13", "IO19"), ("14", "IO20"),
    ("15", "IO3"), ("16", "IO46"), ("17", "IO9"), ("18", "IO10"),
    ("19", "IO11"), ("20", "IO12"), ("21", "IO13"),
]
_esp_right = [
    ("22", "IO14"), ("23", "IO21"), ("24", "IO47"), ("25", "IO48"),
    ("26", "IO45"), ("27", "IO0"), ("28", "IO35"), ("29", "IO36"),
    ("30", "IO37"), ("31", "IO38"), ("32", "IO39"), ("33", "IO40"),
    ("34", "IO41"), ("35", "IO42"), ("36", "RXD0"), ("37", "TXD0"),
    ("38", "IO2"), ("39", "IO1"), ("40", "GND"), ("41", "GND"),
]
_pt = {}
for i, (num, nm) in enumerate(_esp_left):
    y = 25.4 - i * 2.54
    _pt[num] = (nm, -17.78, y, 0)
for j, (num, nm) in enumerate(_esp_right):
    y = -25.4 + j * 2.54
    _pt[num] = (nm, 17.78, y, 180)
PINTABLES["ESP32-S3-WROOM-1"] = _pt

# Direction the wire stub runs AWAY from the symbol body, in screen coords.
# X is unchanged by placement, but Y is negated (see Component.pin_abs), so a
# pin declared at angle 270 (connection point above the body in library space)
# exits UPWARD on screen, i.e. toward smaller Y.
_EXIT_DIR = {0: (-1, 0), 180: (1, 0), 270: (0, -1), 90: (0, 1)}
STUB_LEN = 2.54

SECTIONS = []   # (text, x, y) headings drawn on the sheet

# ---------------------------------------------------------------------------
# lib_symbols text extraction from real KiCad libraries
# ---------------------------------------------------------------------------
def find_matching_paren(text, start):
    depth = 1
    i = start + 1
    in_string = False
    while i < len(text) and depth > 0:
        c = text[i]
        if in_string:
            if c == '\\':
                i += 2
                continue
            elif c == '"':
                in_string = False
        else:
            if c == '"':
                in_string = True
            elif c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
        i += 1
    return i - 1

_lib_cache = {}
def extract_symbol(libfile, symname):
    key = (libfile, symname)
    if key in _lib_cache:
        return _lib_cache[key]
    path = os.path.join(KICAD_SYM_DIR, libfile)
    text = open(path, encoding="utf-8").read()
    pat = re.compile(r'\(symbol\s+"' + re.escape(symname) + r'"')
    m = pat.search(text)
    if not m:
        raise KeyError(f"{symname} not found in {libfile}")
    start = m.start()
    open_paren = text.rfind('(', 0, m.start() + 1)
    # m.start() is at the '(' of "(symbol" itself
    start = m.start()
    end = find_matching_paren(text, start)
    block = text[start:end + 1]
    _lib_cache[key] = block
    return block

_PIN_RE = re.compile(
    r'\(pin\s+(\w+)\s+line\s*'
    r'(?:\(hide yes\)\s*)?'
    r'\(at\s+([\-\d.]+)\s+([\-\d.]+)\s+([\-\d.]+)\)\s*'
    r'\(length\s+[\-\d.]+\)\s*'
    r'(?:\(hide yes\)\s*)?'
    r'\(name\s+"((?:[^"\\]|\\.)*)"',
    re.S,
)
_NUM_RE = re.compile(r'\(number\s+"((?:[^"\\]|\\.)*)"')

def auto_pins_in_declared_order(block):
    """Parse every (pin ...) in a symbol block, in file order, returning an
    OrderedDict[number] = (name, x, y, rot). This is the ground truth for
    both coordinates AND declaration order -- KiCad 9's schematic loader
    requires a symbol instance's per-pin uuid list to be declared in the
    SAME ORDER as the embedded library symbol's own pin declarations, not
    merely matched by number text. Hand-transcribing pins risks getting
    that order wrong (silently breaking connectivity), so every pin table
    for a real (non-custom) library symbol is derived from this parser
    rather than typed by hand.
    """
    from collections import OrderedDict
    out = OrderedDict()
    for m in re.finditer(r'\(pin\s+\w+\s+line', block):
        start = m.start()
        end = find_matching_paren(block, start)
        pin_block = block[start:end + 1]
        at_m = re.search(r'\(at\s+([\-\d.]+)\s+([\-\d.]+)\s+([\-\d.]+)\)', pin_block)
        name_m = re.search(r'\(name\s+"((?:[^"\\]|\\.)*)"', pin_block)
        num_m = re.search(r'\(number\s+"((?:[^"\\]|\\.)*)"', pin_block)
        x, y, rot = float(at_m.group(1)), float(at_m.group(2)), int(float(at_m.group(3)))
        out[num_m.group(1)] = (name_m.group(1), x, y, rot)
    return out

def reindent(block, indent="\t\t"):
    lines = block.split("\n")
    out = [indent + lines[0]]
    for ln in lines[1:]:
        out.append(indent + ln if ln.strip() else ln)
    return "\n".join(out)

# ---------------------------------------------------------------------------
# Replace every hand-transcribed PINTABLES entry for a real library symbol
# with one parsed straight from the library text -- guarantees correct
# coordinates AND correct declaration order (see auto_pins_in_declared_order
# docstring) instead of trusting manual transcription.
# ---------------------------------------------------------------------------
_AUTO_LIB_SYMS = [
    ("Device.kicad_sym", "R"),
    ("Device.kicad_sym", "C"),
    ("Device.kicad_sym", "C_Polarized"),
    ("Device.kicad_sym", "L"),
    ("Device.kicad_sym", "D_Schottky"),
    ("Device.kicad_sym", "Fuse"),
    ("Switch.kicad_sym", "SW_Push"),
    ("Connector.kicad_sym", "Screw_Terminal_01x02"),
    ("Connector.kicad_sym", "Conn_01x04_Pin"),
    ("Connector.kicad_sym", "Conn_01x02_Pin"),
    ("Connector.kicad_sym", "USB_C_Receptacle_USB2.0_14P"),
    ("Driver_Motor.kicad_sym", "TMC2209-LA"),
    ("Interface_Expansion.kicad_sym", "TCA9548APWR"),
    ("power.kicad_sym", "GND"),
    ("power.kicad_sym", "+3V3"),
    ("power.kicad_sym", "+5V"),
    ("power.kicad_sym", "+24V"),
    ("power.kicad_sym", "PWR_FLAG"),
]
for _lf, _sn in _AUTO_LIB_SYMS:
    PINTABLES[_sn] = auto_pins_in_declared_order(extract_symbol(_lf, _sn))

# extends-based symbols: the instance's visible pins live entirely on the
# BASE symbol (the derived block only carries properties), so the pin
# table -- and its order -- must come from the base too.
PINTABLES["LM2596S-5"] = auto_pins_in_declared_order(
    extract_symbol("Regulator_Switching.kicad_sym", "LM2596S-12"))
PINTABLES["AMS1117-3.3"] = auto_pins_in_declared_order(
    extract_symbol("Regulator_Linear.kicad_sym", "AP1117-15"))

# ---------------------------------------------------------------------------
# Custom symbol lib_symbols text (hand-authored, verified pinouts)
# ---------------------------------------------------------------------------

def pin_block(number, name, x, y, rot, ptype="passive"):
    return f'''\t\t\t(pin {ptype} line
\t\t\t\t(at {x} {y} {rot})
\t\t\t\t(length 2.54)
\t\t\t\t(name "{esc(name)}" (effects (font (size 1.27 1.27))))
\t\t\t\t(number "{number}" (effects (font (size 1.27 1.27))))
\t\t\t)'''

def custom_AS5600():
    pins = PINTABLES["AS5600"]
    ptype = {"1": "power_in", "2": "power_in", "3": "output", "4": "power_in",
             "5": "input", "6": "bidirectional", "7": "input", "8": "input"}
    body = '\t\t\t(rectangle (start -5.08 6.35) (end 5.08 -6.35)\n' \
           '\t\t\t\t(stroke (width 0.254) (type default)) (fill (type background)))'
    pins_txt = "\n".join(pin_block(n, nm, x, y, rot, ptype[n]) for n, (nm, x, y, rot) in pins.items())
    return (
        '\t\t(symbol "New_Library:AS5600"\n'
        '\t\t\t(exclude_from_sim no) (in_bom yes) (on_board yes)\n'
        '\t\t\t(property "Reference" "U" (at -5.08 7.62 0) (effects (font (size 1.27 1.27))))\n'
        '\t\t\t(property "Value" "AS5600" (at 5.08 7.62 0) (effects (font (size 1.27 1.27))))\n'
        '\t\t\t(property "Footprint" "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" (at 0 -8.89 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '\t\t\t(property "Datasheet" "https://ams-osram.com/products/sensor-ics/position-sensors/ams-as5600-position-sensor" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '\t\t\t(property "Description" "12-bit magnetic rotary position sensor, I2C, SOIC-8" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '\t\t\t(symbol "AS5600_0_1"\n' + body + '\n\t\t\t)\n'
        '\t\t\t(symbol "AS5600_1_1"\n' + pins_txt + '\n\t\t\t)\n'
        '\t\t\t(embedded_fonts no)\n'
        '\t\t)'
    )

def custom_ESP32S3WROOM1():
    pins = PINTABLES["ESP32-S3-WROOM-1"]
    def ptype(nm):
        if nm in ("GND", "3V3"):
            return "power_in"
        if nm == "EN":
            return "input"
        return "bidirectional"
    body = '\t\t\t(rectangle (start -15.24 27.94) (end 15.24 -27.94)\n' \
           '\t\t\t\t(stroke (width 0.254) (type default)) (fill (type background)))'
    pins_txt = "\n".join(
        pin_block(n, nm, x, y, rot, ptype(nm)) for n, (nm, x, y, rot) in pins.items()
    )
    return (
        '\t\t(symbol "New_Library:ESP32-S3-WROOM-1"\n'
        '\t\t\t(exclude_from_sim no) (in_bom yes) (on_board yes)\n'
        '\t\t\t(property "Reference" "U" (at -15.24 29.21 0) (effects (font (size 1.27 1.27))))\n'
        '\t\t\t(property "Value" "ESP32-S3-WROOM-1-N16R8" (at 15.24 29.21 0) (effects (font (size 1.27 1.27))))\n'
        '\t\t\t(property "Footprint" "RF_Module:ESP32-S3-WROOM-1" (at 0 -29.21 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '\t\t\t(property "Datasheet" "https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '\t\t\t(property "Description" "ESP32-S3 WiFi/BLE module, integrated antenna+crystal, N16R8 (16MB flash/8MB PSRAM)" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '\t\t\t(symbol "ESP32-S3-WROOM-1_0_1"\n' + body + '\n\t\t\t)\n'
        '\t\t\t(symbol "ESP32-S3-WROOM-1_1_1"\n' + pins_txt + '\n\t\t\t)\n'
        '\t\t\t(embedded_fonts no)\n'
        '\t\t)'
    )

# ---------------------------------------------------------------------------
# Board builder
# ---------------------------------------------------------------------------

class Component:
    def __init__(self, ref, lib_id, pinkey, value, footprint, x, y,
                 datasheet="", description="", dnp=False, extra_props=None):
        self.ref = ref
        self.lib_id = lib_id
        self.pinkey = pinkey
        self.value = value
        self.footprint = footprint
        # snap to KiCad's 1.27mm (50mil) connection grid so every pin
        # (base placement + a pin-table offset that's always a multiple of
        # 1.27) lands EXACTLY on-grid with zero floating-point residue
        self.x = round(round(x / 1.27) * 1.27, 4)
        self.y = round(round(y / 1.27) * 1.27, 4)
        self.datasheet = datasheet
        self.description = description
        self.dnp = dnp
        self.mirror = False   # (mirror y) -> pins flip to the other side
        self.extra_props = extra_props or {}
        self.uuid = U()
        self.pin_uuids = {}

    def pin_abs(self, num):
        # KiCad NEGATES Y when placing a library symbol: a pin at library
        # coordinate (lx, ly) lands at screen (sym_x + lx, sym_y - ly).
        # Verified against this project's own pantiltslide_full.kicad_sch --
        # J21 (Conn_01x04_Pin) placed at (430,40) has pin1 at y=37.46 (=40-2.54)
        # and pin4 at y=45.08 (=40-(-5.08)). Adding ly instead mirrors every
        # symbol vertically, which silently maps labels onto the wrong pins.
        num = str(num)
        name, lx, ly, rot = PINTABLES[self.pinkey][num]
        if self.mirror:            # (mirror y) negates X and flips L/R pin exits
            lx = -lx
            rot = {0: 180, 180: 0}.get(rot, rot)
        return (round(self.x + lx, 4), round(self.y - ly, 4), rot, name)


class Board:
    def __init__(self):
        self.components = []
        self.counters = {}
        self.nets = {}          # net_name -> list of (component, pin_num)
        self.no_connects = []   # list of (x,y)
        self.direct = []        # (compA,pinA,compB,pinB) real wires
        self.owner = {}         # cap ref -> (ic ref, rail) for placement
        self.wired_pins = set() # pins joined by a real wire -> no stub/label
        self.stub_len = {}      # (ref,pin) -> stub length, for fanned rail symbols
        self.nc_pins = set()    # (ref, pin_num) already no-connected, dedup guard
        self.lib_needed = set()  # (lib_file, sym_name) for extract_symbol
        self.custom_syms = {}    # lib_id -> sexp text

    def next_ref(self, prefix):
        n = self.counters.get(prefix, 0) + 1
        self.counters[prefix] = n
        return f"{prefix}{n}"

    def place(self, prefix, lib_id, pinkey, value, footprint, x, y,
              datasheet="", description="", ref=None, dnp=False):
        ref = ref or self.next_ref(prefix)
        c = Component(ref, lib_id, pinkey, value, footprint, x, y,
                       datasheet, description, dnp)
        self.components.append(c)
        return c

    def net(self, name, *pairs):
        """pairs: (component, pin_num_or_list)"""
        lst = self.nets.setdefault(name, [])
        for comp, pins in pairs:
            if isinstance(pins, (list, tuple)):
                for p in pins:
                    lst.append((comp, str(p)))
            else:
                lst.append((comp, str(pins)))

    def wire(self, a, pa, b, pb, net=None, suppress="both"):
        """Draw a REAL wire between two pins instead of naming both with a
        global label.

        Series topology (input -> fuse -> rail, switch node -> inductor) cannot
        be read off a label, and two pins of the same part carelessly given the
        same label silently short it out -- which is exactly how the input fuse
        ended up bypassed. A drawn wire makes the current path explicit.
        """
        pa, pb = str(pa), str(pb)
        self.direct.append((a, pa, b, pb, net))
        # `suppress` says which ends lose their own stub/label. A chip supply
        # pin keeps its rail symbol (that is where the power comes from) while
        # the cap end is fed entirely by this wire.
        if suppress in ("both", "a"):
            self.wired_pins.add((a.ref, pa))
        if suppress in ("both", "b"):
            self.wired_pins.add((b.ref, pb))
        if net:
            self.net(net, (a, pa), (b, pb))

    def nc(self, comp, pin_num):
        pin_num = str(pin_num)
        key = (comp.ref, pin_num)
        if key in self.nc_pins:
            return
        self.nc_pins.add(key)
        x, y, rot, name = comp.pin_abs(pin_num)
        self.no_connects.append((x, y))

    def use_lib(self, libfile, symname):
        self.lib_needed.add((libfile, symname))
        return f"{libfile.replace('.kicad_sym','')}:{symname}"


B = Board()

# convenience wrappers for standard device-library parts -------------------

def R(value, x, y, footprint="Resistor_SMD:R_0603_1608Metric", ref=None):
    lib_id = B.use_lib("Device.kicad_sym", "R")
    return B.place("R", lib_id, "R", value, footprint, x, y, ref=ref)

def C(value, x, y, footprint="Capacitor_SMD:C_0603_1608Metric", ref=None):
    lib_id = B.use_lib("Device.kicad_sym", "C")
    return B.place("C", lib_id, "C", value, footprint, x, y, ref=ref)

def CP(value, x, y, footprint="Capacitor_THT:CP_Radial_D8.0mm_P3.50mm", ref=None):
    lib_id = B.use_lib("Device.kicad_sym", "C_Polarized")
    return B.place("C", lib_id, "C_Polarized", value, footprint, x, y, ref=ref)

def L(value, x, y, footprint="Inductor_SMD:L_Bourns-SRN8040_8x8.15mm", ref=None):
    lib_id = B.use_lib("Device.kicad_sym", "L")
    return B.place("L", lib_id, "L", value, footprint, x, y, ref=ref)

def FUSE(value, x, y, footprint="Fuse:Fuseholder_Clip-5x20mm_Littelfuse_111_Inline_P20.00x5.00mm_D1.05mm_Horizontal", ref=None):
    lib_id = B.use_lib("Device.kicad_sym", "Fuse")
    return B.place("F", lib_id, "Fuse", value, footprint, x, y, ref=ref)

def DSCHOTTKY(value, x, y, footprint="Diode_SMD:D_SOD-123", ref=None):
    lib_id = B.use_lib("Device.kicad_sym", "D_Schottky")
    return B.place("D", lib_id, "D_Schottky", value, footprint, x, y, ref=ref)

def SWPUSH(value, x, y, footprint="Button_Switch_THT:SW_PUSH_6mm", ref=None):
    lib_id = B.use_lib("Switch.kicad_sym", "SW_Push")
    return B.place("SW", lib_id, "SW_Push", value, footprint, x, y, ref=ref)

# Screw terminal is reserved for the 24V input only -- it takes bare motor
# supply leads and is the one connector that benefits from a captive screw.
def SCREW2(value, x, y, footprint="TerminalBlock:TerminalBlock_Altech_AK300-2_P5.00mm", ref=None):
    lib_id = B.use_lib("Connector.kicad_sym", "Screw_Terminal_01x02")
    return B.place("J", lib_id, "Screw_Terminal_01x02", value, footprint, x, y, ref=ref)

# Every off-board wiring harness uses JST XH: polarised (cannot be plugged
# in backwards), latching, and rated 3A/contact -- comfortably above the
# TMC2209's 2A RMS ceiling for the motor runs.
def JST_XH4(value, x, y, footprint="Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical", ref=None):
    lib_id = B.use_lib("Connector.kicad_sym", "Conn_01x04_Pin")
    return B.place("J", lib_id, "Conn_01x04_Pin", value, footprint, x, y, ref=ref)

def JST_XH2(value, x, y, footprint="Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical", ref=None):
    lib_id = B.use_lib("Connector.kicad_sym", "Conn_01x02_Pin")
    return B.place("J", lib_id, "Conn_01x02_Pin", value, footprint, x, y, ref=ref)

def PWR(sym, x, y):
    lib_id = f"power:{sym}"
    B.lib_needed.add(("power.kicad_sym", sym))
    return B.place("#PWR", lib_id, sym, sym, "", x, y, ref=None)

print("board builder ready")

# ===========================================================================
# CIRCUIT
# ===========================================================================

# --- POWER SECTION ---------------------------------------------------------
J_pwr = SCREW2("24V_In", 48, 75)
J_pwr.mirror = True          # pins face right, toward the fuse
# F1 sits in line with J_pwr pin 1 so input -> fuse is a straight wire
F1 = FUSE("2A", 75, 78.81)
U_buck = B.place("U", B.use_lib("Regulator_Switching.kicad_sym", "LM2596S-5"),
                  "LM2596S-5", "LM2596S-5", "Package_TO_SOT_SMD:TO-263-5_TabPin3", 118, 82,
                  datasheet="http://www.ti.com/lit/ds/symlink/lm2596.pdf",
                  description="24V->5V buck regulator, 3A, fixed 5V, TO-263-5")
L1 = L("100uH", 165, 78)
D1 = DSCHOTTKY("SS34", 143, 103, footprint="Diode_SMD:D_SMC")
C_cin_bulk = CP("100uF_35V", 92, 110)
B.owner[C_cin_bulk.ref] = (U_buck.ref, "+24V")
C_cin_hf = C("100nF", 106, 110)
B.owner[C_cin_hf.ref] = (U_buck.ref, "+24V")
C_cout_bulk = CP("220uF_25V", 182, 110)
B.owner[C_cout_bulk.ref] = (U_buck.ref, "+5V")
U_ldo = B.place("U", B.use_lib("Regulator_Linear.kicad_sym", "AMS1117-3.3"),
                 "AMS1117-3.3", "AMS1117-3.3", "Package_TO_SOT_SMD:SOT-223-3_TabPin2", 118, 155,
                 datasheet="http://www.advanced-monolithic.com/pdf/ds1117.pdf",
                 description="5V->3.3V LDO, 1A, SOT-223")
C_ldo_in = C("10uF", 92, 172)
B.owner[C_ldo_in.ref] = (U_ldo.ref, "+5V")
C_ldo_out = C("22uF", 152, 172)
B.owner[C_ldo_out.ref] = (U_ldo.ref, "+3V3")
D_usb_or = DSCHOTTKY("SS14", 62, 130, footprint="Diode_SMD:D_SOD-123")

# The raw input side of the fuse MUST be its own net. Putting both fuse
# terminals on "+24V" shorts the fuse out and removes all overcurrent
# protection -- the failure mode that hides behind label-only wiring.
B.wire(J_pwr, 1, F1, 1, net="+24V_RAW")
B.net("GND", (J_pwr, 2))

# fuse output rail (post-fuse, protected 24V rail feeding the buck) is
# also called "+24V" for simplicity (single 24V domain on this board)
# post-fuse, protected rail -- everything downstream hangs off this
B.net("+24V", (F1, 2), (U_buck, 1), (C_cin_bulk, 1), (C_cin_hf, 1))
# switch node: regulator OUT -> catch diode cathode -> inductor.
# Drawn, because the ORDER of this node is the whole buck topology.
B.wire(U_buck, 2, L1, 1, net="SW_NODE")
B.wire(D1, 1, L1, 1, net="SW_NODE")
B.net("+5V", (L1, 2), (U_buck, 4), (C_cout_bulk, 1), (U_ldo, 3), (C_ldo_in, 1),
       (D_usb_or, 1))   # cathode -> current flows USB_VBUS into +5V
B.net("+3V3", (U_ldo, 2), (C_ldo_out, 1))
B.net("USB_VBUS", (D_usb_or, 2))  # anode; J_usb VBUS joins this net below

for comp, pin in [(U_buck, 3), (U_buck, 5), (D1, 2), (C_cin_bulk, 2), (C_cin_hf, 2),
                   (C_cout_bulk, 2), (U_ldo, 1), (C_ldo_in, 2), (C_ldo_out, 2)]:
    B.net("GND", (comp, pin))

# --- MCU SECTION -------------------------------------------------------------
U_esp = B.place("U", "New_Library:ESP32-S3-WROOM-1", "ESP32-S3-WROOM-1",
                 "ESP32-S3-WROOM-1-N16R8", "RF_Module:ESP32-S3-WROOM-1", 330, 130,
                 datasheet="https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf",
                 description="ESP32-S3 module, integrated antenna+crystal, N16R8")
B.custom_syms["New_Library:ESP32-S3-WROOM-1"] = custom_ESP32S3WROOM1()

R_en = R("10k", 268, 78)
C_en = C("1uF", 268, 104)
R_boot = R("10k", 402, 78)
SW_boot = SWPUSH("BOOT", 418, 104)
SW_rst = SWPUSH("RESET", 258, 52)

C_esp_dec = [C("100nF", 286 + i * 18, 205) for i in range(3)]
# 10uF ceramic, not a radial electrolytic: lower ESR for MCU bulk, and
# small enough to sit in the bypass column right beside the 3V3 pin.
C_esp_bulk = C("10uF", 358, 205, footprint="Capacitor_SMD:C_0805_2012Metric")
for _c in C_esp_dec + [C_esp_bulk]:
    B.owner[_c.ref] = (U_esp.ref, "+3V3")

J_usb = B.place("J", B.use_lib("Connector.kicad_sym", "USB_C_Receptacle_USB2.0_14P"), "USB_C_Receptacle_USB2.0_14P",
                 "USB_PROG", "Connector_USB:USB_C_Receptacle_GCT_USB4085", 405, 175)
R_cc1 = R("5.1k", 448, 168)
R_cc2 = R("5.1k", 462, 168)

# ESP32 module net mapping (module pin -> net)
ESP_NET = {
    "1": "GND", "2": "+3V3", "3": "EN_ESP",
    "4": "GPIO4", "5": "GPIO5", "6": "GPIO6", "7": "GPIO7",
    "8": "GPIO15", "9": "GPIO16", "10": "GPIO17", "11": "GPIO18",
    "12": "GPIO8", "13": "GPIO19", "14": "GPIO20",
    "17": "GPIO9", "18": "GPIO10", "19": "GPIO11", "20": "GPIO12",
    "21": "GPIO13", "22": "GPIO14", "23": "GPIO21",
    "27": "GPIO0", "31": "GPIO38", "32": "GPIO39", "33": "GPIO40",
    "35": "GPIO42", "38": "GPIO2", "39": "GPIO1",
    "40": "GND", "41": "GND",
    # NOTE: pin 24 (GPIO47, UART TX) and pin 34 (GPIO41, UART RX) are wired
    # explicitly below onto the shared TMC_UART bus, not auto-mapped here.
}
ESP_NC = ["15", "16", "25", "26", "28", "29", "30", "36", "37"]  # strap/PSRAM/U0 pins, deliberately unused

for pin, net in ESP_NET.items():
    B.net(net, (U_esp, pin))
for pin in ESP_NC:
    B.nc(U_esp, pin)

B.net("EN_ESP", (R_en, 1), (C_en, 1), (SW_rst, 1))
B.net("+3V3", (R_en, 2), (R_boot, 2))
B.net("GND", (C_en, 2), (SW_boot, 2), (SW_rst, 2))
B.net("GPIO0", (R_boot, 1), (SW_boot, 1))
for c in C_esp_dec + [C_esp_bulk]:
    B.net("+3V3", (c, 1))
    B.net("GND", (c, 2))

B.net("GND", (J_usb, "S1"), (J_usb, "A1"))
B.net("USB_VBUS", (J_usb, "A4"))
B.net("USBCC1", (J_usb, "A5"), (R_cc1, 1))
B.net("USBCC2", (J_usb, "B5"), (R_cc2, 1))
B.net("GND", (R_cc1, 2), (R_cc2, 2))
# USB-C carries D+/D- on BOTH sides of the connector so the plug works either
# way up. A7/B7 (D-) and A6/B6 (D+) must be tied together -- wiring only the
# A-side would make the port work in one flip orientation and dead in the other.
B.net("GPIO19", (J_usb, "A7"), (J_usb, "B7"))
B.net("GPIO20", (J_usb, "A6"), (J_usb, "B6"))

# --- DRIVER SECTION (4x TMC2209) --------------------------------------------
DRIVERS = [
    dict(name="Slide", step="GPIO4", dir="GPIO5", ms1="GND", ms2="GND", x=560, y=140),
    dict(name="Pan",   step="GPIO6", dir="GPIO7", ms1="+3V3", ms2="GND", x=740, y=140),
    dict(name="Tilt",  step="GPIO8", dir="GPIO9", ms1="GND", ms2="+3V3", x=560, y=395),
    dict(name="Z",     step="GPIO1", dir="GPIO2", ms1="+3V3", ms2="+3V3", x=740, y=395),
]

R_tmc_tx = R("1k", 405, 258)
B.net("GPIO47", (U_esp, "24"), (R_tmc_tx, 1))
B.net("TMC_UART", (R_tmc_tx, 2), (U_esp, "34"))  # GPIO41 (module pin 34) direct RX tap; pin34 already mapped GPIO41 above via ESP_NET -- see note below

for d in DRIVERS:
    x, y = d["x"], d["y"]
    Utmc = B.place("U", B.use_lib("Driver_Motor.kicad_sym", "TMC2209-LA"), "TMC2209-LA", "TMC2209-LA",
                 "Package_DFN_QFN:VQFN-28-1EP_5x5mm_P0.5mm_EP3.7x3.7mm_ThermalVias",
                 x, y,
                 datasheet="https://www.analog.com/media/en/technical-documentation/data-sheets/TMC2209_datasheet_rev1.09.pdf",
                 description=f"Stepper driver, {d['name']} axis")
    B.net("+24V", (Utmc, "22"), (Utmc, "28"))
    B.net("GND", (Utmc, "3"), (Utmc, "18"), (Utmc, "29"))
    B.net("+3V3", (Utmc, "15"))
    B.net("GND", (Utmc, "13"))  # CLK -> GND (internal osc)
    B.net("DRV_EN", (Utmc, "2"))
    B.net("GND", (Utmc, "7"))  # SPREAD -> GND (stealthChop default)
    for pin in ("20", "17", "11", "12", "25"):
        B.nc(Utmc, pin)
    B.net(d["ms1"], (Utmc, "9"))
    B.net(d["ms2"], (Utmc, "10"))
    B.net("TMC_UART", (Utmc, "14"))
    B.net(d["step"], (Utmc, "16"))
    B.net(d["dir"], (Utmc, "19"))

    cpi_cpo = f"CP_{d['name']}"
    vcp_net = f"VCP_{d['name']}"
    fivev_net = f"5VOUT_{d['name']}"
    rsa_net = f"RSA_{d['name']}"
    rsb_net = f"RSB_{d['name']}"

    c_cpicpo = C("22nF", x + 52, y - 4)
    B.net(cpi_cpo, (Utmc, "5"), (c_cpicpo, 1))
    B.net(cpi_cpo + "_B", (Utmc, "4"), (c_cpicpo, 2))  # CPO isolated node (cap's other leg)

    c_vcpvs = C("100nF", x + 70, y - 40)
    B.owner[c_vcpvs.ref] = (Utmc.ref, "+24V")
    B.net(vcp_net, (Utmc, "6"), (c_vcpvs, 1))
    B.net("+24V", (c_vcpvs, 2))

    c_5vout = C("4.7uF", x + 52, y - 30)
    B.owner[c_5vout.ref] = (Utmc.ref, fivev_net)
    B.net(fivev_net, (Utmc, "8"), (c_5vout, 1))
    B.net("GND", (c_5vout, 2))

    c_vs_hf = C("100nF", x - 11, y - 40)
    c_vs_bulk = CP("100uF_35V", x - 27, y - 40)
    B.owner[c_vs_hf.ref] = (Utmc.ref, "+24V")
    B.owner[c_vs_bulk.ref] = (Utmc.ref, "+24V")
    B.net("+24V", (c_vs_hf, 1), (c_vs_bulk, 1))
    B.net("GND", (c_vs_hf, 2), (c_vs_bulk, 2))

    r_sa = R("0.11", x + 52, y + 17)
    r_sb = R("0.11", x + 66, y + 17)
    B.net(rsa_net, (Utmc, "23"), (r_sa, 1))
    B.net("GND", (r_sa, 2))
    B.net(rsb_net, (Utmc, "27"), (r_sb, 1))
    B.net("GND", (r_sb, 2))

    J_motor = JST_XH4(f"Motor_{d['name']}", x + 82, y - 2)
    B.net(f"MOTOR_{d['name']}_A1", (Utmc, "24"), (J_motor, 1))
    B.net(f"MOTOR_{d['name']}_A2", (Utmc, "21"), (J_motor, 2))
    B.net(f"MOTOR_{d['name']}_B1", (Utmc, "26"), (J_motor, 3))
    B.net(f"MOTOR_{d['name']}_B2", (Utmc, "1"), (J_motor, 4))

# tie shared driver-enable net ("DRV_EN", all 4 TMC EN pins) to GPIO10
# (U_esp pin 18 already added to "GPIO10" by the ESP_NET loop above)
B.nets["GPIO10"].extend(B.nets["DRV_EN"])
del B.nets["DRV_EN"]

# --- SENSOR SECTION (TCA9548A + 2x AS5600) ----------------------------------
U_mux = B.place("U", B.use_lib("Interface_Expansion.kicad_sym", "TCA9548APWR"), "TCA9548APWR", "TCA9548APWR",
                 "Package_SO:TSSOP-24_4.4x7.8mm_P0.65mm", 120, 300,
                 datasheet="http://www.ti.com/lit/ds/symlink/tca9548a.pdf",
                 description="8-channel I2C switch, ch0->pan AS5600, ch1->tilt AS5600")
B.net("+3V3", (U_mux, "24"))
B.net("GND", (U_mux, "12"))
B.net("+3V3", (U_mux, "3"))  # ~RESET tied high (unused reset)
B.net("GND", (U_mux, "1"), (U_mux, "2"), (U_mux, "21"))  # A0/A1/A2 -> addr 0x70
B.net("GPIO12", (U_mux, "22"))  # SCL upstream
B.net("GPIO11", (U_mux, "23"))  # SDA upstream
B.net("I2C_PAN_SCL", (U_mux, "5"))
B.net("I2C_PAN_SDA", (U_mux, "4"))
B.net("I2C_TILT_SCL", (U_mux, "7"))
B.net("I2C_TILT_SDA", (U_mux, "6"))
C_mux_dec = C("100nF", 68, 300)
B.owner[C_mux_dec.ref] = (U_mux.ref, "+3V3")
B.net("+3V3", (C_mux_dec, 1)); B.net("GND", (C_mux_dec, 2))

r_i2c0_sda = R("4.7k", 185, 258); r_i2c0_scl = R("4.7k", 199, 258)
B.net("GPIO11", (r_i2c0_sda, 1)); B.net("+3V3", (r_i2c0_sda, 2))
B.net("GPIO12", (r_i2c0_scl, 1)); B.net("+3V3", (r_i2c0_scl, 2))

r_pan_sda = R("4.7k", 185, 330); r_pan_scl = R("4.7k", 199, 330)
B.net("I2C_PAN_SDA", (r_pan_sda, 1)); B.net("+3V3", (r_pan_sda, 2))
B.net("I2C_PAN_SCL", (r_pan_scl, 1)); B.net("+3V3", (r_pan_scl, 2))

r_tilt_sda = R("4.7k", 185, 366); r_tilt_scl = R("4.7k", 199, 366)
B.net("I2C_TILT_SDA", (r_tilt_sda, 1)); B.net("+3V3", (r_tilt_sda, 2))
B.net("I2C_TILT_SCL", (r_tilt_scl, 1)); B.net("+3V3", (r_tilt_scl, 2))

# The AS5600s do NOT live on this board. Each one needs a <3mm air gap to a
# diametrically-magnetised magnet on its rotating shaft, so they sit on small
# satellite PCBs at the pan and tilt joints (see ../as5600_sensor/) and cable
# back to these two headers. Pinout matches the satellite board's connector.
J_as_pan = JST_XH4("AS5600_Pan", 62, 400)
J_as_tilt = JST_XH4("AS5600_Tilt", 62, 432)
for Jas, sda_net, scl_net in ((J_as_pan, "I2C_PAN_SDA", "I2C_PAN_SCL"),
                               (J_as_tilt, "I2C_TILT_SDA", "I2C_TILT_SCL")):
    B.net("+3V3", (Jas, 1))
    B.net("GND", (Jas, 2))
    B.net(sda_net, (Jas, 3))
    B.net(scl_net, (Jas, 4))

# --- REMAINING I/O CONNECTORS ------------------------------------------------
J_oled = JST_XH4("OLED", 62, 464)
B.net("+3V3", (J_oled, 1)); B.net("GND", (J_oled, 2))
B.net("GPIO13", (J_oled, 3)); B.net("GPIO14", (J_oled, 4))
r_oled_sda = R("4.7k", 185, 402); r_oled_scl = R("4.7k", 199, 402)
B.net("GPIO13", (r_oled_sda, 1)); B.net("+3V3", (r_oled_sda, 2))
B.net("GPIO14", (r_oled_scl, 1)); B.net("+3V3", (r_oled_scl, 2))

J_jog = JST_XH4("Enc_Jog", 62, 496)
B.net("GPIO15", (J_jog, 1)); B.net("GPIO16", (J_jog, 2))
B.net("GPIO17", (J_jog, 3)); B.net("GND", (J_jog, 4))

J_angle = JST_XH4("Enc_Angle", 62, 528)
B.net("GPIO18", (J_angle, 1)); B.net("GPIO21", (J_angle, 2))
B.net("GPIO38", (J_angle, 3)); B.net("GND", (J_angle, 4))

J_lim_min = JST_XH2("Limit_Min", 190, 464)
B.net("GPIO39", (J_lim_min, 1)); B.net("GND", (J_lim_min, 2))
J_lim_max = JST_XH2("Limit_Max", 190, 496)
B.net("GPIO40", (J_lim_max, 1)); B.net("GND", (J_lim_max, 2))
J_lim_z = JST_XH2("Limit_Z_Home", 190, 528)
B.net("GPIO42", (J_lim_z, 1)); B.net("GND", (J_lim_z, 2))

# PWR_FLAG tells ERC that a rail is externally driven. +24V arrives from the
# screw terminal and GND is the return -- neither originates at a power_out
# pin on this board, and the LM2596's OUT is typed "output" rather than
# "power_out", so +5V needs one too.
def PWR_FLAG(x, y):
    lib_id = B.use_lib("power.kicad_sym", "PWR_FLAG")
    return B.place("#FLG", lib_id, "PWR_FLAG", "PWR_FLAG", "", x, y)

for _rail, _fx, _fy in [("+24V", 40, 205), ("+5V", 175, 205), ("GND", 108, 205)]:
    _f = PWR_FLAG(_fx, _fy)
    B.net(_rail, (_f, 1))

# --- BYPASS CAPS WIRED TO THE PIN THEY SERVE --------------------------------
# The rail side of each bypass cap is drawn onto its chip's own supply pin
# instead of onto a shared rail symbol, so the schematic states which pin the
# cap belongs to. The GND side still goes to a GND symbol -- ground is a plane,
# not a routed net.
_by_ref = {c.ref: c for c in B.components}
for _cap_ref, (_ic_ref, _rail) in sorted(B.owner.items()):
    _cap = _by_ref.get(_cap_ref)
    _ic = _by_ref.get(_ic_ref)
    if _cap is None or _ic is None or _rail not in B.nets:
        continue
    _cap_pins = [p for c, p in B.nets[_rail] if c.ref == _cap_ref]
    _ic_pins = [p for c, p in B.nets[_rail] if c.ref == _ic_ref]
    if not _cap_pins or not _ic_pins:
        continue
    # already on the same net -- this only draws it, so pass net=None
    B.wire(_ic, _ic_pins[0], _cap, _cap_pins[0], net=None, suppress="b")

# --- POWER RAILS AS SYMBOLS, NOT LABELS -------------------------------------
# 158 of the 331 net connections on this sheet are power rails (90 of them GND
# alone). Rendering each as a global label buries the actual signal flow in a
# wall of text. Every power-rail pin instead gets the conventional GND / +3V3
# / +5V / +24V symbol at the end of its stub, which is both how KiCad
# schematics are normally drawn and far quicker to read.
POWER_NETS = ("GND", "+3V3", "+5V", "+24V")
_rail_pins = []
for _net in POWER_NETS:
    for _comp, _pin in list(B.nets.get(_net, [])):
        if _comp.ref.startswith("#PWR"):
            continue                      # already a rail symbol
        _rail_pins.append((_net, _comp, _pin))

# fan out rail symbols that share a side of the same part, so their names clear
_by_side = {}
for _net, _comp, _pin in _rail_pins:
    if (_comp.ref, str(_pin)) in B.wired_pins:
        continue                      # fed by a drawn wire, not a rail symbol
    _x, _y, _rot, _ = _comp.pin_abs(_pin)
    _by_side.setdefault((_comp.ref, _rot), []).append((_y, _x, _net, _comp, _pin))
_FAN = [7.62, 13.97, 20.32, 26.67]
for _key, _grp in _by_side.items():
    _grp.sort()
    for _i, (_y, _x, _net, _comp, _pin) in enumerate(_grp):
        _rot = _key[1]
        _dx, _dy = _EXIT_DIR.get(_rot, (-1, 0))
        _len = _FAN[_i % len(_FAN)] if len(_grp) > 1 else STUB_LEN
        B.stub_len[(_comp.ref, str(_pin))] = _len
        _ps = PWR(_net, round(_x + _dx * _len, 4), round(_y + _dy * _len, 4))
        B.nets[_net].append((_ps, "1"))

# auto-extraction (see PINTABLES override above) surfaces every real pin on
# library symbols like USB-C and TCA9548A, including ones this design never
# uses (redundant power/GND duplicates, unused mux channels). Any such pin
# with neither a net nor an explicit no_connect gets one now, so nothing is
# silently left dangling.
_wired_pins = set()
_wired_xy = set()
for _net, _members in B.nets.items():
    for _comp, _pin in _members:
        _wired_pins.add((_comp.ref, _pin))
        _x, _y, _r, _n = _comp.pin_abs(_pin)
        _wired_xy.add((_comp.ref, _x, _y))
_auto_nc = []
for _c in B.components:
    for _pin in PINTABLES[_c.pinkey].keys():
        if (_c.ref, _pin) in _wired_pins:
            continue
        _x, _y, _r, _n = _c.pin_abs(_pin)
        # Symbols like USB_C_Receptacle stack their redundant VBUS/GND/D+/D-
        # pins at ONE coordinate. If a sibling pin at this exact point is
        # already wired, this pin is that same electrical node -- flagging it
        # no-connect would contradict a real connection.
        if (_c.ref, _x, _y) in _wired_xy:
            continue
        B.nc(_c, _pin)
        _auto_nc.append(f"{_c.ref}.{_pin}({PINTABLES[_c.pinkey][_pin][0]})")
print(f"auto no-connect added for {len(_auto_nc)} unused pins:", ", ".join(_auto_nc[:40]),
      "..." if len(_auto_nc) > 40 else "")

SECTIONS.extend([
    ("POWER  24V IN / 5V BUCK / 3V3 LDO", 40, 50),
    ("MCU  ESP32-S3-WROOM-1 + USB-C", 255, 40),
    ("I2C MUX + OFF-BOARD I/O CONNECTORS", 40, 240),
    ("STEPPER DRIVERS  4x TMC2209", 500, 40),
])

print("circuit built:", len(B.components), "components,", len(B.nets), "nets")

# ===========================================================================
# EMISSION
# ===========================================================================

def rename_header(block, full_name):
    # block starts with: (symbol "OldName"\n ...
    m = re.match(r'\(symbol\s+"((?:[^"\\]|\\.)*)"', block)
    old = m.group(1)
    return '(symbol "' + esc(full_name) + '"' + block[m.end():]

def find_extends(block):
    m = re.search(r'\(extends\s+"((?:[^"\\]|\\.)*)"\)', block)
    return m.group(1) if m else None

def split_top_children(block):
    """Split the direct children S-expressions of a (symbol "X" ...) block body."""
    # skip past `(symbol "X"`
    m = re.match(r'\(symbol\s+"(?:[^"\\]|\\.)*"', block)
    i = m.end()
    n = len(block)
    children = []
    while i < n:
        while i < n and block[i] not in '(':
            if block[i] == ')':
                return children
            i += 1
        if i >= n:
            break
        start = i
        end = find_matching_paren(block, start)
        children.append(block[start:end + 1])
        i = end + 1
    return children

def flatten_extends(raw, base_raw, full_name, bare_name):
    """Merge a derived (extends-based) symbol with its base into one
    self-contained embedded symbol -- KiCad schematics do not support
    `extends` indirection in an embedded lib_symbols table, only real
    library files do, so derived parts must be flattened before embedding.
    """
    derived_children = split_top_children(raw)
    base_children = split_top_children(base_raw)
    base_name_m = re.match(r'\(symbol\s+"((?:[^"\\]|\\.)*)"', base_raw)
    base_name = base_name_m.group(1)

    derived_props = {}
    for ch in derived_children:
        pm = re.match(r'\(property\s+"([^"]+)"', ch)
        if pm:
            derived_props[pm.group(1)] = ch

    header_flags, properties, units, trailing = [], [], [], []
    base_props_order = []
    for ch in base_children:
        pm = re.match(r'\(property\s+"([^"]+)"', ch)
        if pm:
            base_props_order.append(pm.group(1))
        elif ch.startswith('(symbol "'):
            units.append(ch.replace(base_name + "_", bare_name + "_"))
        elif ch.startswith('(embedded_fonts'):
            trailing.append(ch)
        else:
            header_flags.append(ch)  # pin_numbers/pin_names/exclude_from_sim/in_bom/on_board/etc.

    for name in base_props_order:
        properties.append(derived_props.get(name) or [c for c in base_children
                           if c.startswith(f'(property "{name}"')][0])
    for name, ch in derived_props.items():
        if name not in base_props_order:
            properties.append(ch)

    merged = header_flags + properties + units + trailing
    body = "\n".join(merged)
    return f'(symbol "{esc(full_name)}"\n{body}\n)'

def build_lib_symbols():
    out = []
    seen_full = set()
    for libfile, symname in sorted(B.lib_needed):
        full_name = f"{libfile[:-len('.kicad_sym')]}:{symname}"
        if full_name in seen_full:
            continue
        seen_full.add(full_name)
        raw = extract_symbol(libfile, symname)
        base = find_extends(raw)
        if base:
            base_raw = extract_symbol(libfile, base)
            flat = flatten_extends(raw, base_raw, full_name, symname)
            out.append(reindent(flat))
        else:
            renamed = rename_header(raw, full_name)
            out.append(reindent(renamed))
    for lib_id, text in B.custom_syms.items():
        out.append(text)
    return "\n".join(out)

def label_rot_justify(rot):
    """(label_rot, justify) -- ALWAYS horizontal text.

    Label rotations 90/270 render the text vertically, which is unreadable
    without turning the sheet sideways. Every resistor and capacitor has its
    pins pointing up/down, so the naive "rotate the label to match the pin"
    rule turned every passive's net name on its side. Labels are kept
    horizontal regardless of which way the pin points:

      pin exits left / up / down -> text ends AT the wire (right-justified)
      pin exits right           -> text starts AT the wire (left-justified)

    so labels form a flush column down each side of a symbol.
    """
    if rot == 180:    # pin exits right -> text runs rightwards, away from body
        return 0, ' (justify left)'
    return 180, ' (justify right)'


# Pins pointing up/down get a longer stub so their horizontal label clears the
# neighbouring part instead of colliding with it.
VERT_STUB_LEN = 5.08

def build_component_instances(project_name, sheet_uuid):
    out = []
    for c in B.components:
        props = []
        is_rail = c.ref.startswith("#PWR") or c.ref.startswith("#FLG")
        # A rail symbol's VALUE is its name (GND / +24V / +3V3) -- it is the
        # only thing identifying which rail the symbol represents, so it must
        # stay visible. Only the meaningless auto reference (#PWR07) is hidden.
        hide_ref = is_rail
        hide_val = False
        # Put Reference above the topmost pin and Value below the bottommost,
        # so text clears the symbol body instead of landing inside tall parts
        # like the TMC2209 (body spans ~48mm) or the ESP32 module.
        _ys = [c.pin_abs(p)[1] for p in PINTABLES[c.pinkey]]
        ref_y = round(min(_ys) - 2.54, 4)
        val_y = round(max(_ys) + 2.54, 4)
        if is_rail:
            # rail name sits clear of the glyph: below a GND stub, above a
            # supply arrow, matching which way the symbol points
            up = PINTABLES[c.pinkey]["1"][3] == 270
            val_y = round(c.y + (3.6 if up else -3.6), 4)
        props.append(f'(property "Reference" "{esc(c.ref)}" (at {c.x} {ref_y} 0) '
                     f'(show_name no) (effects (font (size 1.27 1.27))'
                     + (' (hide yes))' if hide_ref else ')') + ')')
        # rail names are set smaller than signal labels: they repeat constantly
        # and should read as subordinate annotation, not compete with net names
        _vs = '1 1' if is_rail else '1.27 1.27'
        props.append(f'(property "Value" "{esc(c.value)}" (at {c.x} {val_y} 0) '
                     f'(show_name no) (effects (font (size {_vs}))'
                     + (' (hide yes))' if hide_val else ')') + ')')
        if c.footprint:
            props.append(f'(property "Footprint" "{esc(c.footprint)}" (at {c.x} {c.y} 0) '
                         f'(show_name no) (effects (font (size 1.27 1.27)) (hide yes)))')
        if c.datasheet:
            props.append(f'(property "Datasheet" "{esc(c.datasheet)}" (at {c.x} {c.y} 0) '
                         f'(effects (font (size 1.27 1.27)) (hide yes)))')
        _own = B.owner.get(c.ref)
        if _own:
            # hidden on the sheet, but visible in the symbol/footprint fields
            # and in the BOM -- this is the only record of which pin the cap
            # is bypassing, since its net names cannot say.
            props.append(f'(property "Decouples" "{esc(_own[0])} {esc(_own[1])}" '
                         f'(at {c.x} {c.y} 0) '
                         f'(effects (font (size 1.27 1.27)) (hide yes)))')
        if c.description:
            props.append(f'(property "Description" "{esc(c.description)}" (at {c.x} {c.y} 0) '
                         f'(effects (font (size 1.27 1.27)) (hide yes)))')
        props_txt = "\n\t\t".join(props)

        pins = PINTABLES[c.pinkey]
        pin_txt = "\n".join(
            f'\t\t(pin "{num}" (uuid "{U()}"))' for num in pins.keys()
        )

        instances = (
            f'\t\t(instances\n'
            f'\t\t\t(project "{esc(project_name)}"\n'
            f'\t\t\t\t(path "/{sheet_uuid}"\n'
            f'\t\t\t\t\t(reference "{esc(c.ref)}")\n'
            f'\t\t\t\t\t(unit 1)\n'
            f'\t\t\t\t)\n'
            f'\t\t\t)\n'
            f'\t\t)'
        )

        block = (
            f'\t(symbol\n'
            f'\t\t(lib_id "{esc(c.lib_id)}")\n'
            f'\t\t(at {c.x} {c.y} 0)\n'
            + ('\t\t(mirror y)\n' if c.mirror else '')
            + f'\t\t(unit 1)\n'
            f'\t\t(exclude_from_sim no) (in_bom yes) (on_board yes) (dnp {"yes" if c.dnp else "no"})\n'
            f'\t\t(uuid "{c.uuid}")\n'
            f'\t\t{props_txt}\n'
            f'{pin_txt}\n'
            f'{instances}\n'
            f'\t)'
        )
        out.append(block)
    return "\n".join(out)


def build_labels():
    """Every net member gets a real wire stub from the pin out to whatever
    names its net, rather than a label sitting on the pin (label-on-pin with
    no wire is not a connection mechanism in KiCad).

    Power rails are named by a GND / +3V3 / +5V / +24V SYMBOL sitting at the
    end of the stub instead of a text label, so only genuine signals carry
    label text.
    """
    out = []
    for net_name, members in B.nets.items():
        is_power = net_name in POWER_NETS
        for comp, pin in members:
            if comp.ref.startswith("#PWR"):
                continue          # the rail symbol itself -- no stub, no label
            if (comp.ref, pin) in B.wired_pins:
                continue          # already joined by a drawn wire
            x, y, rot, pname = comp.pin_abs(pin)
            dx, dy = _EXIT_DIR.get(rot, (-1, 0))
            stub = B.stub_len.get((comp.ref, pin))
            if stub is None:
                stub = VERT_STUB_LEN if (dy and not is_power) else STUB_LEN
            ex = round(x + dx * stub, 4)
            ey = round(y + dy * stub, 4)
            lrot, justify = label_rot_justify(rot)
            out.append(
                f'\t(wire\n'
                f'\t\t(pts (xy {x} {y}) (xy {ex} {ey}))\n'
                f'\t\t(stroke (width 0) (type default))\n'
                f'\t\t(uuid "{U()}")\n'
                f'\t)'
            )
            if is_power:
                continue          # named by the rail symbol at (ex, ey)
            out.append(
                f'\t(global_label "{esc(net_name)}"\n'
                f'\t\t(shape input)\n'
                f'\t\t(at {ex} {ey} {lrot})\n'
                f'\t\t(effects (font (size 1.27 1.27)){justify})\n'
                f'\t\t(uuid "{U()}")\n'
                f'\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}"\n'
                f'\t\t\t(at {ex} {ey} 0)\n'
                f'\t\t\t(hide yes)\n'
                f'\t\t\t(show_name no)\n'
                f'\t\t\t(do_not_autoplace no)\n'
                f'\t\t\t(effects (font (size 1.27 1.27)))\n'
                f'\t\t)\n'
                f'\t)'
            )
    return "\n".join(out)


def build_section_text():
    out = []
    for label, x, y in SECTIONS:
        out.append(
            f'\t(text "{esc(label)}"\n'
            f'\t\t(at {x} {y} 0)\n'
            f'\t\t(effects (font (size 3.5 3.5) (bold yes)) (justify left bottom))\n'
            f'\t\t(uuid "{U()}")\n'
            f'\t)'
        )
    return "\n".join(out)

def build_direct_wires():
    """Orthogonal wires for explicitly-wired pin pairs, plus junction dots
    wherever three or more wire ends meet at one point."""
    out = []
    ends = {}
    labelled = set()
    for a, pa, b, pb, netname in B.direct:
        ax, ay, _, _ = a.pin_abs(pa)
        bx, by, _, _ = b.pin_abs(pb)
        if abs(ax - bx) < 0.001 or abs(ay - by) < 0.001:
            pts = [(ax, ay), (bx, by)]               # straight run
        else:
            pts = [(ax, ay), (bx, ay), (bx, by)]     # L-shaped dogleg
        for i in range(len(pts) - 1):
            x1, y1 = pts[i]
            x2, y2 = pts[i + 1]
            out.append(
                "\t(wire\n"
                "\t\t(pts (xy {} {}) (xy {} {}))\n".format(x1, y1, x2, y2) +
                "\t\t(stroke (width 0) (type default))\n"
                "\t\t(uuid \"{}\")\n".format(U()) +
                "\t)"
            )
        if netname and netname not in labelled and netname not in POWER_NETS:
            labelled.add(netname)
            mx = round((pts[0][0] + pts[1][0]) / 2.0, 4)
            my = round((pts[0][1] + pts[1][1]) / 2.0, 4)
            out.append(
                '\t(label "{}"\n'.format(esc(netname)) +
                '\t\t(at {} {} 0)\n'.format(mx, my) +
                '\t\t(effects (font (size 1.27 1.27)) (justify left bottom))\n' +
                '\t\t(uuid "{}")\n'.format(U()) +
                '\t)'
            )
        for _r, _p, pt in ((a.ref, pa, (ax, ay)), (b.ref, pb, (bx, by))):
            ends[pt] = ends.get(pt, 0) + 1
            if (_r, _p) not in B.wired_pins:
                ends[pt] = ends.get(pt, 0) + 1   # also has its own rail stub
    for (x, y), n in ends.items():
        if n >= 3:
            out.append(
                "\t(junction (at {} {}) (diameter 0) (color 0 0 0 0) "
                "(uuid \"{}\"))".format(x, y, U())
            )
    return "\n".join(out)



def build_no_connects():
    out = []
    for x, y in B.no_connects:
        out.append(f'\t(no_connect (at {x} {y}) (uuid "{U()}"))')
    return "\n".join(out)


def main(project_name="pantiltslide_integrated", title=None, out_dir=None):
    title = title or "PanTilt Slide -- Integrated Controller (PCBWay assembly)"
    sheet_uuid = U()
    lib_symbols_txt = build_lib_symbols()
    components_txt = build_component_instances(project_name, sheet_uuid)
    labels_txt = build_labels()
    nc_txt = build_no_connects()
    dw_txt = build_direct_wires()
    text_txt = build_section_text()

    doc = f'''(kicad_sch
\t(version 20250114)
\t(generator "eeschema")
\t(generator_version "9.0")
\t(uuid "{sheet_uuid}")
\t(paper "A1")
\t(title_block
\t\t(title "{esc(title)}")
\t\t(company "")
\t\t(rev "A")
\t)
\t(lib_symbols
{lib_symbols_txt}
\t)
{components_txt}
{labels_txt}
{dw_txt}
{nc_txt}
{text_txt}
\t(sheet_instances
\t\t(path "/"
\t\t\t(page "1")
\t\t)
\t)
\t(embedded_fonts no)
)
'''
    out_dir = out_dir or os.path.dirname(__file__)
    out_path = os.path.join(out_dir, f"{project_name}.kicad_sch")
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(doc)

    # Ship the two hand-authored symbols as a real, editable library (plus the
    # table entry that registers it) so the custom parts open in the Symbol
    # Editor instead of only existing as an embedded copy in the schematic.
    # Emit EVERY hand-authored symbol, not just the ones this board happens to
    # place. The main board and the AS5600 satellite share one New_Library
    # file; building it from B.custom_syms meant whichever board generated
    # last silently dropped the other board's symbol.
    all_custom = {"New_Library:AS5600": custom_AS5600(),
                  "New_Library:ESP32-S3-WROOM-1": custom_ESP32S3WROOM1()}
    lib_syms = []
    for _lib_id, _text in all_custom.items():
        bare = _lib_id.split(":", 1)[1]
        lib_syms.append(rename_header(_text.strip(), bare).replace(
            f'"{_lib_id}_', f'"{bare}_'))
    lib_doc = ('(kicad_symbol_lib\n'
               '\t(version 20241209)\n'
               '\t(generator "kicad_symbol_editor")\n'
               '\t(generator_version "9.0")\n'
               + "\n".join(lib_syms) + '\n)\n')
    with open(os.path.join(out_dir, "New_Library.kicad_sym"), "w",
              encoding="utf-8", newline="\n") as f:
        f.write(lib_doc)

    with open(os.path.join(out_dir, "sym-lib-table"), "w",
              encoding="utf-8", newline="\n") as f:
        f.write('(sym_lib_table\n\t(version 7)\n'
                '\t(lib (name "New_Library")(type "KiCad")'
                '(uri "${KIPRJMOD}/New_Library.kicad_sym")(options "")(descr ""))\n)\n')
    print("wrote", out_path, len(doc), "bytes")
    return out_path

if __name__ == "__main__":
    main()
