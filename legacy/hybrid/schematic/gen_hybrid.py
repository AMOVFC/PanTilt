"""Generate the PanTilt *hybrid* controller schematic.

Board split: the ESP32-S3 module and the whole power chain are soldered down;
everything else plugs in. The four TMC2209s are SilentStepStick modules in
sockets and the TCA9548A is an off-the-shelf breakout on an 8-pin header,
rather than a QFN28 and a TSSOP24 placed by the assembly house.

Reuses three sheets verbatim from the integrated design -- esp32, power and
usb -- plus its already-drawn stepstick driver sheet. The only new sheet is
the I2C / IO one, which swaps the soldered mux for a module connector.

Two things this fixes relative to the integrated root sheet:

  * The driver sheet exposes 14 hierarchical labels (STEP0-3, DIR0-3,
    DIAG0-3, EN, UART) but the integrated root's sheet symbol carries no
    matching pins, so those nets are dangling -- the drivers are not actually
    connected to the MCU. This root declares all 14 pins and wires them to the
    ESP32's global labels.
  * DIAG0-3 are deliberately terminated with no-connects. They are broken out
    to the PDN_DIAG headers inside the driver sheet for optional stall
    detection; nothing at board level consumes them, and saying so explicitly
    beats leaving them floating.

Pin assignment follows include/config.h (the firmware's authoritative map),
not the older carrier board's connector layout.
"""

import os
import uuid as _uuid

NS = _uuid.UUID('7c9e2a10-5b3d-4f81-9a26-0d4c8e77b1aa')
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', '..', 'integrated', 'schematic',
                                    'I2C-IO.kicad_sch'))  # donor for lib_symbols
KLIB = r"C:\Program Files\KiCad\10.0\share\kicad\symbols\Connector.kicad_sym"
PROJECT = 'pantiltslide_hybrid'

_seen = set()


def U(tag):
    """Deterministic uuid so regenerating produces a stable diff."""
    assert tag not in _seen, 'duplicate uuid tag ' + tag
    _seen.add(tag)
    return str(_uuid.uuid5(NS, tag))


ROOT_UUID = U('root')
SH_I2C = U('sheet/i2c')
SH_ESP = U('sheet/esp32')
SH_PWR = U('sheet/power')
SH_USB = U('sheet/usb')
SH_DRV = U('sheet/drivers')


# ---------------------------------------------------------------------------
# lib_symbol extraction
# ---------------------------------------------------------------------------

def _balanced(text, start):
    depth, i, instr = 0, start, False
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
    raise ValueError('unbalanced s-expression')


def lib_from(path, name):
    """Pull one (symbol "<name>" ...) block out of a .kicad_sch or .kicad_sym."""
    s = open(path, encoding='utf-8').read()
    key = '(symbol "%s"' % name
    i = s.find(key)
    if i < 0:
        raise KeyError('%s not found in %s' % (name, path))
    return _balanced(s, i)


def lib_from_klib(name, lib='Connector'):
    """System library symbols are stored unqualified; re-qualify the name."""
    blk = lib_from(KLIB, name)
    return blk.replace('(symbol "%s"' % name, '(symbol "%s:%s"' % (lib, name), 1)


# Pin geometry, parsed straight out of the symbol definition so wire endpoints
# can never drift from the artwork.
def pin_geometry(libblk):
    """{number: (local_x, local_y, angle)} for every pin in a lib_symbol."""
    out = {}
    import re
    for m in re.finditer(
            r'\(pin\s+\w+\s+\w+\s*\n\s*\(at\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(\d+)\)'
            r'.*?\(number "([^"]+)"', libblk, __import__('re').S):
        out[m.group(4)] = (float(m.group(1)), float(m.group(2)), int(m.group(3)))
    return out


GRID = 1.27


def G(v):
    """Snap to KiCad's 1.27mm connection grid.

    Every derived offset in this file (pin pitch 2.54, stubs 6.35/7.62) is
    already a grid multiple, so snapping the placement origins is enough to
    keep every wire endpoint on grid.
    """
    return round(round(float(v) / GRID) * GRID, 4)


def n(v):
    """Trim floats the way KiCad writes them."""
    s = ('%f' % float(v)).rstrip('0').rstrip('.')
    return s if s not in ('', '-0') else '0'


# Wire leaves the pin in the direction the pin body points away from the symbol.
OUTWARD = {0: (-1.0, 0.0), 180: (1.0, 0.0), 90: (0.0, 1.0), 270: (0.0, -1.0)}
LABEL_STYLE = {0: (180, 'right'), 180: (0, 'left'), 90: (270, 'right'), 270: (90, 'left')}


class Sheet:
    def __init__(self, title, paper='A2', comment=''):
        self.title = title
        self.comment = comment
        self.paper = paper
        self.body = []
        self.libs = {}

    def lib(self, lib_id, blk):
        self.libs[lib_id] = blk

    def emit(self, s):
        self.body.append(s)

    def wire(self, x1, y1, x2, y2, tag):
        self.emit('\t(wire\n\t\t(pts (xy %s %s) (xy %s %s))\n'
                  '\t\t(stroke (width 0) (type default))\n\t\t(uuid "%s")\n\t)\n'
                  % (n(x1), n(y1), n(x2), n(y2), U(tag)))

    def glabel(self, text, x, y, angle, justify, tag, shape='input'):
        self.emit('\t(global_label "%s"\n\t\t(shape %s)\n\t\t(at %s %s %d)\n'
                  '\t\t(effects (font (size 1.27 1.27)) (justify %s))\n\t\t(uuid "%s")\n'
                  '\t\t(property "Intersheetrefs" "${INTERSHEET_REFS}"\n'
                  '\t\t\t(at %s %s 0)\n\t\t\t(hide yes)\n\t\t\t(show_name no)\n'
                  '\t\t\t(do_not_autoplace no)\n'
                  '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n\t)\n'
                  % (text, shape, n(x), n(y), angle, justify, U(tag), n(x), n(y)))

    def no_connect(self, x, y, tag):
        self.emit('\t(no_connect (at %s %s) (uuid "%s"))\n' % (n(x), n(y), U(tag)))

    def text(self, s, x, y, tag, size=2.0, bold=True):
        self.emit('\t(text "%s"\n\t\t(exclude_from_sim no)\n\t\t(at %s %s 0)\n'
                  '\t\t(effects (font (size %s %s) %s) (justify left bottom))\n'
                  '\t\t(uuid "%s")\n\t)\n'
                  % (s, n(x), n(y), n(size), n(size),
                     '(bold yes)' if bold else '', U(tag)))

    def power(self, kind, x, y, tag, rot=0):
        # GND draws downward, +3V3 upward; put each one's text on the
        # far side of its own symbol so it never lands back among the
        # signal labels it just left.
        text_dy = 5.5 if kind == 'GND' else -5.5
        ref = '#PWR%s' % (len([b for b in self.body if '#PWR' in b]) + 1)
        self.emit('\t(symbol\n\t\t(lib_id "power:%s")\n\t\t(at %s %s %d)\n\t\t(unit 1)\n'
                  '\t\t(exclude_from_sim no)\n\t\t(in_bom no)\n\t\t(on_board yes)\n'
                  '\t\t(in_pos_files no)\n\t\t(dnp no)\n\t\t(fields_autoplaced yes)\n'
                  '\t\t(uuid "%s")\n'
                  '\t\t(property "Reference" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(hide yes)\n'
                  '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n'
                  '\t\t(property "Value" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
                  '\t\t\t(do_not_autoplace no)\n'
                  '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n'
                  '\t\t(pin "1" (uuid "%s"))\n'
                  '\t\t(instances\n\t\t\t(project "%s"\n\t\t\t\t(path "/%s/%s"\n'
                  '\t\t\t\t\t(reference "%s")\n\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n'
                  % (kind, n(x), n(y), rot, U(tag), ref, n(x), n(y + text_dy * 0.45), kind,
                     n(x), n(y + text_dy), U(tag + '/p1'), PROJECT, ROOT_UUID, SH_I2C, ref))

    def symbol(self, lib_id, ref, value, x, y, tag, footprint='', pins=(),
               ref_dy=-7.0, val_dy=-4.5, rot=0):
        L = ['\t(symbol\n\t\t(lib_id "%s")\n\t\t(at %s %s %d)\n\t\t(unit 1)\n'
             '\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n'
             '\t\t(in_pos_files yes)\n\t\t(dnp no)\n\t\t(fields_autoplaced yes)\n'
             '\t\t(uuid "%s")\n' % (lib_id, n(x), n(y), rot, U(tag))]
        L.append('\t\t(property "Reference" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
                 '\t\t\t(do_not_autoplace no)\n'
                 '\t\t\t(effects (font (size 1.27 1.27)) (justify left))\n\t\t)\n'
                 % (ref, n(x), n(y + ref_dy)))
        L.append('\t\t(property "Value" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
                 '\t\t\t(do_not_autoplace no)\n'
                 '\t\t\t(effects (font (size 1.27 1.27)) (justify left))\n\t\t)\n'
                 % (value, n(x), n(y + val_dy)))
        for k, v in (('Footprint', footprint), ('Datasheet', ''), ('Description', '')):
            L.append('\t\t(property "%s" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
                     '\t\t\t(do_not_autoplace no)\n\t\t\t(hide yes)\n'
                     '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n'
                     % (k, v, n(x), n(y)))
        for p in pins:
            L.append('\t\t(pin "%s" (uuid "%s"))\n' % (p, U(tag + '/pin' + str(p))))
        L.append('\t\t(instances\n\t\t\t(project "%s"\n\t\t\t\t(path "/%s/%s"\n'
                 '\t\t\t\t\t(reference "%s")\n\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n'
                 % (PROJECT, ROOT_UUID, SH_I2C, ref))
        self.emit(''.join(L))

    def render(self, path, uuid_):
        out = ['(kicad_sch\n\t(version 20260306)\n\t(generator "gen_hybrid")\n'
               '\t(generator_version "10.0")\n\t(uuid "%s")\n\t(paper "%s")\n'
               % (uuid_, self.paper)]
        out.append('\t(title_block\n\t\t(title "%s")\n\t\t(rev "A")\n' % self.title)
        if self.comment:
            out.append('\t\t(comment 1 "%s")\n' % self.comment)
        out.append('\t)\n')
        out.append('\t(lib_symbols\n')
        for _, blk in sorted(self.libs.items()):
            out.append('\t\t' + blk.replace('\n', '\n\t\t') + '\n')
        out.append('\t)\n')
        out.extend(self.body)
        out.append('\t(sheet_instances\n\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n\t)\n')
        out.append('\t(embedded_fonts no)\n)\n')
        open(path, 'w', encoding='utf-8').write(''.join(out))
        print('wrote', os.path.basename(path))


# ---------------------------------------------------------------------------
# Sheet 1 -- I2C / IO, with the mux as a plug-in module
# ---------------------------------------------------------------------------

CONN4_FP = 'Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical'
CONN2_FP = 'Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical'
CONN8_FP = 'Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical'
R_FP = 'Resistor_SMD:R_0603_1608Metric'


def build_i2c_io():
    sh = Sheet('PanTilt Hybrid -- I2C and IO (plug-in mux)', paper='A3')
    sh.lib('Connector:Conn_01x02_Pin', lib_from(SRC, 'Connector:Conn_01x02_Pin'))
    sh.lib('Connector:Conn_01x04_Pin', lib_from(SRC, 'Connector:Conn_01x04_Pin'))
    sh.lib('Connector:Conn_01x08_Pin', lib_from_klib('Conn_01x08_Pin'))
    sh.lib('Device:R', lib_from(SRC, 'Device:R'))
    sh.lib('power:+3V3', lib_from(SRC, 'power:+3V3'))
    sh.lib('power:GND', lib_from(SRC, 'power:GND'))

    geo = {k: pin_geometry(sh.libs[k]) for k in
           ('Connector:Conn_01x02_Pin', 'Connector:Conn_01x04_Pin',
            'Connector:Conn_01x08_Pin')}

    sh.text('I2C bus A (muxed) -- pan / tilt AS5600', 25, 25, 't/hdr1', size=2.5)
    sh.text('TCA9548A is an off-the-shelf breakout on J200, not a soldered '
            'TSSOP24.', 25, 30, 't/hdr1b', size=1.7, bold=False)
    sh.text('I2C bus B, encoders, limit switches', 175, 25, 't/hdr2', size=2.5)
    sh.text('Pull-ups here serve the mux input and bus B; the AS5600 and OLED '
            'breakouts carry their own.', 175, 30, 't/hdr2b', size=1.7, bold=False)

    # Stub lengths are staggered by net class: the +3V3 riser leaves from the
    # top pin, and the GND drop is taken far enough right to clear the widest
    # net label (I2C_TILT_SCL) rather than running down across it.
    SIG, PWR_H, GND_H, VERT = 7.62, 11.43, 25.4, 6.35

    def place(lib_id, ref, value, x, y, nets, fp, tag):
        x, y = G(x), G(y)
        pins = sorted(geo[lib_id], key=int)
        half = 2.54 * len(pins) / 2
        # Reference and value sit above and to the LEFT; every wire leaves to
        # the right, so nothing can collide with them.
        sh.symbol(lib_id, ref, value, x, y, tag, footprint=fp, pins=pins,
                  ref_dy=-(half + 5.08), val_dy=-(half + 2.54))
        for pn in pins:
            lx, ly, ang = geo[lib_id][pn]
            px, py = G(x + lx), G(y - ly)
            net = nets.get(int(pn))
            if net is None:
                continue
            if net == '+3V3':
                sh.wire(px, py, px + PWR_H, py, tag + '/w' + pn)
                sh.wire(px + PWR_H, py, px + PWR_H, py - VERT, tag + '/wv' + pn)
                sh.power('+3V3', px + PWR_H, py - VERT, tag + '/pwr' + pn)
            elif net == 'GND':
                sh.wire(px, py, px + GND_H, py, tag + '/w' + pn)
                sh.wire(px + GND_H, py, px + GND_H, py + VERT, tag + '/wv' + pn)
                sh.power('GND', px + GND_H, py + VERT, tag + '/pwr' + pn)
            else:
                sh.wire(px, py, px + SIG, py, tag + '/w' + pn)
                sh.glabel(net, px + SIG, py, 0, 'left', tag + '/l' + pn)

    # --- bus A: mux module and the two AS5600 heads ----------------------
    # J200's pinout deliberately matches the carrier board's J20, so the same
    # breakout and the same harness work on either board.
    place('Connector:Conn_01x08_Pin', 'J200', 'TCA9548A_Mux', 45, 55,
          {1: '+3V3', 2: 'GND', 3: 'GPIO11', 4: 'GPIO12',
           5: 'I2C_PAN_SDA', 6: 'I2C_PAN_SCL',
           7: 'I2C_TILT_SDA', 8: 'I2C_TILT_SCL'}, CONN8_FP, 's/j200')
    place('Connector:Conn_01x04_Pin', 'J201', 'AS5600_Pan', 45, 100,
          {1: '+3V3', 2: 'GND', 3: 'I2C_PAN_SDA', 4: 'I2C_PAN_SCL'},
          CONN4_FP, 's/j201')
    place('Connector:Conn_01x04_Pin', 'J202', 'AS5600_Tilt', 45, 130,
          {1: '+3V3', 2: 'GND', 3: 'I2C_TILT_SDA', 4: 'I2C_TILT_SCL'},
          CONN4_FP, 's/j202')

    # --- bus B, encoders, limits -----------------------------------------
    place('Connector:Conn_01x04_Pin', 'J203', 'OLED', 195, 55,
          {1: '+3V3', 2: 'GND', 3: 'GPIO13', 4: 'GPIO14'}, CONN4_FP, 's/j203')
    place('Connector:Conn_01x04_Pin', 'J204', 'Enc_Jog', 195, 90,
          {1: 'GPIO15', 2: 'GPIO16', 3: 'GND', 4: 'GPIO17'}, CONN4_FP, 's/j204')
    place('Connector:Conn_01x04_Pin', 'J205', 'Enc_Angle', 195, 120,
          {1: 'GPIO18', 2: 'GPIO21', 3: 'GND', 4: 'GPIO38'}, CONN4_FP, 's/j205')
    place('Connector:Conn_01x02_Pin', 'J206', 'Limit_Min', 195, 150,
          {1: 'GPIO39', 2: 'GND'}, CONN2_FP, 's/j206')
    place('Connector:Conn_01x02_Pin', 'J207', 'Limit_Max', 195, 172,
          {1: 'GPIO40', 2: 'GND'}, CONN2_FP, 's/j207')
    place('Connector:Conn_01x02_Pin', 'J208', 'Limit_Z_Home', 195, 194,
          {1: 'GPIO42', 2: 'GND'}, CONN2_FP, 's/j208')

    # --- pull-ups, sat next to the bus each one serves --------------------
    rgeo = pin_geometry(sh.libs['Device:R'])
    for ref, net, x, y in [('R200', 'GPIO11', 105, 60), ('R201', 'GPIO12', 120, 60),
                           ('R202', 'GPIO13', 255, 60), ('R203', 'GPIO14', 270, 60)]:
        x, y = G(x), G(y)
        tag = 's/' + ref.lower()
        sh.symbol('Device:R', ref, '4.7k', x, y, tag, footprint=R_FP,
                  pins=['1', '2'], ref_dy=-1.27, val_dy=3.81, rot=0)
        tx, ty = G(x + rgeo['1'][0]), G(y - rgeo['1'][1])
        bx, by = G(x + rgeo['2'][0]), G(y - rgeo['2'][1])
        sh.wire(tx, ty, tx, ty - VERT, tag + '/wt')
        sh.power('+3V3', tx, ty - VERT, tag + '/pwr')
        sh.wire(bx, by, bx, by + VERT, tag + '/wb')
        sh.glabel(net, bx, by + VERT, 0, 'left', tag + '/l')

    sh.render(os.path.join(HERE, 'i2c_io_plugin.kicad_sch'), SH_I2C)


# ---------------------------------------------------------------------------
# Sheet 0 -- root
# ---------------------------------------------------------------------------

# Driver sheet interface -> ESP32 global label, per include/config.h.
DRIVER_MAP = [
    ('STEP0', 'input', 'GPIO4'),    # slide
    ('DIR0', 'input', 'GPIO5'),
    ('STEP1', 'input', 'GPIO6'),    # pan
    ('DIR1', 'input', 'GPIO7'),
    ('STEP2', 'input', 'GPIO8'),    # tilt
    ('DIR2', 'input', 'GPIO9'),
    ('STEP3', 'input', 'GPIO1'),    # z
    ('DIR3', 'input', 'GPIO2'),
    ('EN', 'input', 'GPIO10'),      # shared, active low
    ('UART', 'bidirectional', 'TMC_UART'),
    ('DIAG0', 'output', None),      # -> PDN_DIAG headers only
    ('DIAG1', 'output', None),
    ('DIAG2', 'output', None),
    ('DIAG3', 'output', None),
]


def sheet_block(name, filename, x, y, w, h, uuid_, page, pins=()):
    x, y, w, h = G(x), G(y), G(w), G(h)
    L = ['\t(sheet\n\t\t(at %s %s)\n\t\t(size %s %s)\n'
         '\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(dnp no)\n'
         '\t\t(fields_autoplaced yes)\n'
         '\t\t(stroke (width 0.1524) (type solid))\n\t\t(fill (color 0 0 0 0))\n'
         '\t\t(uuid "%s")\n' % (n(x), n(y), n(w), n(h), uuid_)]
    L.append('\t\t(property "Sheetname" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
             '\t\t\t(do_not_autoplace no)\n'
             '\t\t\t(effects (font (size 1.27 1.27)) (justify left bottom))\n\t\t)\n'
             % (name, n(x), n(y - 0.7116)))
    L.append('\t\t(property "Sheetfile" "%s"\n\t\t\t(at %s %s 0)\n\t\t\t(show_name no)\n'
             '\t\t\t(do_not_autoplace no)\n'
             '\t\t\t(effects (font (size 1.27 1.27)) (justify left top))\n\t\t)\n'
             % (filename, n(x), n(y + h + 0.7146)))
    for pname, shape, px, py in pins:
        L.append('\t\t(pin "%s" %s\n\t\t\t(at %s %s 180)\n\t\t\t(uuid "%s")\n'
                 '\t\t\t(effects (font (size 1.27 1.27)) (justify right))\n\t\t)\n'
                 % (pname, shape, n(px), n(py), U('rootpin/' + pname)))
    L.append('\t\t(instances\n\t\t\t(project "%s"\n\t\t\t\t(path "/%s"\n'
             '\t\t\t\t\t(page "%s")\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n'
             % (PROJECT, ROOT_UUID, page))
    return ''.join(L)


def build_root():
    sh = Sheet('PanTilt Slide -- Hybrid Controller (integrated MCU + power, '
               'plug-in drivers and mux)', paper='A3')

    sh.text('HYBRID CONTROLLER', 22, 26, 't/title', size=4.5)
    sh.text('Soldered down:  ESP32-S3-WROOM-1-N16R8, 24V->5V buck, 5V->3V3 LDO, '
            'USB-C.', 22, 33, 't/l1', size=2.0, bold=False)
    sh.text('Plug-in:  4x TMC2209 SilentStepStick, TCA9548A breakout, and every '
            'motor, sensor and switch.', 22, 38, 't/l2', size=2.0, bold=False)

    # Sheets that speak only in global labels need no pins.
    sh.emit(sheet_block('power', 'power.kicad_sch', 22, 50, 85, 48, SH_PWR, '2'))
    sh.emit(sheet_block('usb', 'usb.kicad_sch', 22, 108, 85, 32, SH_USB, '3'))
    sh.emit(sheet_block('esp32', 'esp32.kicad_sch', 22, 150, 85, 48, SH_ESP, '4'))
    sh.emit(sheet_block('I2C-IO', 'i2c_io_plugin.kicad_sch', 120, 150, 95, 48,
                        SH_I2C, '6'))

    # Driver sheet: 14 pins down the left edge, each wired out to the matching
    # ESP32 global label (or a no-connect for the unused DIAG taps).
    x0, y0, w, h = G(258), G(50), G(120), G(96)
    pitch = 6.35
    top = G(y0 + 7.62)
    pins = [(pname, shape, x0, G(top + i * pitch))
            for i, (pname, shape, _n) in enumerate(DRIVER_MAP)]
    sh.emit(sheet_block('stepper drivers', 'tmc2209_driver_stepstick.kicad_sch',
                        x0, y0, w, h, SH_DRV, '5', pins))

    for i, (pname, _shape, net) in enumerate(DRIVER_MAP):
        py = G(top + i * pitch)
        ex = G(x0 - 25.4)
        sh.wire(x0, py, ex, py, 'root/w/' + pname)
        if net is None:
            sh.no_connect(ex, py, 'root/nc/' + pname)
        else:
            sh.glabel(net, ex, py, 180, 'right', 'root/l/' + pname,
                      shape='bidirectional' if net == 'TMC_UART' else 'output')

    sh.text('Driver interface -- wired to the firmware pin map in '
            'include/config.h', G(x0 - 25.4), y0 - 3, 't/drv', size=1.7, bold=False)
    sh.text('DIAG0-3 go to the PDN_DIAG headers inside the driver sheet; '
            'unused at board level.', G(x0 - 25.4), y0 + h + 6, 't/diag',
            size=1.7, bold=False)
    sh.text('Slide=0  Pan=1  Tilt=2  Z=3   (MS1/MS2 strap the UART address)',
            G(x0 - 25.4), y0 + h + 11, 't/addr', size=1.7, bold=False)

    sh.render(os.path.join(HERE, PROJECT + '.kicad_sch'), ROOT_UUID)


build_i2c_io()
build_root()
print('done')
