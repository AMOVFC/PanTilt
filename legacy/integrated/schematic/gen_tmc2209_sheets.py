"""Generate the two TMC2209 driver sub-schematics for the PanTilt integrated board.

Both files expose an IDENTICAL hierarchical-pin interface so they are drop-in
swappable on the parent sheet:

    STEP0 DIR0 DIAG0  STEP1 DIR1 DIAG1  STEP2 DIR2 DIAG2  STEP3 DIR3 DIAG3
    EN  UART

Power (+24V, +3V3, GND) crosses the sheet boundary through global power symbols.
"""

import uuid as _uuid
import os

NS = _uuid.UUID('11111111-2222-3333-4444-555555555555')
SRC = r"D:\CODE\PanTilt\integrated\schematic\pantiltslide_integrated.kicad_sch"
KLIB = r"C:\Program Files\KiCad\10.0\share\kicad\symbols\Driver_Motor.kicad_sym"
OUT = r"D:\CODE\PanTilt\integrated\schematic"

_seen = set()


def U(tag):
    """Deterministic uuid so regenerating the file produces a stable diff."""
    assert tag not in _seen, "duplicate uuid tag " + tag
    _seen.add(tag)
    return str(_uuid.uuid5(NS, tag))


# --------------------------------------------------------------------------
# raw lib_symbol extraction
# --------------------------------------------------------------------------

def _block(text, start):
    """Return the balanced s-expression starting at index `start`."""
    depth = 0
    i = start
    instr = False
    while i < len(text):
        c = text[i]
        if instr:
            if c == chr(92):
                i += 2
                continue
            if c == '"':
                instr = False
        elif c == '"':
            instr = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
        i += 1
    raise ValueError('unbalanced')


def from_sch(name):
    s = open(SRC, encoding='utf-8').read()
    key = '\t\t(symbol "%s"\n' % name
    i = s.index(key)
    return '		' + _block(s, i + 2)


# tokens that only exist in the KiCad 10 file format; the rest of this project
# is still serialised as 20250114, so strip them out.
K10_ONLY = ('(duplicate_pin_numbers_are_jumpers ', '(do_not_autoplace ')


def from_lib(name, libname):
    s = open(KLIB, encoding='utf-8').read()
    key = '\t(symbol "%s"\n' % name
    i = s.index(key)
    blk = _block(s, i + 1)
    out = []
    for k, line in enumerate(blk.split('\n')):
        if any(t in line for t in K10_ONLY):
            continue
        if k == 0:
            # inside a schematic the lib_symbols entry is keyed "Lib:Name"
            line = line.replace('"%s"' % name, '"%s:%s"' % (libname, name), 1)
        out.append(('\t\t' if k == 0 else '\t') + line)
    return '\n'.join(out)


# --------------------------------------------------------------------------
# emitters
# --------------------------------------------------------------------------

def n(v):
    """KiCad writes numbers without trailing .0"""
    v = round(v + 0.0, 4)
    if v == int(v):
        return str(int(v))
    return ('%f' % v).rstrip('0')


class Sheet:
    def __init__(self, title, comment, paper='A2'):
        self.title = title
        self.comment = comment
        self.paper = paper
        self.items = []
        self.libs = []
        self.uuid = None
        self.pwr = 200   # parent sheet already uses #PWR1..#PWR123

    def wire(self, x1, y1, x2, y2, tag):
        self.items.append(
            '\t(wire\n\t\t(pts\n\t\t\t(xy %s %s) (xy %s %s)\n\t\t)\n'
            '\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n'
            '\t\t(uuid "%s")\n\t)' % (n(x1), n(y1), n(x2), n(y2), U(tag)))

    def poly(self, pts, tag):
        for k in range(len(pts) - 1):
            self.wire(pts[k][0], pts[k][1], pts[k + 1][0], pts[k + 1][1],
                      '%s.%d' % (tag, k))

    def nc(self, x, y, tag):
        self.items.append('\t(no_connect\n\t\t(at %s %s)\n\t\t(uuid "%s")\n\t)'
                          % (n(x), n(y), U(tag)))

    def label(self, text, x, y, rot, tag):
        just = 'left' if rot == 0 else 'right'
        self.items.append(
            '\t(label "%s"\n\t\t(at %s %s %d)\n\t\t(fields_autoplaced yes)\n'
            '\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n'
            '\t\t\t(justify %s bottom)\n\t\t)\n\t\t(uuid "%s")\n\t)'
            % (text, n(x), n(y), rot, just, U(tag)))

    def hlabel(self, text, shape, x, y, rot, tag):
        just = 'left' if rot == 0 else 'right'
        self.items.append(
            '\t(hierarchical_label "%s"\n\t\t(shape %s)\n\t\t(at %s %s %d)\n'
            '\t\t(fields_autoplaced yes)\n'
            '\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n'
            '\t\t\t(justify %s)\n\t\t)\n\t\t(uuid "%s")\n\t)'
            % (text, shape, n(x), n(y), rot, just, U(tag)))

    def text(self, s, x, y, size, tag, bold=True):
        self.items.append(
            '\t(text "%s"\n\t\t(exclude_from_sim no)\n\t\t(at %s %s 0)\n'
            '\t\t(effects\n\t\t\t(font\n\t\t\t\t(size %s %s)\n'
            '\t\t\t\t(bold %s)\n\t\t\t)\n\t\t\t(justify left bottom)\n\t\t)\n'
            '\t\t(uuid "%s")\n\t)'
            % (s, n(x), n(y), n(size), n(size), 'yes' if bold else 'no', U(tag)))

    # ---- symbols -------------------------------------------------------
    def symbol(self, lib_id, ref, value, x, y, tag, footprint='',
               datasheet='', description='', npins=1, rot=0,
               ref_dy=-2.54, val_dy=2.54, hide_ref=False, hide_val=False,
               extra_props=(), ref_dx=0.0, val_dx=0.0, justify=None):
        props = []
        jline = ('\t\t\t\t(justify %s)\n' % justify) if justify else ''

        def p(name, val, dx, dy, hide, size=1.27, show_name='no', j=''):
            props.append(
                '\t\t(property "%s" "%s"\n\t\t\t(at %s %s 0)\n%s'
                '\t\t\t(show_name %s)\n\t\t\t(do_not_autoplace no)\n'
                '\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size %s %s)\n'
                '\t\t\t\t)\n%s\t\t\t)\n\t\t)'
                % (name, val, n(x + dx), n(y + dy),
                   '\t\t\t(hide yes)\n' if hide else '', show_name,
                   n(size), n(size), j))

        p('Reference', ref, ref_dx, ref_dy, hide_ref, j=jline)
        p('Value', value, val_dx, val_dy, hide_val, j=jline)
        p('Footprint', footprint, 0, 0, True)
        p('Datasheet', datasheet, 0, 0, True)
        p('Description', description, 0, 0, True)
        for nm, vl in extra_props:
            p(nm, vl, 0, 0, True)

        pins = '\n'.join('\t\t(pin "%d"\n\t\t\t(uuid "%s")\n\t\t)'
                         % (k + 1, U('%s.pin%d' % (tag, k + 1)))
                         for k in range(npins))

        self.items.append(
            '\t(symbol\n\t\t(lib_id "%s")\n\t\t(at %s %s %d)\n\t\t(unit 1)\n'
            '\t\t(body_style 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n'
            '\t\t(on_board yes)\n\t\t(in_pos_files yes)\n\t\t(dnp no)\n'
            '\t\t(uuid "%s")\n%s\n%s\n\t\t(instances\n'
            '\t\t\t(project "pantiltslide_integrated"\n\t\t\t\t(path "/%%SHEETUUID%%"\n'
            '\t\t\t\t\t(reference "%s")\n\t\t\t\t\t(unit 1)\n\t\t\t\t)\n'
            '\t\t\t)\n\t\t)\n\t)'
            % (lib_id, n(x), n(y), rot, U(tag), '\n'.join(props), pins, ref))

    def power(self, kind, x, y, tag, rot=0):
        self.pwr += 1
        ref = '#PWR%03d' % self.pwr
        down = (kind == 'GND')
        self.symbol('power:%s' % kind, ref, kind, x, y, tag,
                    npins=1, rot=rot,
                    ref_dy=2.54 if down else -2.54,
                    val_dy=3.6 if down else -3.6,
                    hide_ref=True)

    # ---- output --------------------------------------------------------
    def render(self, path):
        su = str(_uuid.uuid5(NS, 'sheet:' + os.path.basename(path)))
        body = '\n'.join(self.items).replace('%SHEETUUID%', su)
        libs = '\n'.join(self.libs)
        out = (
            '(kicad_sch\n'
            '\t(version 20250114)\n'
            '\t(generator "eeschema")\n'
            '\t(generator_version "9.0")\n'
            '\t(uuid "%s")\n'
            '\t(paper "%s")\n'
            '\t(title_block\n'
            '\t\t(title "%s")\n'
            '\t\t(date "2026-08-24")\n'
            '\t\t(rev "A")\n'
            '\t\t(comment 1 "%s")\n'
            '\t)\n'
            '\t(lib_symbols\n%s\n\t)\n'
            '%s\n'
            '\t(sheet_instances\n\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n\t)\n'
            '\t(embedded_fonts no)\n'
            ')\n' % (su, self.paper, self.title, self.comment, libs, body))
        open(path, 'w', encoding='utf-8').write(out)
        print('wrote', path, len(out), 'bytes')


# --------------------------------------------------------------------------
# shared constants
# --------------------------------------------------------------------------

FP_C0603 = 'Capacitor_SMD:C_0603_1608Metric'
FP_R0603 = 'Resistor_SMD:R_0603_1608Metric'
FP_CP = 'Capacitor_THT:CP_Radial_D8.0mm_P3.50mm'
FP_MOT = 'Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical'
FP_TMC = 'Package_DFN_QFN:VQFN-28-1EP_5x5mm_P0.5mm_EP3.7x3.7mm_ThermalVias'
FP_STICK = 'Module:Pololu_Breakout-16_15.2x20.3mm'
FP_HDR2 = 'Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical'
DS_TMC = ('https://www.analog.com/media/en/technical-documentation/'
          'data-sheets/TMC2209_datasheet_rev1.09.pdf')

# UART slave address per channel -> (MS1/AD0, MS2/AD1)
ADDR = {0: ('GND', 'GND'), 1: ('+3V3', 'GND'),
        2: ('GND', '+3V3'), 3: ('+3V3', '+3V3')}

AXIS = {0: 'Slide', 1: 'Pan', 2: 'Tilt', 3: 'Z'}

# origins must sit on the 1.27 mm (50 mil) grid or every derived endpoint
# trips ERC's endpoint_off_grid check
ORIGIN = {0: (139.7, 111.76), 1: (342.9, 111.76),
          2: (139.7, 266.7), 3: (342.9, 266.7)}


def cap(sh, ref, val, x, y, tag, polar=False, fp=None):
    sh.symbol('Device:C_Polarized' if polar else 'Device:C', ref, val, x, y,
              tag, footprint=fp or (FP_CP if polar else FP_C0603),
              npins=2, ref_dx=2.54, ref_dy=-1.27, val_dx=2.54, val_dy=1.27,
              justify='left')


# ==========================================================================
# variant A -- integrated TMC2209-LA
# ==========================================================================

def build_chip():
    sh = Sheet('PanTilt Slide -- Stepper Drivers (integrated TMC2209-LA)',
               'Four identical TMC2209-LA channels, STEPPER 0..3')
    for nm in ('Connector:Conn_01x04_Pin', 'Device:C', 'Device:C_Polarized',
               'Device:R', 'Driver_Motor:TMC2209-LA', 'power:+24V',
               'power:+3V3', 'power:GND'):
        sh.libs.append(from_sch(nm))

    sh.text('STEPPER DRIVERS  --  4 x TMC2209-LA (integrated)', 30, 30, 4,
            't.title')
    sh.text('Every channel is identical apart from the MS1/MS2 UART slave '
            'address strap.', 30, 38, 2, 't.sub', bold=False)
    sh.text('Sheet pins: STEPn / DIRn / DIAGn per channel, EN + UART shared. '
            'Power crosses the sheet as +24V / +3V3 / GND.', 30, 44, 2,
            't.sub2', bold=False)

    for ch in range(4):
        DX, DY = ORIGIN[ch]
        t = 'c%d' % ch
        base = 100 + ch * 10

        def X(d):
            return DX + d

        def Y(d):
            return DY + d

        sh.text('STEPPER %d  (%s)   UART addr %d' % (ch, AXIS[ch], ch),
                X(-55.88), Y(-58.42), 2.5, t + '.hdr')

        # ---- the driver ------------------------------------------------
        sh.symbol('Driver_Motor:TMC2209-LA', 'U%d' % base, 'TMC2209-LA',
                  DX, DY, t + '.U', footprint=FP_TMC, datasheet=DS_TMC,
                  description='Stepper driver, STEPPER %d (%s)' % (ch, AXIS[ch]),
                  npins=29, ref_dx=15.24, ref_dy=-30.48,
                  val_dx=-15.24, val_dy=30.48)

        # ---- left: step/dir/uart/en/diag -------------------------------
        sh.wire(X(-15.24), Y(-20.32), X(-27.94), Y(-20.32), t + '.w.step')
        sh.hlabel('STEP%d' % ch, 'input', X(-27.94), Y(-20.32), 180, t + '.h.step')

        sh.wire(X(-15.24), Y(-17.78), X(-27.94), Y(-17.78), t + '.w.dir')
        sh.hlabel('DIR%d' % ch, 'input', X(-27.94), Y(-17.78), 180, t + '.h.dir')

        sh.wire(X(-15.24), Y(15.24), X(-27.94), Y(15.24), t + '.w.diag')
        sh.hlabel('DIAG%d' % ch, 'output', X(-27.94), Y(15.24), 180, t + '.h.diag')

        sh.wire(X(-15.24), Y(-7.62), X(-27.94), Y(-7.62), t + '.w.uart')
        sh.wire(X(-15.24), Y(7.62), X(-27.94), Y(7.62), t + '.w.en')
        if ch == 0:
            sh.hlabel('UART', 'bidirectional', X(-27.94), Y(-7.62), 180, t + '.h.uart')
            sh.hlabel('EN', 'input', X(-27.94), Y(7.62), 180, t + '.h.en')
        else:
            sh.label('UART', X(-27.94), Y(-7.62), 180, t + '.l.uart')
            sh.label('EN', X(-27.94), Y(7.62), 180, t + '.l.en')

        # ---- left: strap pins (decreasing stagger, no graphic overlap) --
        sh.wire(X(-15.24), Y(-12.7), X(-20.32), Y(-12.7), t + '.w.clk')
        sh.power('GND', X(-20.32), Y(-12.7), t + '.p.clk')

        ms1, ms2 = ADDR[ch]
        sh.poly([(X(-15.24), Y(-2.54)), (X(-52.07), Y(-2.54)),
                 (X(-52.07), Y(22.86))], t + '.w.ms1')
        sh.power(ms1, X(-52.07), Y(22.86), t + '.p.ms1')
        sh.poly([(X(-15.24), Y(0)), (X(-45.72), Y(0)),
                 (X(-45.72), Y(22.86))], t + '.w.ms2')
        sh.power(ms2, X(-45.72), Y(22.86), t + '.p.ms2')

        sh.wire(X(-15.24), Y(2.54), X(-35.56), Y(2.54), t + '.w.spread')
        sh.power('GND', X(-35.56), Y(2.54), t + '.p.spread')

        sh.wire(X(-15.24), Y(10.16), X(-22.86), Y(10.16), t + '.w.stdby')
        sh.power('GND', X(-22.86), Y(10.16), t + '.p.stdby')

        sh.nc(X(-15.24), Y(17.78), t + '.nc.index')

        # ---- top: VCC_IO / VS / VCP ------------------------------------
        sh.wire(X(-2.54), Y(-27.94), X(-2.54), Y(-46.99), t + '.w.vio')
        sh.power('+3V3', X(-2.54), Y(-46.99), t + '.p.vio')

        sh.wire(DX, Y(-27.94), DX, Y(-40.64), t + '.w.vs')
        sh.power('+24V', DX, Y(-40.64), t + '.p.vs')

        # charge-pump reservoir: 100nF VCP -> VS
        sh.poly([(X(5.08), Y(-27.94)), (X(5.08), Y(-34.29)),
                 (X(12.7), Y(-34.29))], t + '.w.vcp')
        cap(sh, 'C%d' % (base + 3), '100nF', X(12.7), Y(-38.1), t + '.C.vcp')
        sh.wire(X(12.7), Y(-41.91), X(12.7), Y(-44.45), t + '.w.vcp2')
        sh.power('+24V', X(12.7), Y(-44.45), t + '.p.vcp')

        # ---- VS bulk + hf decoupling, VIO decoupling -------------------
        for dx, ref, val, polar, tg in (
                (-20.32, base + 0, '100uF_35V', True, 'bulk'),
                (-35.56, base + 1, '100nF', False, 'vshf'),
                (-50.8, base + 2, '100nF', False, 'viohf')):
            cap(sh, 'C%d' % ref, val, X(dx), Y(-38.1), t + '.C.' + tg,
                polar=polar)
            sh.wire(X(dx), Y(-41.91), X(dx), Y(-44.45), t + '.w.' + tg + 'a')
            sh.power('+3V3' if tg == 'viohf' else '+24V', X(dx), Y(-44.45),
                     t + '.p.' + tg)
            sh.wire(X(dx), Y(-34.29), X(dx), Y(-31.75), t + '.w.' + tg + 'b')
            sh.power('GND', X(dx), Y(-31.75), t + '.g.' + tg)

        # ---- bottom: GND paddle ----------------------------------------
        sh.wire(DX, Y(25.4), DX, Y(30.48), t + '.w.gnd')
        sh.power('GND', DX, Y(30.48), t + '.p.gnd')
        sh.nc(X(2.54), Y(25.4), t + '.nc.pin25')

        # ---- right: 5VOUT ----------------------------------------------
        sh.poly([(X(15.24), Y(-20.32)), (X(25.4), Y(-20.32)),
                 (X(25.4), Y(-24.13))], t + '.w.5v')
        cap(sh, 'C%d' % (base + 4), '4.7uF', X(25.4), Y(-27.94), t + '.C.5v')
        sh.wire(X(25.4), Y(-31.75), X(25.4), Y(-36.83), t + '.w.5vb')
        sh.power('GND', X(25.4), Y(-36.83), t + '.g.5v')

        # VREF unused: current set over UART / internal reference
        sh.nc(X(15.24), Y(-15.24), t + '.nc.vref')

        # ---- right: charge pump cap CPO<->CPI --------------------------
        sh.poly([(X(15.24), Y(-10.16)), (X(24.13), Y(-10.16)),
                 (X(24.13), Y(-11.43)), (X(34.29), Y(-11.43))], t + '.w.cpo')
        sh.poly([(X(15.24), Y(-5.08)), (X(24.13), Y(-5.08)),
                 (X(24.13), Y(-3.81)), (X(34.29), Y(-3.81))], t + '.w.cpi')
        cap(sh, 'C%d' % (base + 5), '22nF', X(34.29), Y(-7.62), t + '.C.cp')

        # ---- right: motor phases ---------------------------------------
        for dy, nmm, tg in ((0, 'A1', 'oa1'), (2.54, 'A2', 'oa2'),
                            (7.62, 'B1', 'ob1'), (10.16, 'B2', 'ob2')):
            sh.wire(X(15.24), Y(dy), X(22.86), Y(dy), t + '.w.' + tg)
            sh.label('M%d_%s' % (ch, nmm), X(22.86), Y(dy), 0, t + '.l.' + tg)

        # ---- right: sense resistors ------------------------------------
        for dy, dx, ref, tg in ((15.24, 40.64, base + 0, 'rsa'),
                                (17.78, 25.4, base + 1, 'rsb')):
            sh.wire(X(15.24), Y(dy), X(dx), Y(dy), t + '.w.' + tg)
            sh.wire(X(dx), Y(dy), X(dx), Y(19.05), t + '.w.' + tg + '2')
            sh.symbol('Device:R', 'R%d' % ref, '0.11', X(dx), Y(22.86),
                      t + '.R.' + tg, footprint=FP_R0603, npins=2,
                      ref_dx=2.54, ref_dy=-1.27, val_dx=2.54, val_dy=1.27,
                      justify='left')
            sh.wire(X(dx), Y(26.67), X(dx), Y(29.21), t + '.w.' + tg + '3')
            sh.power('GND', X(dx), Y(29.21), t + '.g.' + tg)

        # ---- motor connector -------------------------------------------
        motor_connector(sh, ch, X(53.34), Y(5.08), t, 100 + ch, -2.54)

    sh.render(os.path.join(OUT, 'tmc2209_driver_chip.kicad_sch'))


def motor_connector(sh, ch, x, y, t, jnum, first_dy):
    sh.symbol('Connector:Conn_01x04_Pin', 'J%d' % jnum,
              'Motor_%s' % AXIS[ch], x, y, t + '.J', footprint=FP_MOT,
              npins=4, ref_dy=-10.16, val_dy=10.16)
    for k, nmm in enumerate(('A1', 'A2', 'B1', 'B2')):
        py = y + first_dy + 2.54 * k
        sh.wire(x + 5.08, py, x + 12.7, py, '%s.w.j%d' % (t, k))
        sh.label('M%d_%s' % (ch, nmm), x + 12.7, py, 0, '%s.l.j%d' % (t, k))


# ==========================================================================
# variant B -- plug-in StepStick / SilentStepStick modules
# ==========================================================================

STICK_NOTE = (
    'Socket = Pololu / StepStick 2x8 outline, drawn with the A4988 legacy pin '
    'names. TMC2209 SilentStepStick function by pin number: 9=EN  10=MS1/AD0  '
    '11=MS2/AD1  12=SPREAD  13,14=no pin on the module  15=STEP  16=DIR  '
    '2=VIO  8=VM  1,7=GND  3=OB2  4=OB1  5=OA1  6=OA2.')


def build_stick():
    sh = Sheet('PanTilt Slide -- Stepper Drivers (plug-in TMC2209 StepStick)',
               'Four identical StepStick sockets, STEPPER 0..3')
    for nm in ('Connector:Conn_01x02_Pin', 'Connector:Conn_01x04_Pin',
               'Device:C', 'Device:C_Polarized',
               'power:+24V', 'power:+3V3', 'power:GND'):
        sh.libs.append(from_sch(nm))
    sh.libs.append(from_lib('Pololu_Breakout_A4988', 'Driver_Motor'))

    sh.text('STEPPER DRIVERS  --  4 x plug-in TMC2209 StepStick', 30, 30, 4,
            't.title')
    sh.text('Drop-in alternative to tmc2209_driver_chip.kicad_sch -- identical '
            'sheet pins.', 30, 38, 2, 't.sub', bold=False)
    sh.text(STICK_NOTE, 30, 44, 2, 't.sub2', bold=False)
    sh.text('Set motor current with the module trimpot (VREF); the module '
            'carries its own sense resistors, charge pump and 5V regulator.',
            30, 50, 2, 't.sub3', bold=False)

    for ch in range(4):
        DX, DY = ORIGIN[ch]
        t = 's%d' % ch
        base = 100 + ch * 10

        def X(d):
            return DX + d

        def Y(d):
            return DY + d

        sh.text('STEPPER %d  (%s)   UART addr %d' % (ch, AXIS[ch], ch),
                X(-48.26), Y(-42), 2.5, t + '.hdr')

        sh.symbol('Driver_Motor:Pololu_Breakout_A4988', 'A%d' % base,
                  'TMC2209_StepStick', DX, DY, t + '.A', footprint=FP_STICK,
                  datasheet=DS_TMC,
                  description='Plug-in TMC2209 StepStick, STEPPER %d (%s)'
                              % (ch, AXIS[ch]),
                  npins=16, ref_dx=20.32, ref_dy=-25.4,
                  val_dx=20.32, val_dy=-22.86)

        # ---- left: EN(9) STEP(15) DIR(16) ------------------------------
        # Pins 13/14 are the A4988 RESET/SLEEP positions.  On the Watterott
        # SilentStepStick TMC2209 they are not bonded to anything (verified
        # against the board's own layout), so PDN_UART and DIAG can only be
        # reached over the flying-lead header below.
        sh.nc(X(-10.16), Y(-10.16), t + '.nc.p13')
        sh.nc(X(-10.16), Y(-7.62), t + '.nc.p14')
        sh.wire(X(-10.16), Y(-2.54), X(-22.86), Y(-2.54), t + '.w.en')
        if ch == 0:
            sh.hlabel('EN', 'input', X(-22.86), Y(-2.54), 180, t + '.h.en')
        else:
            sh.label('EN', X(-22.86), Y(-2.54), 180, t + '.l.en')

        sh.wire(X(-10.16), Y(0), X(-22.86), Y(0), t + '.w.step')
        sh.hlabel('STEP%d' % ch, 'input', X(-22.86), Y(0), 180, t + '.h.step')

        sh.wire(X(-10.16), Y(2.54), X(-22.86), Y(2.54), t + '.w.dir')
        sh.hlabel('DIR%d' % ch, 'input', X(-22.86), Y(2.54), 180, t + '.h.dir')

        # ---- left: address / mode straps -------------------------------
        ms1, ms2 = ADDR[ch]
        sh.poly([(X(-10.16), Y(7.62)), (X(-45.72), Y(7.62)),
                 (X(-45.72), Y(20.32))], t + '.w.ms1')
        sh.power(ms1, X(-45.72), Y(20.32), t + '.p.ms1')
        sh.poly([(X(-10.16), Y(10.16)), (X(-38.1), Y(10.16)),
                 (X(-38.1), Y(20.32))], t + '.w.ms2')
        sh.power(ms2, X(-38.1), Y(20.32), t + '.p.ms2')
        sh.wire(X(-10.16), Y(12.7), X(-25.4), Y(12.7), t + '.w.spread')
        sh.power('GND', X(-25.4), Y(12.7), t + '.p.spread')

        # ---- UART / DIAG flying-lead header ----------------------------
        jx, jy = X(20.32), Y(25.4)
        sh.symbol('Connector:Conn_01x02_Pin', 'J%d' % (110 + ch),
                  'PDN_DIAG_%d' % ch, jx, jy, t + '.Jd', footprint=FP_HDR2,
                  npins=2, ref_dy=-7.62, val_dy=7.62,
                  description='Jumper to the module PDN_UART and DIAG pads')
        sh.wire(jx + 5.08, jy, jx + 12.7, jy, t + '.w.juart')
        sh.wire(jx + 5.08, jy + 2.54, jx + 12.7, jy + 2.54, t + '.w.jdiag')
        if ch == 0:
            sh.hlabel('UART', 'bidirectional', jx + 12.7, jy, 0, t + '.h.uart')
        else:
            sh.label('UART', jx + 12.7, jy, 0, t + '.l.uart')
        sh.hlabel('DIAG%d' % ch, 'output', jx + 12.7, jy + 2.54, 0,
                  t + '.h.diag')
        sh.text('1 -> module PDN_UART pad   2 -> module DIAG pad',
                jx - 2.54, jy + 12.7, 1.5, t + '.n.jd', bold=False)

        # ---- top: VIO / VMOT -------------------------------------------
        sh.wire(DX, Y(-17.78), DX, Y(-25.4), t + '.w.vio')
        sh.power('+3V3', DX, Y(-25.4), t + '.p.vio')
        sh.wire(X(5.08), Y(-17.78), X(5.08), Y(-30.48), t + '.w.vm')
        sh.power('+24V', X(5.08), Y(-30.48), t + '.p.vm')

        for dx, ref, val, polar, rail, tg in (
                (-15.24, base + 0, '100uF_35V', True, '+24V', 'bulk'),
                (-30.48, base + 1, '100nF', False, '+24V', 'vmhf'),
                (-45.72, base + 2, '100nF', False, '+3V3', 'viohf')):
            cap(sh, 'C%d' % ref, val, X(dx), Y(-25.4), t + '.C.' + tg,
                polar=polar)
            sh.wire(X(dx), Y(-29.21), X(dx), Y(-31.75), t + '.w.' + tg + 'a')
            sh.power(rail, X(dx), Y(-31.75), t + '.p.' + tg)
            sh.wire(X(dx), Y(-21.59), X(dx), Y(-19.05), t + '.w.' + tg + 'b')
            sh.power('GND', X(dx), Y(-19.05), t + '.g.' + tg)

        # ---- bottom: the two GND pins ----------------------------------
        sh.wire(DX, Y(20.32), DX, Y(25.4), t + '.w.gnd1')
        sh.power('GND', DX, Y(25.4), t + '.p.gnd1')
        sh.wire(X(5.08), Y(20.32), X(5.08), Y(30.48), t + '.w.gnd2')
        sh.power('GND', X(5.08), Y(30.48), t + '.p.gnd2')

        # ---- right: motor phases ---------------------------------------
        # A4988 pad -> SilentStepStick TMC2209 output, taken row-for-row from
        # the Watterott board layout: 1B=OB2, 1A=OB1, 2A=OA1, 2B=OA2.
        for dy, nmm, tg in ((-2.54, 'B2', '1b'), (0, 'B1', '1a'),
                            (2.54, 'A1', '2a'), (5.08, 'A2', '2b')):
            sh.wire(X(12.7), Y(dy), X(20.32), Y(dy), t + '.w.' + tg)
            sh.label('M%d_%s' % (ch, nmm), X(20.32), Y(dy), 0, t + '.l.' + tg)

        motor_connector(sh, ch, X(53.34), Y(2.54), t, 100 + ch, -2.54)

    sh.render(os.path.join(OUT, 'tmc2209_driver_stepstick.kicad_sch'))


build_chip()
_seen.clear()
build_stick()
