import uuid

def U():
    return str(uuid.uuid4())

OUT = []

def emit(s):
    OUT.append(s)

# ---------------------------------------------------------------------------
# COORDINATE CONVENTION (validated against the original KiCad-authored file):
#   sheet_x = origin_x + local_x
#   sheet_y = origin_y - local_y      <-- Y IS INVERTED (.kicad_sym is Y-up,
#                                         .kicad_sch is Y-down)
# Ground truth: J1 (Conn_01x22_Socket) at origin y=104.14, pin4 local y=17.78,
# and KiCad placed that pin's GPIO_4 label at y=86.36 == 104.14 - 17.78.
# ---------------------------------------------------------------------------

def sheet_pos(cx, cy, lx, ly):
    return (round(cx + lx, 3), round(cy - ly, 3))

# Outward direction (away from the symbol body) expressed in SHEET space,
# keyed by the library pin's angle. Sheet +y is DOWN the page.
OUTWARD = {
    0:   (-1.0,  0.0),   # pin body extends +x  -> wire leaves leftward
    180: ( 1.0,  0.0),   # pin body extends -x  -> wire leaves rightward
    90:  ( 0.0,  1.0),   # pin body extends +y(sym)=up(sheet) -> wire leaves down
    270: ( 0.0, -1.0),   # pin body extends -y(sym)=down(sheet) -> wire leaves up
}

# A global_label with justify=right anchors its RIGHT edge at the wire end and
# the text box grows back toward the pin, so required clearance scales with
# label text length. 25.4 comfortably clears the longest names used here
# (11 chars, e.g. MUX_CH0_SCL) -- verified in a render.
STUB = 25.4
NC_STUB = 7.62   # no-connects carry no text, so they need far less room

def wire_block(x1, y1, x2, y2):
    return '\t(wire\n\t\t(pts (xy %s %s) (xy %s %s))\n\t\t(stroke (width 0) (type default))\n\t\t(uuid "%s")\n\t)\n' % (x1, y1, x2, y2, U())

def global_label_block(name, x, y):
    return ('\t(global_label "%s"\n\t\t(shape input)\n\t\t(at %s %s 180)\n'
            '\t\t(effects (font (size 1.27 1.27)) (justify right))\n\t\t(uuid "%s")\n'
            '\t\t(property "Intersheetrefs" "${INTERSHEET_REFS}"\n\t\t\t(at %s %s 0)\n\t\t\t(hide yes)\n\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n\t)\n'
            ) % (name, x, y, U(), x, y)

def no_connect_block(x, y):
    return '\t(no_connect\n\t\t(at %s %s)\n\t\t(uuid "%s")\n\t)\n' % (x, y, U())

def symbol_block(ref, lib_id, x, y, footprint, value, description, datasheet, pins,
                  ref_dy, val_dy, project="pantiltslide_full",
                  sheet_path="/069dac62-ca78-4a21-b689-b9421532705f"):
    lines = []
    lines.append('\t(symbol\n\t\t(lib_id "%s")\n\t\t(at %s %s 0)\n\t\t(unit 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(in_pos_files yes)\n\t\t(dnp no)\n\t\t(fields_autoplaced yes)\n\t\t(uuid "%s")\n' % (lib_id, x, y, U()))
    lines.append('\t\t(property "Reference" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (ref, x, round(y + ref_dy, 3)))
    lines.append('\t\t(property "Value" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (value, x, round(y + val_dy, 3)))
    lines.append('\t\t(property "Footprint" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n\t\t\t(hide yes)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (footprint, x, y))
    lines.append('\t\t(property "Datasheet" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n\t\t\t(hide yes)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (datasheet, x, y))
    lines.append('\t\t(property "Description" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n\t\t\t(hide yes)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (description, x, y))
    for pn in pins:
        lines.append('\t\t(pin "%s"\n\t\t\t(uuid "%s")\n\t\t)\n' % (pn, U()))
    lines.append('\t\t(instances\n\t\t\t(project "%s"\n\t\t\t\t(path "%s"\n\t\t\t\t\t(reference "%s")\n\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n' % (project, sheet_path, ref))
    lines.append('\t)\n')
    return ''.join(lines)

def wire_and_label(px, py, angle, net):
    dx, dy = OUTWARD[angle]
    ex = round(px + dx * STUB, 3)
    ey = round(py + dy * STUB, 3)
    emit(wire_block(px, py, ex, ey))
    emit(global_label_block(net, ex, ey))

def wire_nc(px, py, angle):
    dx, dy = OUTWARD[angle]
    ex = round(px + dx * NC_STUB, 3)
    ey = round(py + dy * NC_STUB, 3)
    emit(wire_block(px, py, ex, ey))
    emit(no_connect_block(ex, ey))

def place(ref, lib_id, x, y, footprint, value, description, datasheet,
          pin_defs, pin_nets, nc_pins, ref_dy, val_dy):
    """pin_defs: {pin_number: (local_x, local_y, angle)}"""
    emit(symbol_block(ref, lib_id, x, y, footprint, value, description, datasheet,
                      [str(n) for n in sorted(pin_defs, key=int)], ref_dy, val_dy))
    for pn in sorted(pin_defs, key=int):
        lx, ly, ang = pin_defs[pn]
        px, py = sheet_pos(x, y, lx, ly)
        net = pin_nets.get(pn)
        if net:
            wire_and_label(px, py, ang, net)
        elif pn in nc_pins:
            wire_nc(px, py, ang)

# ===========================================================================
# Library pin geometry, read verbatim from the cached lib_symbols definitions.
# ===========================================================================
TMC_PINS = {
    1: (-5.08, 7.62, 0),    2: (-5.08, 5.08, 0),    3: (-5.08, 2.54, 0),   4: (-5.08, 0, 0),
    5: (-5.08, -2.54, 0),   6: (-5.08, -5.08, 0),   7: (-5.08, -7.62, 0),  8: (-5.08, -10.16, 0),
    9: (11.43, 7.62, 180), 10: (11.43, 5.08, 180), 11: (11.43, 2.54, 180), 12: (11.43, 0, 180),
    13: (11.43, -2.54, 180), 14: (11.43, -5.08, 180), 15: (11.43, -7.62, 180), 16: (11.43, -10.16, 180),
    17: (2.54, -15.24, 90), 18: (5.08, -15.24, 90),
}
C4_PINS = {1: (5.08, 2.54, 180), 2: (5.08, 0, 180), 3: (5.08, -2.54, 180), 4: (5.08, -5.08, 180)}
C8_PINS = {1: (-5.08, 7.62, 0), 2: (-5.08, 5.08, 0), 3: (-5.08, 2.54, 0), 4: (-5.08, 0, 0),
           5: (-5.08, -2.54, 0), 6: (-5.08, -5.08, 0), 7: (-5.08, -7.62, 0), 8: (-5.08, -10.16, 0)}
ST2_PINS = {1: (-5.08, 0, 0), 2: (-5.08, -2.54, 0)}

# Ref/Value sit above the topmost pin. Topmost pin sheet offset = -max(local_y).
TMC_REF_DY, TMC_VAL_DY = -14.8, -12.0   # top pin at -7.62
C4_REF_DY,  C4_VAL_DY  = -9.8,  -7.0    # top pin at -2.54
C8_REF_DY,  C8_VAL_DY  = -14.8, -12.0   # top pin at -7.62
ST2_REF_DY, ST2_VAL_DY = -8.8,  -6.0    # top pin at 0

TMC_FP = "TMC2209:TMC2209_SilentStepStick_Socket_18P"
TMC_DS = "https://www.mouser.com/pdfDocs/TMC2209_SilentStepStick_Rev110.pdf"
C4_FP  = "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical"
C8_FP  = "Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical"
ST2_FP = "TerminalBlock_MaiXu:TerminalBlock_MaiXu_MX126-5.0-02P_1x02_P5.00mm"

TMC_NC = {13, 17, 18}   # SPRD (stealthChop via onboard pulldown), INDEX, DIAG

# All three drivers share one half-duplex UART bus. On the SilentStepStick the
# chip's single PDN_UART pin is broken out twice: header pin 12 ("UART") via an
# onboard 1k series resistor, and header pin 11 ("PDN") directly. The standard
# hookup drives TX into the resistor side and listens on the direct side.
TMC_TX_NET = "GPIO_41"   # ESP32 TX -> pin 12 UART (through module's 1k)
TMC_RX_NET = "GPIO_42"   # ESP32 RX <- pin 11 PDN  (direct)

# With UART in use, MS1/MS2 stop selecting microstepping and become the slave
# address (MS1 = bit0, MS2 = bit1). Strapped on-board so a kit builder has
# nothing to configure and no way to create a duplicate-address bus conflict.
#   U1 slide = 0, U2 pan = 1, U3 tilt = 2
def add_tmc(ref, x, y, step, dirn, m1a, m1b, m2a, m2b, ms1, ms2):
    place(ref, "New_Library:tmc2209", x, y, TMC_FP, "tmc2209",
          "TMC2209 SilentStepStick stepper driver module (UART mode; MS1/MS2 strap the slave address)",
          TMC_DS, TMC_PINS,
          {1: "GND", 2: "3.3v", 3: m1b, 4: m1a, 5: m2a, 6: m2b, 7: "GND", 8: "+24V",
           9: dirn, 10: step,
           11: TMC_RX_NET, 12: TMC_TX_NET,
           14: ms2, 15: ms1,
           16: "GPIO_10"},
          TMC_NC, TMC_REF_DY, TMC_VAL_DY)

def add_c4(ref, x, y, value, desc, nets):
    place(ref, "Connector:Conn_01x04_Pin", x, y, C4_FP, value, desc, "",
          C4_PINS, nets, set(), C4_REF_DY, C4_VAL_DY)

def add_c8(ref, x, y, value, desc, nets):
    place(ref, "Connector:Conn_01x08_Socket", x, y, C8_FP, value, desc, "",
          C8_PINS, nets, set(), C8_REF_DY, C8_VAL_DY)

def add_st2(ref, x, y, value, desc, nets):
    place(ref, "Connector:Screw_Terminal_01x02", x, y, ST2_FP, value, desc, "",
          ST2_PINS, nets, set(), ST2_REF_DY, ST2_VAL_DY)

# ===========================================================================
# Placement. Columns are spaced so each part's label text clears its neighbour.
# ===========================================================================
#                                                                                      addr  MS1     MS2
add_tmc("U1", 290, 40,  "GPIO_4", "GPIO_5", "SLIDE_M1A", "SLIDE_M1B", "SLIDE_M2A", "SLIDE_M2B", "GND",   "GND")    # 0
add_tmc("U2", 290, 115, "GPIO_6", "GPIO_7", "PAN_M1A",   "PAN_M1B",   "PAN_M2A",   "PAN_M2B",   "3.3v",  "GND")    # 1
add_tmc("U3", 290, 190, "GPIO_8", "GPIO_9", "TILT_M1A",  "TILT_M1B",  "TILT_M2A",  "TILT_M2B",  "GND",   "3.3v")   # 2

add_c4("J17", 380, 40,  "Motor_Slide", "NEMA17 slide motor, 4-wire",
       {1: "SLIDE_M1A", 2: "SLIDE_M1B", 3: "SLIDE_M2A", 4: "SLIDE_M2B"})
add_c4("J18", 380, 115, "Motor_Pan", "NEMA17 pan motor, 4-wire",
       {1: "PAN_M1A", 2: "PAN_M1B", 3: "PAN_M2A", 4: "PAN_M2B"})
add_c4("J19", 380, 190, "Motor_Tilt", "NEMA17 tilt motor, 4-wire",
       {1: "TILT_M1A", 2: "TILT_M1B", 3: "TILT_M2A", 4: "TILT_M2B"})

add_c4("J21", 470, 40,  "AS5600_Pan", "AS5600 magnetic angle sensor, pan axis (via TCA9548A ch0)",
       {1: "3.3v", 2: "GND", 3: "MUX_CH0_SDA", 4: "MUX_CH0_SCL"})
add_c4("J22", 470, 90,  "AS5600_Tilt", "AS5600 magnetic angle sensor, tilt axis (via TCA9548A ch1)",
       {1: "3.3v", 2: "GND", 3: "MUX_CH1_SDA", 4: "MUX_CH1_SCL"})
add_c4("J23", 470, 140, "OLED", "SSD1306 OLED status display, isolated I2C bus B",
       {1: "3.3v", 2: "GND", 3: "GPIO_13", 4: "GPIO_14"})
add_c4("J24", 470, 190, "Jog_Encoder", "Jog rotary encoder w/ push: speed and dir, BLE resync/record",
       {1: "GPIO_15", 2: "GPIO_16", 3: "GPIO_17", 4: "GND"})
add_c4("J25", 470, 240, "Manual_Setpoint_Encoder",
       "Manual jog knob (mechanical quadrature encoder, not AS5600) for dialing in pan/tilt angle setpoint; push selects axis",
       {1: "GPIO_18", 2: "GPIO_21", 3: "GPIO_38", 4: "GND"})

add_c8("J20", 560, 40,  "TCA9548A_Mux", "TCA9548A 8-ch I2C mux, ch0->pan AS5600, ch1->tilt AS5600",
       {1: "3.3v", 2: "GND", 3: "GPIO_11", 4: "GPIO_12", 5: "MUX_CH0_SDA", 6: "MUX_CH0_SCL",
        7: "MUX_CH1_SDA", 8: "MUX_CH1_SCL"})

add_st2("J26", 560, 130, "Limit_Min", "Slide axis min-travel limit switch, idle HIGH / triggered LOW",
        {1: "GPIO_39", 2: "GND"})
add_st2("J27", 560, 170, "Limit_Max", "Slide axis max-travel limit switch, idle HIGH / triggered LOW",
        {1: "GPIO_40", 2: "GND"})

import os
_OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '_body.generated.txt')
with open(_OUT, 'w', encoding='utf-8') as f:
    f.write(''.join(OUT))

print("emitted blocks:", len(OUT))
