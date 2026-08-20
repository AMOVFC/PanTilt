import math
import os
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

# Outward direction (away from the symbol body) in SHEET space, keyed by the
# library pin's angle. Sheet +y is DOWN the page.
OUTWARD = {
    0:   (-1.0,  0.0),   # pin body extends +x  -> wire leaves leftward
    180: ( 1.0,  0.0),   # pin body extends -x  -> wire leaves rightward
    90:  ( 0.0,  1.0),   # pin body extends up(sheet)   -> wire leaves down
    270: ( 0.0, -1.0),   # pin body extends down(sheet) -> wire leaves up
}

# LABEL ORIENTATION -- measured from KiCad's own output, not guessed.
# Keyed by the PIN's angle, giving the (label angle, justify) that makes the
# pentagon's point touch the wire and its body extend AWAY from the pin:
#
#   pin angle 0   (wire leaves left)  -> label 180, justify right
#   pin angle 180 (wire leaves right) -> label 0,   justify left
#
# Both halves matter. Wrong ANGLE mirrors the label; wrong JUSTIFY anchors the
# text on the wrong edge so it grows back over the pin numbers. An earlier
# revision had both wrong and papered over it with a 25mm stub.
LABEL_STYLE = {
    0:   (180, 'right'),
    180: (0,   'left'),
    90:  (270, 'right'),
    270: (90,  'left'),
}

STUB = 7.62      # wire length from pin to label anchor
NC_STUB = 5.08   # no-connects carry no text, so they need less room

def wire_block(x1, y1, x2, y2):
    return ('\t(wire\n\t\t(pts (xy %s %s) (xy %s %s))\n\t\t(stroke (width 0) (type default))\n'
            '\t\t(uuid "%s")\n\t)\n') % (x1, y1, x2, y2, U())

def global_label_block(name, x, y, angle, justify):
    return ('\t(global_label "%s"\n\t\t(shape input)\n\t\t(at %s %s %s)\n'
            '\t\t(effects (font (size 1.27 1.27)) (justify ' + justify + '))\n\t\t(uuid "%s")\n'
            '\t\t(property "Intersheetrefs" "${INTERSHEET_REFS}"\n\t\t\t(at %s %s 0)\n'
            '\t\t\t(hide yes)\n\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n'
            '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n\t)\n'
            ) % (name, x, y, angle, U(), x, y)

def no_connect_block(x, y):
    return '\t(no_connect\n\t\t(at %s %s)\n\t\t(uuid "%s")\n\t)\n' % (x, y, U())

def symbol_block(ref, lib_id, x, y, footprint, value, description, datasheet, pins,
                 ref_dy, val_dy, project="pantiltslide_full",
                 sheet_path="/069dac62-ca78-4a21-b689-b9421532705f"):
    L = []
    L.append('\t(symbol\n\t\t(lib_id "%s")\n\t\t(at %s %s 0)\n\t\t(unit 1)\n'
             '\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n'
             '\t\t(in_pos_files yes)\n\t\t(dnp no)\n\t\t(fields_autoplaced yes)\n'
             '\t\t(uuid "%s")\n' % (lib_id, x, y, U()))
    L.append('\t\t(property "Reference" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
             '\t\t\t(do_not_autoplace no)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n'
             % (ref, x, round(y + ref_dy, 3)))
    L.append('\t\t(property "Value" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
             '\t\t\t(do_not_autoplace no)\n\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n'
             % (value, x, round(y + val_dy, 3)))
    for k, v in (("Footprint", footprint), ("Datasheet", datasheet),
                 ("Description", description)):
        L.append('\t\t(property "%s" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
                 '\t\t\t(do_not_autoplace no)\n\t\t\t(hide yes)\n'
                 '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n' % (k, v, x, y))
    for pn in pins:
        L.append('\t\t(pin "%s"\n\t\t\t(uuid "%s")\n\t\t)\n' % (pn, U()))
    L.append('\t\t(instances\n\t\t\t(project "%s"\n\t\t\t\t(path "%s"\n'
             '\t\t\t\t\t(reference "%s")\n\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n'
             % (project, sheet_path, ref))
    L.append('\t)\n')
    return ''.join(L)

def wire_and_label(px, py, angle, net):
    dx, dy = OUTWARD[angle]
    ex = round(px + dx * STUB, 3)
    ey = round(py + dy * STUB, 3)
    emit(wire_block(px, py, ex, ey))
    lab_angle, justify = LABEL_STYLE[angle]
    emit(global_label_block(net, ex, ey, lab_angle, justify))

def wire_nc(px, py, angle):
    dx, dy = OUTWARD[angle]
    ex = round(px + dx * NC_STUB, 3)
    ey = round(py + dy * NC_STUB, 3)
    emit(wire_block(px, py, ex, ey))
    emit(no_connect_block(ex, ey))

def place(ref, lib_id, x, y, footprint, value, description, datasheet,
          pin_defs, pin_nets, nc_pins, ref_dy, val_dy):
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
# Library is "TerminalBlock" -- "MaiXu" is part of the footprint NAME, not the
# library name. Verified against the installed library and against what the
# original PCB already used.
ST2_FP = "TerminalBlock:TerminalBlock_MaiXu_MX126-5.0-02P_1x02_P5.00mm"

TMC_NC = {13, 17, 18}   # SPRD (stealthChop via onboard pulldown), INDEX, DIAG

# All four drivers share one half-duplex UART bus. On the SilentStepStick the
# chip's single PDN_UART pin is broken out twice: header pin 12 ("UART") via an
# onboard 1k series resistor, and header pin 11 ("PDN") directly.
TMC_TX_NET = "GPIO_41"   # ESP32 TX -> pin 12 UART (through module's 1k)
TMC_RX_NET = "GPIO_42"   # ESP32 RX <- pin 11 PDN  (direct)

# With UART in use MS1/MS2 stop selecting microstepping and become the slave
# address (MS1 = bit0, MS2 = bit1). Strapped on-board so a kit builder has
# nothing to configure and cannot create a duplicate-address bus conflict.
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

def add_c4(ref, x, y, value, desc, nets, nc=()):
    place(ref, "Connector:Conn_01x04_Pin", x, y, C4_FP, value, desc, "",
          C4_PINS, nets, set(nc), C4_REF_DY, C4_VAL_DY)

def add_c8(ref, x, y, value, desc, nets, nc=()):
    place(ref, "Connector:Conn_01x08_Socket", x, y, C8_FP, value, desc, "",
          C8_PINS, nets, set(nc), C8_REF_DY, C8_VAL_DY)

def add_st2(ref, x, y, value, desc, nets):
    place(ref, "Connector:Screw_Terminal_01x02", x, y, ST2_FP, value, desc, "",
          ST2_PINS, nets, set(), ST2_REF_DY, ST2_VAL_DY)

# ===========================================================================
# PLACEMENT
#   col 1 x=290  stepper drivers          col 3 x=430  sensors + axis encoders
#   col 2 x=360  motor connectors         col 4 x=520  mux, limits, buttons
# ===========================================================================

#                                                                                    addr  MS1     MS2
add_tmc("U1", 290, 40,  "GPIO_4",  "GPIO_5",  "SLIDE_M1A", "SLIDE_M1B", "SLIDE_M2A", "SLIDE_M2B", "GND",  "GND")   # 0
add_tmc("U2", 290, 115, "GPIO_6",  "GPIO_7",  "PAN_M1A",   "PAN_M1B",   "PAN_M2A",   "PAN_M2B",   "3.3v", "GND")   # 1
add_tmc("U3", 290, 190, "GPIO_8",  "GPIO_9",  "TILT_M1A",  "TILT_M1B",  "TILT_M2A",  "TILT_M2B",  "GND",  "3.3v")  # 2
add_tmc("U4", 290, 265, "GPIO_47", "GPIO_48", "AUX_M1A",   "AUX_M1B",   "AUX_M2A",   "AUX_M2B",   "3.3v", "3.3v")  # 3

add_c4("J17", 360, 40,  "Motor_Slide", "NEMA17 slide motor, 4-wire",
       {1: "SLIDE_M1A", 2: "SLIDE_M1B", 3: "SLIDE_M2A", 4: "SLIDE_M2B"})
add_c4("J18", 360, 115, "Motor_Pan", "NEMA17 pan motor, 4-wire",
       {1: "PAN_M1A", 2: "PAN_M1B", 3: "PAN_M2A", 4: "PAN_M2B"})
add_c4("J19", 360, 190, "Motor_Tilt", "NEMA17 tilt motor, 4-wire",
       {1: "TILT_M1A", 2: "TILT_M1B", 3: "TILT_M2A", 4: "TILT_M2B"})
add_c4("J28", 360, 265, "Motor_Aux", "NEMA17 4th-axis motor (focus/zoom/slide-rotate), 4-wire",
       {1: "AUX_M1A", 2: "AUX_M1B", 3: "AUX_M2A", 4: "AUX_M2B"})

add_c4("J21", 430, 40,  "AS5600_Pan", "AS5600 magnetic angle sensor, pan axis (via TCA9548A ch0)",
       {1: "3.3v", 2: "GND", 3: "MUX_CH0_SDA", 4: "MUX_CH0_SCL"})
add_c4("J22", 430, 80,  "AS5600_Tilt", "AS5600 magnetic angle sensor, tilt axis (via TCA9548A ch1)",
       {1: "3.3v", 2: "GND", 3: "MUX_CH1_SDA", 4: "MUX_CH1_SCL"})
add_c4("J23", 430, 120, "OLED", "SSD1306 OLED status display, isolated I2C bus B",
       {1: "3.3v", 2: "GND", 3: "GPIO_13", 4: "GPIO_14"})

# One manual control encoder per axis. Pin 4 is left as a no-connect so the
# encoder's push switch can be brought in later without a board change.
add_c4("J29", 430, 170, "Enc_Slide", "Slide-axis manual control encoder (A/B quadrature, common to GND)",
       {1: "GPIO_15", 2: "GPIO_16", 3: "GND"}, nc=(4,))
add_c4("J30", 430, 210, "Enc_Pan", "Pan-axis manual control encoder (A/B quadrature, common to GND)",
       {1: "GPIO_17", 2: "GPIO_18", 3: "GND"}, nc=(4,))
add_c4("J31", 430, 250, "Enc_Tilt", "Tilt-axis manual control encoder (A/B quadrature, common to GND)",
       {1: "GPIO_21", 2: "GPIO_38", 3: "GND"}, nc=(4,))
add_c4("J32", 430, 290, "Enc_Aux", "4th-axis manual control encoder (A/B quadrature, common to GND)",
       {1: "GPIO_19", 2: "GPIO_20", 3: "GND"}, nc=(4,))

add_c8("J20", 520, 40,  "TCA9548A_Mux", "TCA9548A 8-ch I2C mux, ch0->pan AS5600, ch1->tilt AS5600",
       {1: "3.3v", 2: "GND", 3: "GPIO_11", 4: "GPIO_12", 5: "MUX_CH0_SDA", 6: "MUX_CH0_SCL",
        7: "MUX_CH1_SDA", 8: "MUX_CH1_SCL"})

add_st2("J26", 520, 120, "Limit_Min", "Slide axis min-travel limit switch, idle HIGH / triggered LOW",
        {1: "GPIO_39", 2: "GND"})
add_st2("J27", 520, 160, "Limit_Max", "Slide axis max-travel limit switch, idle HIGH / triggered LOW",
        {1: "GPIO_40", 2: "GND"})

# Keyframe/transport buttons -- one named terminal each so the schematic says
# which button is which, rather than leaving a bare GPIO number for a kit
# builder to guess at. All active-LOW to GND on the ESP32's internal pull-ups,
# so no external resistors are needed.
#
# GPIO_45/46 are strapping pins that must read LOW at reset. A button-to-GND is
# safe on both: their default state is an internal pull-down (already LOW), and
# holding one down during power-up only reinforces that same required LOW.
BTN_DESC = "%s pushbutton, active-low to GND (enable the internal pull-up in firmware)"
add_st2("J33", 520, 210, "Btn_SetKeyframe",   BTN_DESC % "Set keyframe",
        {1: "GPIO_1", 2: "GND"})
add_st2("J34", 520, 245, "Btn_ClearKeyframe", BTN_DESC % "Delete/clear keyframe",
        {1: "GPIO_3", 2: "GND"})
add_st2("J35", 520, 280, "Btn_PlayPause",     BTN_DESC % "Play/pause",
        {1: "GPIO_45", 2: "GND"})
add_st2("J36", 520, 315, "Btn_Reset",         BTN_DESC % "Reset",
        {1: "GPIO_46", 2: "GND"})

_OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '_body.generated.txt')
with open(_OUT, 'w', encoding='utf-8') as f:
    f.write(''.join(OUT))

print("emitted blocks:", len(OUT))
