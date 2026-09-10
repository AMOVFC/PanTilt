"""Generate prototype_wired.kicad_sch: a fully point-to-point wired schematic
for the 3-axis prototype (slide/pan/tilt), with NO global labels and NO local
labels anywhere -- every connection is a drawn `wire` from pin to pin.

Coordinate convention reused from ../../pantiltslide/tools/gen_wiring.py,
validated there against a real KiCad-authored file: library symbols are
Y-up, the .kicad_sch canvas is Y-down, so a pin's local (lx, ly) maps to
sheet position (cx + lx, cy - ly) for a symbol placed at (cx, cy).

Real chip symbols (pin names/numbers/positions, transcribed from the actual
installed KiCad 9.0 libraries):
  - TMC2209-LA from Driver_Motor.kicad_sym (full 28-pin QFN pinout)
  - TCA9548A (TCA9548AMRGER) from Interface_Expansion.kicad_sym (channels
    0-2 kept, 3-7 omitted -- unused in this design, noted in Description)
  - EncoderSwitch / LimitSwitch from Device.kicad_sym's RotaryEncoder_Switch
    and Switch.kicad_sym's SW_Push (kept complete)
Everything else (ESP32-S3 DevKit header pins, AS5600 breakout, OLED
breakout, NEMA17 motor connector, power-input terminals) has no accurate
real symbol available offline, so those are generic multi-pin connectors
with correct pin NAMEs -- a pin name on a connector is normal schematic
practice, not the disconnected global/local label style this avoids.

ALL pins, on every part, are addressed by NUMBER (never by name) to avoid
any ambiguity between the two.
"""
import uuid

OUT = []
LIB_SYMBOLS = []
PLACED = {}       # ref -> {"pins": {number(str): (sheet_x, sheet_y)}}
JOINTS = set()


def U():
    return str(uuid.uuid4())


def emit(s):
    OUT.append(s)


def sheet_pos(cx, cy, lx, ly):
    return (round(cx + lx, 3), round(cy - ly, 3))


# ---------------------------------------------------------------------------
# lib_symbols
# ---------------------------------------------------------------------------

def pin_block(etype, name, number, x, y, angle):
    return (
        '\t\t\t(pin %s line\n\t\t\t\t(at %s %s %s)\n\t\t\t\t(length 2.54)\n'
        '\t\t\t\t(name "%s" (effects (font (size 1.27 1.27))))\n'
        '\t\t\t\t(number "%s" (effects (font (size 1.27 1.27))))\n\t\t\t)\n'
    ) % (etype, x, y, angle, name, number)


def make_symbol(lib_name, ref_prefix, x0, y0, x1, y1, pins, footprint, datasheet, description):
    """pins: list of (etype, name, number, lx, ly, angle). Registers the
    lib_symbol AND returns the local pin geometry dict {number: (lx,ly,angle)}."""
    body = []
    body.append('\t(symbol "%s"\n\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n'
                 % lib_name)
    body.append('\t\t(property "Reference" "%s"\n\t\t\t(at %s %s 0)\n'
                 '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (ref_prefix, x0, y1 + 2.54))
    body.append('\t\t(property "Value" "%s"\n\t\t\t(at %s %s 0)\n'
                 '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (lib_name, x0, y1 + 5.08))
    body.append('\t\t(property "Footprint" "%s"\n\t\t\t(at 0 0 0)\n'
                 '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n' % footprint)
    body.append('\t\t(property "Datasheet" "%s"\n\t\t\t(at 0 0 0)\n'
                 '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n' % datasheet)
    body.append('\t\t(property "Description" "%s"\n\t\t\t(at 0 0 0)\n'
                 '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n' % description)
    body.append('\t\t(symbol "%s_1_1"\n' % lib_name)
    body.append('\t\t\t(rectangle\n\t\t\t\t(start %s %s)\n\t\t\t\t(end %s %s)\n'
                 '\t\t\t\t(stroke (width 0.254) (type default))\n'
                 '\t\t\t\t(fill (type background))\n\t\t\t)\n' % (x0, y0, x1, y1))
    geom = {}
    for etype, name, number, lx, ly, angle in pins:
        body.append(pin_block(etype, name, number, lx, ly, angle))
        geom[str(number)] = (lx, ly, angle)
    body.append('\t\t)\n\t\t(embedded_fonts no)\n\t)\n')
    LIB_SYMBOLS.append(''.join(body))
    return geom


SYMBOL_GEOM = {}  # lib_name -> {number: (lx,ly,angle)}


def define(lib_name, *args, **kwargs):
    SYMBOL_GEOM[lib_name] = make_symbol(lib_name, *args, **kwargs)


TMC_PINS = [
    ("input", "STEP", "16", -15.24, 20.32, 0),
    ("input", "DIR", "19", -15.24, 17.78, 0),
    ("input", "CLK", "13", -15.24, 12.7, 0),
    ("bidirectional", "~{PD}/UART", "14", -15.24, 7.62, 0),
    ("input", "MS1/AD0", "9", -15.24, 2.54, 0),
    ("input", "MS2/AD1", "10", -15.24, 0, 0),
    ("input", "SPREAD", "7", -15.24, -2.54, 0),
    ("input", "~{EN}", "2", -15.24, -7.62, 0),
    ("input", "STDBY", "20", -15.24, -10.16, 0),
    ("output", "DIAG", "11", -15.24, -15.24, 0),
    ("output", "INDEX", "12", -15.24, -17.78, 0),
    ("power_in", "VCC_IO", "15", -2.54, 27.94, 270),
    ("power_in", "VS", "22", 0, 27.94, 270),
    ("power_in", "GND", "3", 0, -25.4, 90),
    ("passive", "NC", "25", 2.54, -25.4, 90),
    ("output", "VCP", "6", 5.08, 27.94, 270),
    ("power_out", "5VOUT", "8", 15.24, 20.32, 180),
    ("passive", "VREF", "17", 15.24, 15.24, 180),
    ("input", "CPO", "4", 15.24, 10.16, 180),
    ("input", "CPI", "5", 15.24, 5.08, 180),
    ("output", "OA1", "24", 15.24, 0, 180),
    ("output", "OA2", "21", 15.24, -2.54, 180),
    ("output", "OB1", "26", 15.24, -7.62, 180),
    ("output", "OB2", "1", 15.24, -10.16, 180),
    ("input", "BRA", "23", 15.24, -15.24, 180),
    ("input", "BRB", "27", 15.24, -17.78, 180),
]
define("TMC2209-LA", "U", -12.7, 25.4, 12.7, -22.86, TMC_PINS,
       "Package_DFN_QFN:QFN-28-1EP_5x5mm_P0.5mm_EP3.4x3.4mm",
       "https://www.analog.com/TMC2209",
       "TMC2209 stepper driver chip, real datasheet pinout. Physical breakout board "
       "exposes PDN_UART (pin 14) twice as separate TX/RX pads with an on-board "
       "resistor between them -- electrically one node, drawn here as one pin.")

TCA_PINS = [
    ("input", "SCL", "19", -10.16, 17.78, 0),
    ("bidirectional", "SDA", "20", -10.16, 15.24, 0),
    ("input", "~{RESET}", "24", -10.16, 5.08, 0),
    ("input", "A2", "18", -10.16, -7.62, 0),
    ("input", "A1", "23", -10.16, -10.16, 0),
    ("input", "A0", "22", -10.16, -12.7, 0),
    ("power_in", "VCC", "21", 0, 22.86, 270),
    ("power_in", "GND", "9", 0, -25.4, 90),
    ("output", "SC0", "2", 10.16, 17.78, 180),
    ("bidirectional", "SD0", "1", 10.16, 15.24, 180),
    ("output", "SC1", "4", 10.16, 12.7, 180),
    ("bidirectional", "SD1", "3", 10.16, 10.16, 180),
    ("output", "SC2", "6", 10.16, 7.62, 180),
    ("bidirectional", "SD2", "5", 10.16, 5.08, 180),
]
define("TCA9548A", "U", -8.89, 20.32, 8.89, -22.86, TCA_PINS,
       "Package_DFN_QFN:VQFN-24-1EP_4x4mm_P0.5mm_EP2.7x2.7mm",
       "https://www.ti.com/lit/ds/symlink/tca9548a.pdf",
       "TCA9548A 8-ch I2C mux (channels 3-7 omitted, unused in this design)")

ENC_PINS = [  # 1=A 2=C 3=B 4=S1 5=S2  (numbered for easy reference in wiring)
    ("passive", "A", "1", -7.62, 2.54, 0),
    ("passive", "C", "2", -7.62, 0, 0),
    ("passive", "B", "3", -7.62, -2.54, 0),
    ("passive", "S1", "4", 7.62, 2.54, 180),
    ("passive", "S2", "5", 7.62, -2.54, 180),
]
define("EncoderSwitch", "SW", -5.08, 5.08, 5.08, -5.08, ENC_PINS, "", "",
       "Rotary encoder w/ integrated push button. Pin1=A Pin2=C(common) Pin3=B Pin4=S1 Pin5=S2")

SW_PINS = [
    ("passive", "1", "1", -5.08, 0, 0),
    ("passive", "2", "2", 5.08, 0, 180),
]
define("LimitSwitch", "SW", -2.54, 2.54, 2.54, -2.54, SW_PINS, "", "",
       "Mechanical limit switch, SPST-NO")


def define_connector(lib_name, n, side):
    """Generic N-pin header, unnamed pins (numbered 1..n only) -- the wiring
    comments in the placement section say what each number is."""
    top_y = (n - 1) * 2.54 / 2
    pins = []
    for i in range(n):
        ly = round(top_y - i * 2.54, 3)
        if side == "right":
            pins.append(("passive", str(i + 1), str(i + 1), 5.08, ly, 180))
        else:
            pins.append(("passive", str(i + 1), str(i + 1), -5.08, ly, 0))
    define(lib_name, "J", -2.54, top_y + 2.54, 2.54, -top_y - 2.54, pins,
           "Connector_Generic:Conn_01x%02d" % n, "", "Generic %d-pin header" % n)


define_connector("Conn2L", 2, "left")
define_connector("Conn4L", 4, "left")
define_connector("Conn11R", 11, "right")
define_connector("Conn13L", 13, "left")


# ---------------------------------------------------------------------------
# Placement + pin lookup
# ---------------------------------------------------------------------------

def place(ref, lib_name, x, y):
    # Every symbol origin must itself land on the 1.27mm grid, or its pins
    # (origin + on-grid local offset) end up off-grid too, and an off-grid
    # pin can get silently dropped from ERC connectivity even when a wire
    # is drawn exactly to its (off-grid) coordinate.
    x, y = snap(x), snap(y)
    geom = SYMBOL_GEOM[lib_name]
    emit('\t(symbol\n\t\t(lib_id "%s")\n\t\t(at %s %s 0)\n\t\t(unit 1)\n'
         '\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n'
         '\t\t(dnp no)\n\t\t(fields_autoplaced yes)\n\t\t(uuid "%s")\n' % (lib_name, x, y, U()))
    emit('\t\t(property "Reference" "%s"\n\t\t\t(at %s %s 0)\n'
         '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (ref, x, y - 12))
    emit('\t\t(property "Value" "%s"\n\t\t\t(at %s %s 0)\n'
         '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (lib_name, x, y - 9.4))
    for number in geom:
        emit('\t\t(pin "%s"\n\t\t\t(uuid "%s")\n\t\t)\n' % (number, U()))
    emit('\t\t(instances\n\t\t\t(project "prototype_wired"\n\t\t\t\t(path "/"\n'
         '\t\t\t\t\t(reference "%s")\n\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n' % ref)
    emit('\t)\n')
    pins = {}
    for number, (lx, ly, angle) in geom.items():
        pins[number] = sheet_pos(x, y, lx, ly)
    PLACED[ref] = pins


def pin_xy(ref, number):
    return PLACED[ref][str(number)]


GRID = 1.27


def snap(v):
    """Every wire/junction coordinate must land on KiCad's schematic grid
    (1.27mm) or the connectivity engine can silently drop the segment from
    the netlist -- even at the end that DOES touch a real pin. All real
    symbol pins in this file are already exact multiples of 1.27 by
    construction (transcribed from the library / built on a 2.54mm pitch),
    so snapping here never moves a genuine pin coordinate."""
    return round(round(v / GRID) * GRID, 3)


def wire(points):
    for i in range(len(points) - 1):
        x1, y1 = snap(points[i][0]), snap(points[i][1])
        x2, y2 = snap(points[i + 1][0]), snap(points[i + 1][1])
        if (x1, y1) == (x2, y2):
            continue
        emit('\t(wire\n\t\t(pts (xy %s %s) (xy %s %s))\n'
             '\t\t(stroke (width 0) (type default))\n\t\t(uuid "%s")\n\t)\n' % (x1, y1, x2, y2, U()))


def connect(a, b):
    """Auto-routed 2-segment orthogonal wire: exit `a` horizontally, then
    turn vertically into `b`."""
    ax, ay = pin_xy(*a)
    bx, by = pin_xy(*b)
    if ax == bx or ay == by:
        wire([(ax, ay), (bx, by)])
        return
    via = (bx, ay)
    wire([(ax, ay), via, (bx, by)])


RAIL_TAPS = {}  # rail_y (snapped) -> set of tap x (snapped)


def tap(ref, number, rail_y):
    px, py = pin_xy(ref, number)
    wire([(px, py), (px, rail_y)])
    ry = snap(rail_y)
    JOINTS.add((snap(px), ry))
    RAIL_TAPS.setdefault(ry, set()).add(snap(px))


def draw_rail(rail_y, x0, x1):
    """A rail MUST be emitted as separate wire segments between consecutive
    tap points -- KiCad's connectivity engine does not treat a wire endpoint
    landing in the middle of ANOTHER, uninterrupted wire as connected to it,
    even with a junction dot present. (Confirmed empirically: splitting a
    rail into segments at each tap turned a "pin not connected" ERC error
    into a clean connection, with no other change.) Call this AFTER every
    tap() for this rail_y has already run, so RAIL_TAPS is complete."""
    ry = snap(rail_y)
    xs = sorted(RAIL_TAPS.get(ry, set()) | {snap(x0), snap(x1)})
    for i in range(len(xs) - 1):
        wire([(xs[i], ry), (xs[i + 1], ry)])


def no_connect(ref, number):
    px, py = pin_xy(ref, number)
    emit('\t(no_connect\n\t\t(at %s %s)\n\t\t(uuid "%s")\n\t)\n' % (px, py, U()))


def note(x, y, text, size=1.27):
    emit('\t(text "%s"\n\t\t(at %s %s 0)\n\t\t(effects (font (size %s %s)))\n\t\t(uuid "%s")\n\t)\n'
         % (text, x, y, size, size, U()))


def junction_blocks():
    s = []
    for x, y in sorted(JOINTS):
        s.append('\t(junction\n\t\t(at %s %s)\n\t\t(diameter 0)\n'
                 '\t\t(color 0 0 0 0)\n\t\t(uuid "%s")\n\t)\n' % (x, y, U()))
    return ''.join(s)


# ===========================================================================
# PLACEMENT (mm, A2 sheet 594x420)
# ===========================================================================
place("U1", "TMC2209-LA", 260, 60)    # Slide driver
place("U2", "TMC2209-LA", 260, 190)   # Pan driver
place("U3", "TMC2209-LA", 260, 320)   # Tilt driver

place("J1", "Conn4L", 400, 60)    # Motor Slide: 1=A+ 2=A- 3=B+ 4=B-
place("J2", "Conn4L", 400, 190)   # Motor Pan
place("J3", "Conn4L", 400, 320)   # Motor Tilt

place("J4", "EncoderSwitch", 480, 40)    # Slide encoder: 1=A 2=C 3=B 4=S1 5=S2
place("J5", "EncoderSwitch", 480, 170)   # Pan encoder
place("J6", "EncoderSwitch", 480, 300)   # Tilt encoder

place("U4", "TCA9548A", 480, 400)  # I2C mux

place("J7", "Conn4L", 560, 190)   # AS5600 pan: 1=VCC 2=GND 3=SDA 4=SCL
place("J8", "Conn4L", 560, 320)   # AS5600 tilt

place("J9", "LimitSwitch", 560, 40)    # Limit min: 1=signal 2=GND
place("J10", "LimitSwitch", 560, 70)   # Limit max

place("J11", "Conn4L", 560, 440)  # OLED (optional): 1=VCC 2=GND 3=SDA 4=SCL

# ESP32-S3 DevKitC-1, only the pins used. Numbered 1..N; see the comment
# lists below for what each number is (matches the written wiring diagram's
# "right pins" / "left pins" split 1:1).
place("ESP1", "Conn11R", 120, 190)
# ESP1: 1=GPIO4(SlideSTEP) 2=GPIO5(SlideDIR) 3=GPIO6(PanSTEP) 4=GPIO7(PanDIR)
#       5=GPIO8(TiltSTEP) 6=GPIO9(TiltDIR) 7=GPIO10(EN) 8=GPIO41(UART_RX)
#       9=GPIO47(UART_TX) 10=GPIO11(SDA_MUX) 11=GPIO12(SCL_MUX)

place("ESP2", "Conn13L", 40, 190)
# ESP2: 1=GPIO13(SDA_OLED) 2=GPIO14(SCL_OLED) 3=GPIO15(SlideA) 4=GPIO16(SlideB)
#       5=GPIO17(SlidePush) 6=GPIO18(PanA) 7=GPIO21(PanB) 8=GPIO38(PanPush)
#       9=GPIO1(TiltA) 10=GPIO2(TiltB) 11=GPIO42(TiltPush) 12=GPIO39(LimitMin)
#       13=GPIO40(LimitMax)

place("ESP3", "Conn2L", 80, 400)  # 1=3V3 2=GND  (devkit's own power pins used)
place("PWR1", "Conn2L", 20, 400)  # 1=VM(12-24V in) 2=GND

# ===========================================================================
# NETS
# ===========================================================================

# STEP/DIR
connect(("ESP1", 1), ("U1", 16))
connect(("ESP1", 2), ("U1", 19))
connect(("ESP1", 3), ("U2", 16))
connect(("ESP1", 4), ("U2", 19))
connect(("ESP1", 5), ("U3", 16))
connect(("ESP1", 6), ("U3", 19))

def draw_vtrunk(trunk_x, ys):
    """Same reasoning as draw_rail() but for a vertical trunk: must be
    emitted as segments between consecutive tap y's, not one wire threading
    past mid-span T-junctions."""
    ys = sorted(set(snap(y) for y in ys))
    for i in range(len(ys) - 1):
        wire([(trunk_x, ys[i]), (trunk_x, ys[i + 1])])
    for y in ys[1:-1]:
        JOINTS.add((snap(trunk_x), y))


# EN (shared): fan out from ESP1 pin7 to all 3 drivers' ~{EN} (pin 2)
en_x, en_y = pin_xy("ESP1", 7)
en_trunk_x = en_x + 40
wire([(en_x, en_y), (en_trunk_x, en_y)])
JOINTS.add((snap(en_trunk_x), snap(en_y)))
en_ys = [en_y]
for ref in ("U1", "U2", "U3"):
    px, py = pin_xy(ref, 2)
    wire([(en_trunk_x, py), (px, py)])
    JOINTS.add((snap(en_trunk_x), snap(py)))
    en_ys.append(py)
draw_vtrunk(en_trunk_x, en_ys)

# UART bus (single electrical node: ESP RX + ESP TX + all 3 drivers' pin14)
rx_x, rx_y = pin_xy("ESP1", 8)
tx_x, tx_y = pin_xy("ESP1", 9)
uart_x = rx_x + 55
wire([(rx_x, rx_y), (uart_x, rx_y)])
wire([(tx_x, tx_y), (uart_x, tx_y)])
JOINTS.add((snap(uart_x), snap(rx_y)))
JOINTS.add((snap(uart_x), snap(tx_y)))
uart_ys = [rx_y, tx_y]
for ref in ("U1", "U2", "U3"):
    px, py = pin_xy(ref, 14)
    wire([(uart_x, py), (px, py)])
    JOINTS.add((snap(uart_x), snap(py)))
    uart_ys.append(py)
draw_vtrunk(uart_x, uart_ys)
note(uart_x + 2, (rx_y + tx_y) / 2,
     "physical board: separate TX/RX pads, on-board resistor between them -- one node here", size=1.0)

# I2C mux bus: ESP1 pin10(SDA)/pin11(SCL) -> TCA9548A SDA(20)/SCL(19)
connect(("ESP1", 10), ("U4", 20))
connect(("ESP1", 11), ("U4", 19))

# TCA9548A channel0(pan)/channel1(tilt) -> AS5600 breakouts
connect(("U4", 1), ("J7", 3))  # SD0 -> AS5600 pan SDA
connect(("U4", 2), ("J7", 4))  # SC0 -> AS5600 pan SCL
connect(("U4", 3), ("J8", 3))  # SD1 -> AS5600 tilt SDA
connect(("U4", 4), ("J8", 4))  # SC1 -> AS5600 tilt SCL

# Encoders: ESP2 -> J4/J5/J6 (EncoderSwitch pins: 1=A 2=C 3=B 4=S1 5=S2)
connect(("ESP2", 3), ("J4", 1))   # A
connect(("ESP2", 4), ("J4", 3))   # B
connect(("ESP2", 5), ("J4", 4))   # S1 (push)
connect(("ESP2", 6), ("J5", 1))
connect(("ESP2", 7), ("J5", 3))
connect(("ESP2", 8), ("J5", 4))
connect(("ESP2", 9), ("J6", 1))
connect(("ESP2", 10), ("J6", 3))
connect(("ESP2", 11), ("J6", 4))

# Limit switches: ESP2 -> J9/J10 pin1
connect(("ESP2", 12), ("J9", 1))
connect(("ESP2", 13), ("J10", 1))

# OLED: ESP2 -> J11 SDA(3)/SCL(4)
connect(("ESP2", 1), ("J11", 3))
connect(("ESP2", 2), ("J11", 4))

# Motor phases: TMC OA1(24)/OA2(21)/OB1(26)/OB2(1) -> motor connector 1/2/3/4
for tmc_ref, mot_ref in (("U1", "J1"), ("U2", "J2"), ("U3", "J3")):
    connect((tmc_ref, 24), (mot_ref, 1))
    connect((tmc_ref, 21), (mot_ref, 2))
    connect((tmc_ref, 26), (mot_ref, 3))
    connect((tmc_ref, 1), (mot_ref, 4))

# Genuinely unused pins, explicitly marked no-connect (not just left dangling):
# charge-pump/analog-reference network (populated on the breakout board
# itself, nothing to wire externally), StallGuard/status outputs this
# firmware doesn't read, and the mux's unused channel 2.
for tmc_ref in ("U1", "U2", "U3"):
    for pinnum in (11, 12, 17, 20, 23, 25, 27, 4, 5, 6, 7, 8):
        no_connect(tmc_ref, pinnum)
no_connect("U4", 5)  # SD2 (unused channel 2)
no_connect("U4", 6)  # SC2

# ===========================================================================
# RAILS (drawn as real wires, not power symbols -- a power symbol is another
# kind of label: same net name = same net, no drawn connection).
# ===========================================================================
GND_Y = 470
V3_Y = 15
VM_Y = 5
RAIL_X0, RAIL_X1 = 10, 600

for ref, pin in [("U1", 3), ("U2", 3), ("U3", 3), ("U4", 9),
                 ("U4", 18), ("U4", 23), ("U4", 22),  # A2/A1/A0 -> GND (I2C addr 0x70)
                 ("J4", 2), ("J4", 5), ("J5", 2), ("J5", 5), ("J6", 2), ("J6", 5),
                 ("J7", 2), ("J8", 2), ("J9", 2), ("J10", 2), ("J11", 2),
                 ("ESP3", 2), ("PWR1", 2)]:
    tap(ref, pin, GND_Y)

for ref, pin in [("U1", 15), ("U2", 15), ("U3", 15), ("U4", 21),
                 ("U4", 24),  # ~RESET -> 3V3 (not in reset)
                 ("J7", 1), ("J8", 1), ("J11", 1), ("ESP3", 1)]:
    tap(ref, pin, V3_Y)

for ref, pin in [("U1", 22), ("U2", 22), ("U3", 22), ("PWR1", 1)]:
    tap(ref, pin, VM_Y)

note(RAIL_X0, GND_Y - 3, "GND rail")
note(RAIL_X0, V3_Y - 3, "3V3 rail")
note(RAIL_X0, VM_Y - 3, "VM rail (12-24V, from PWR1)")

# ===========================================================================
# MS1/MS2 address straps (short stub straight to the GND or 3V3 rail) and
# CLK->GND (use internal oscillator, per TMC2209 datasheet guidance).
# These are just more taps onto the same rails, so they go through tap()
# too -- RAIL_TAPS must be complete before draw_rail() runs below.
# ===========================================================================
def strap_to_rail(ref, pinnum, rail_y, label):
    px, py = pin_xy(ref, pinnum)
    tap(ref, pinnum, rail_y)
    note(px - 8, py + 1.5, label, size=1.0)

strap_to_rail("U1", 9, GND_Y, "MS1->GND (addr0)")
strap_to_rail("U1", 10, GND_Y, "MS2->GND (addr0)")
strap_to_rail("U2", 9, V3_Y, "MS1->3V3 (addr1)")
strap_to_rail("U2", 10, GND_Y, "MS2->GND (addr1)")
strap_to_rail("U3", 9, GND_Y, "MS1->GND (addr2)")
strap_to_rail("U3", 10, V3_Y, "MS2->3V3 (addr2)")

for ref in ("U1", "U2", "U3"):
    strap_to_rail(ref, 13, GND_Y, "CLK->GND")

# Now that every tap (including the address/CLK straps above) has registered
# its x in RAIL_TAPS, draw each rail as segments between consecutive taps.
draw_rail(GND_Y, RAIL_X0, RAIL_X1)
draw_rail(V3_Y, RAIL_X0, RAIL_X1)
draw_rail(VM_Y, RAIL_X0, RAIL_X1)

# ===========================================================================
# Envelope
# ===========================================================================
HEADER = (
    '(kicad_sch\n\t(version 20250114)\n\t(generator "eeschema")\n'
    '\t(generator_version "9.0")\n\t(uuid "%s")\n\t(paper "A0")\n'
    '\t(lib_symbols\n%s\t)\n'
) % (U(), ''.join(LIB_SYMBOLS))

FOOTER = (junction_blocks() +
          '\t(sheet_instances\n\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n\t)\n'
          '\t(embedded_fonts no)\n)\n')

with open("prototype_wired.kicad_sch", "w", encoding="utf-8") as f:
    f.write(HEADER + ''.join(OUT) + FOOTER)

print("wrote prototype_wired.kicad_sch:", len(OUT), "body blocks,", len(LIB_SYMBOLS), "lib symbols,",
      len(JOINTS), "junctions")
