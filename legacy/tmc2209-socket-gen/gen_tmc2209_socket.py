"""Generate the reusable TMC2209 SilentStepStick *socket* part for PanTilt.

Three artefacts, all written to the repo root so any of the boards can pull
them in:

    TMC2209_Socket.kicad_sym                            -- schematic symbol
    TMC2209_Socket.pretty/TMC2209_StepStick_Socket.kicad_mod  -- footprint
    tmc2209_socket.kicad_sch                            -- one-channel sheet

Geometry and pin names are lifted from Watterott's own KiCad project for the
SilentStepStick TMC2209 v2.0 (vendored under
malloc-19-SilentStepStickKiCad-ed32888/), not from the generic A4988 StepStick
symbol the rest of this repo has been using.  From that board:

    board outline   15.24 x 20.32 mm, headers 12.7 mm apart (+-6.35 from centre)
    J1 (logic)  1..8 = DIR STEP PDN_UART PDN_UART SPREAD MS2 MS1 EN
    J2 (power)  1..8 = VM GND OA2 OB1... -> VM GND M2B M2A M1A M1B VIO GND
    J3 + REF1   DIAG / INDEX on the EN..VM row, VREF 1.651 mm inboard

Pads 3 and 4 of J1 are the *two* PDN_UART positions -- the module's solder
jumper picks one, the other is left floating, so the socket wires both to the
same net and works whichever way the jumper is set.

Pad numbering follows the A4988/Pololu StepStick convention (1..16, GND=1)
so this footprint stays swappable with Module:Pololu_Breakout-16_15.2x20.3mm;
17/18/19 are the aux DIAG/VREF/INDEX pads that the Pololu part does not have.

Careful with the vendored project: its copper geometry is MIRRORED.  The
handedness of the (EN, VM, DIR) triangle in that file is the opposite of both
Watterott's own "view from the top side" pinout drawing and KiCad's stock
Pololu_Breakout-16 / Pololu_Breakout_A4988 pairing, and those two agree with
each other.  Pitches, offsets and the aux-pad positions are unaffected by a
mirror so they are taken from the import; the handedness is not, so it is
taken from the other two.  See the comment above PINS.

The module plugs in component side up, so top view of the module and top view
of the carrier agree -- once the handedness is right, no further mirroring.
"""

import os
import uuid as _uuid

NS = _uuid.UUID('3f2a17c5-9d64-4b0e-8f31-6c25a8e7d410')
HERE = os.path.dirname(os.path.abspath(__file__))
KSYM = r"C:\Program Files\KiCad\10.0\share\kicad\symbols"

LIB = 'TMC2209_Socket'
PART = 'TMC2209_StepStick_Socket'
PROJECT = 'tmc2209_socket'
DATE = '2026-08-26'
DS = ('https://learn.watterott.com/silentstepstick/pinconfig/tmc2209/')

_seen = set()


def U(tag):
    """Deterministic uuid so regenerating produces a stable diff."""
    assert tag not in _seen, 'duplicate uuid tag ' + tag
    _seen.add(tag)
    return str(_uuid.uuid5(NS, tag))


def n(v):
    """KiCad writes numbers without a trailing .0"""
    v = round(v + 0.0, 4)
    if v == int(v):
        return str(int(v))
    return ('%f' % v).rstrip('0')


# ---------------------------------------------------------------------------
# the part, described once
# ---------------------------------------------------------------------------
# (pad, name, footprint x, footprint y, electrical type, symbol side, symbol y)
#
# Footprint frame: origin = centre of the module outline, and the part is
# drawn the way Watterott draws it -- EN / VM / DIAG / VREF / INDEX along the
# TOP edge (-y), EN column on the left, DIR / GND along the bottom.
#
# ORIENTATION, and why it is not simply copied out of the vendored project:
# the Eagle -> KiCad import under malloc-19-SilentStepStickKiCad-ed32888/ is
# MIRRORED.  Checking the handedness of the (EN, VM, DIR) triangle against
# Watterott's own "view from the top side" pinout drawing and against KiCad's
# Module:Pololu_Breakout-16_15.2x20.3mm + Driver_Motor:Pololu_Breakout_A4988
# pairing, those two agree with each other and the import is the odd one out.
# So the pad *positions*, *pitches* and *offsets* come from the import (they
# are unaffected by a mirror) and the handedness comes from the other two.
#
# Same pad numbering as Module:Pololu_Breakout-16_15.2x20.3mm, which is this
# footprint rotated 180 degrees; 17/18/19 are the aux pads it does not have.

COL_L = -6.35          # logic header column
COL_R = 6.35           # power header column
ROW = 2.54             # header pitch
BODY_X, BODY_Y = 7.62, 10.16

PINS = [
    # pad  name        fx      fy      type           side     sy
    (1,  'GND',       COL_R, 8.89, 'power_in',      'bot',   5.08),
    (2,  'VIO',       COL_R, 6.35, 'power_in',      'top',   5.08),
    (3,  'M1B',       COL_R, 3.81, 'output',        'right', 7.62),
    (4,  'M1A',       COL_R, 1.27, 'output',        'right', 10.16),
    (5,  'M2A',       COL_R, -1.27, 'output',        'right', 2.54),
    (6,  'M2B',       COL_R, -3.81, 'output',        'right', 0.0),
    (7,  'GND',       COL_R, -6.35, 'power_in',      'bot',  -5.08),
    (8,  'VM',        COL_R, -8.89, 'power_in',      'top',  -5.08),
    (9,  'EN',        COL_L, -8.89, 'input',         'left',  15.24),
    (10, 'MS1',       COL_L, -6.35, 'input',         'left',   2.54),
    (11, 'MS2',       COL_L, -3.81, 'input',         'left',   0.0),
    (12, 'SPREAD',    COL_L, -1.27, 'input',         'left',  -2.54),
    (13, 'PDN_UART',  COL_L, 1.27, 'bidirectional', 'left',  -7.62),
    (14, 'PDN_UART',  COL_L, 3.81, 'bidirectional', 'left', -10.16),
    (15, 'STEP',      COL_L, 6.35, 'input',         'left',  10.16),
    (16, 'DIR',       COL_L, 8.89, 'input',         'left',   7.62),
    (17, 'DIAG',      -3.81, -8.89, 'output',        'right', -7.62),
    (18, 'VREF',      -2.54, -7.239, 'passive',       'right', -15.24),
    (19, 'INDEX',     -1.27, -8.89, 'output',        'right', -10.16),
]

# name shown in the schematic symbol (overbar for the active-low enable)
SYM_NAME = {'EN': '~{EN}'}


# ---------------------------------------------------------------------------
# footprint
# ---------------------------------------------------------------------------

def fp_text(kind, s, x, y, layer, size, thick, tag, just='', rot=0):
    j = '\t\t\t(justify %s)\n' % just if just else ''
    return ('\t(fp_text %s "%s"\n\t\t(at %s %s %s)\n\t\t(unlocked yes)\n'
            '\t\t(layer "%s")\n\t\t(uuid "%s")\n\t\t(effects\n\t\t\t(font\n'
            '\t\t\t\t(size %s %s)\n\t\t\t\t(thickness %s)\n\t\t\t)\n%s'
            '\t\t)\n\t)'
            % (kind, s, n(x), n(y), n(rot), layer, U(tag), n(size), n(size),
               n(thick), j))


def fp_line(x1, y1, x2, y2, layer, width, tag):
    return ('\t(fp_line\n\t\t(start %s %s)\n\t\t(end %s %s)\n\t\t(stroke\n'
            '\t\t\t(width %s)\n\t\t\t(type solid)\n\t\t)\n\t\t(layer "%s")\n'
            '\t\t(uuid "%s")\n\t)'
            % (n(x1), n(y1), n(x2), n(y2), n(width), layer, U(tag)))


def fp_rect(x1, y1, x2, y2, layer, width, tag):
    return ('\t(fp_rect\n\t\t(start %s %s)\n\t\t(end %s %s)\n\t\t(stroke\n'
            '\t\t\t(width %s)\n\t\t\t(type default)\n\t\t)\n\t\t(fill no)\n'
            '\t\t(layer "%s")\n\t\t(uuid "%s")\n\t)'
            % (n(x1), n(y1), n(x2), n(y2), n(width), layer, U(tag)))


def fp_prop(name, val, x, y, layer, tag, hide=False, size=1.0, thick=0.15):
    return ('\t(property "%s" "%s"\n\t\t(at %s %s 0)\n\t\t(unlocked yes)\n'
            '\t\t(layer "%s")\n%s\t\t(uuid "%s")\n\t\t(effects\n\t\t\t(font\n'
            '\t\t\t\t(size %s %s)\n\t\t\t\t(thickness %s)\n\t\t\t)\n\t\t)\n\t)'
            % (name, val, n(x), n(y), layer, '\t\t(hide yes)\n' if hide else '',
               U(tag), n(size), n(size), n(thick)))


def pad(num, shape, x, y, size, drill, tag):
    return ('\t(pad "%d" thru_hole %s\n\t\t(at %s %s)\n\t\t(size %s %s)\n'
            '\t\t(drill %s)\n\t\t(layers "*.Cu" "*.Mask")\n'
            '\t\t(remove_unused_layers no)\n\t\t(uuid "%s")\n\t)'
            % (num, shape, n(x), n(y), n(size), n(size), n(drill), U(tag)))


def build_footprint():
    it = []

    it.append(fp_prop('Reference', 'REF**', 0, -11.4, 'F.SilkS', 'fp.ref'))
    it.append(fp_prop('Value', PART, 0, 11.4, 'F.Fab', 'fp.val'))
    it.append(fp_prop('Datasheet', DS, 0, 0, 'F.Fab', 'fp.ds', hide=True,
                      size=1.27))
    it.append(fp_prop('Description',
                      'Socket for a TMC2209 SilentStepStick / StepStick '
                      'stepper driver module, 2 x 1x08 2.54 mm plus the '
                      'DIAG / VREF / INDEX aux pads',
                      0, 0, 'F.Fab', 'fp.descr', hide=True, size=1.27))
    it.append('\t(attr through_hole)')
    it.append('\t(duplicate_pad_numbers_are_jumpers no)')

    # ---- silkscreen: outline with the pin-1 corner chamfered -------------
    # pad 1 (GND) is the bottom right corner, so that is where the chamfer goes
    sx, sy = BODY_X + 0.12, BODY_Y + 0.12
    it.append(fp_line(-sx, -sy, sx, -sy, 'F.SilkS', 0.12, 'fp.s1'))
    it.append(fp_line(sx, -sy, sx, sy - 1.0, 'F.SilkS', 0.12, 'fp.s2'))
    it.append(fp_line(sx, sy - 1.0, sx - 1.0, sy, 'F.SilkS', 0.12, 'fp.s3'))
    it.append(fp_line(sx - 1.0, sy, -sx, sy, 'F.SilkS', 0.12, 'fp.s4'))
    it.append(fp_line(-sx, sy, -sx, -sy, 'F.SilkS', 0.12, 'fp.s5'))

    # the two silk labels that actually matter once a module is plugged in
    it.append(fp_text('user', 'EN', COL_L - 1.75, -8.89, 'F.SilkS', 0.7, 0.12,
                      'fp.t.en', just='right'))
    it.append(fp_text('user', 'VM', COL_R + 1.75, -8.89, 'F.SilkS', 0.7, 0.12,
                      'fp.t.vm', just='left'))

    # ---- fab: true module outline + every pin name -----------------------
    it.append(fp_rect(-BODY_X, -BODY_Y, BODY_X, BODY_Y, 'F.Fab', 0.1,
                      'fp.fab.body'))
    for num, name, fx, fy, _t, _s, _y in PINS:
        if num > 16:
            continue
        left = fx < 0
        # EN shares its row with the DIAG and INDEX pads -- drop its label
        # one row's worth so the pad does not sit on top of the text
        dy = 1.4 if num == 9 else 0.0
        it.append(fp_text('user', name, (COL_L + 1.05) if left
                          else (COL_R - 1.05), fy + dy, 'F.Fab', 0.5, 0.08,
                          'fp.fab.%d' % num, just='left' if left else 'right'))
    it.append(fp_text('user', '17 DIAG   18 VREF   19 INDEX', 0, 12.6,
                      'F.Fab', 0.5, 0.08, 'fp.fab.aux'))

    # ---- courtyard -------------------------------------------------------
    it.append(fp_rect(-(BODY_X + 0.25), -(BODY_Y + 0.25),
                      BODY_X + 0.25, BODY_Y + 0.25, 'F.CrtYd', 0.05,
                      'fp.crtyd'))

    # ---- pads ------------------------------------------------------------
    for num, name, fx, fy, _t, _s, _y in PINS:
        if num == 18:                       # VREF is a small probe pad
            it.append(pad(num, 'circle', fx, fy, 1.2, 0.7, 'fp.pad%d' % num))
        else:
            it.append(pad(num, 'rect' if num == 1 else 'circle', fx, fy,
                          1.7, 1.0, 'fp.pad%d' % num))

    out = ('(footprint "%s"\n\t(version 20260206)\n\t(generator "pcbnew")\n'
           '\t(generator_version "10.0")\n\t(layer "F.Cu")\n'
           '\t(descr "Socket for a TMC2209 SilentStepStick v2.x stepper '
           'driver module (15.24 x 20.32 mm, 2 x 1x08 headers 12.7 mm apart, '
           'plus DIAG / VREF / INDEX aux pads). Drawn as seen from the top '
           'with the module fitted: EN / VM row along the top edge, EN column '
           'left, VM column right, DIAG and INDEX to the right of EN. Pads '
           '1-16 keep the A4988 / Pololu StepStick numbering, so this is '
           'Module:Pololu_Breakout-16_15.2x20.3mm rotated 180 degrees plus '
           'the three aux pads.")\n'
           '\t(tags "TMC2209 SilentStepStick StepStick socket stepper '
           'driver module")\n%s\n)\n' % (PART, '\n'.join(it)))
    d = os.path.join(HERE, LIB + '.pretty')
    if not os.path.isdir(d):
        os.makedirs(d)
    p = os.path.join(d, PART + '.kicad_mod')
    open(p, 'w', encoding='utf-8').write(out)
    print('wrote', p, len(out), 'bytes')


# ---------------------------------------------------------------------------
# symbol
# ---------------------------------------------------------------------------

SYM_HALF_X = 12.7          # body half width
SYM_PIN_X = 17.78          # pin origin on the left / right
SYM_HALF_Y = 20.32
SYM_PIN_Y = 22.86


def sym_pin(kind, x, y, rot, length, name, num):
    return ('\t\t\t(pin %s line\n\t\t\t\t(at %s %s %d)\n\t\t\t\t(length %s)\n'
            '\t\t\t\t(name "%s"\n\t\t\t\t\t(effects\n\t\t\t\t\t\t(font\n'
            '\t\t\t\t\t\t\t(size 1.27 1.27)\n\t\t\t\t\t\t)\n\t\t\t\t\t)\n'
            '\t\t\t\t)\n\t\t\t\t(number "%d"\n\t\t\t\t\t(effects\n'
            '\t\t\t\t\t\t(font\n\t\t\t\t\t\t\t(size 1.27 1.27)\n'
            '\t\t\t\t\t\t)\n\t\t\t\t\t)\n\t\t\t\t)\n\t\t\t)'
            % (kind, n(x), n(y), rot, n(length), name, num))


def build_symbol(qualified):
    """The symbol body, indented for a .kicad_sym (one tab)."""
    def prop(name, val, x, y, hide, show_name='no', just=''):
        j = '\t\t\t\t(justify %s)\n' % just if just else ''
        return ('\t\t(property "%s" "%s"\n\t\t\t(at %s %s 0)\n%s'
                '\t\t\t(show_name %s)\n\t\t\t(do_not_autoplace no)\n'
                '\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n'
                '\t\t\t\t)\n%s\t\t\t)\n\t\t)'
                % (name, val, n(x), n(y), '\t\t\t(hide yes)\n' if hide else '',
                   show_name, j))

    head = ['\t(symbol "%s"' % qualified,
            '\t\t(pin_names\n\t\t\t(offset 0.508)\n\t\t)',
            '\t\t(exclude_from_sim no)',
            '\t\t(in_bom yes)',
            '\t\t(on_board yes)',
            '\t\t(in_pos_files yes)',
            '\t\t(duplicate_pin_numbers_are_jumpers no)',
            prop('Reference', 'A', -SYM_HALF_X, SYM_HALF_Y + 2.54, False,
                 just='left'),
            prop('Value', PART, -SYM_HALF_X, SYM_HALF_Y + 5.08, False,
                 just='left'),
            prop('Footprint', '%s:%s' % (LIB, PART), 0, -SYM_HALF_Y - 2.54,
                 True),
            prop('Datasheet', DS, 0, 0, True),
            prop('Description',
                 'Socket for a TMC2209 SilentStepStick stepper driver module '
                 '(plug-in StepStick, 2 x 1x08 + DIAG / VREF / INDEX)',
                 0, 0, True),
            prop('ki_keywords',
                 'TMC2209 SilentStepStick StepStick socket stepper driver '
                 'module trinamic', 0, 0, True),
            prop('ki_fp_filters', 'TMC2209*Socket* Pololu*Breakout*16*',
                 0, 0, True)]

    body = ['\t\t(symbol "%s_0_1"' % qualified.split(':')[-1],
            '\t\t\t(rectangle\n\t\t\t\t(start %s %s)\n\t\t\t\t(end %s %s)\n'
            '\t\t\t\t(stroke\n\t\t\t\t\t(width 0.254)\n\t\t\t\t\t'
            '(type default)\n\t\t\t\t)\n\t\t\t\t(fill\n\t\t\t\t\t'
            '(type background)\n\t\t\t\t)\n\t\t\t)'
            % (n(-SYM_HALF_X), n(SYM_HALF_Y), n(SYM_HALF_X), n(-SYM_HALF_Y)),
            '\t\t)']

    pins = ['\t\t(symbol "%s_1_1"' % qualified.split(':')[-1]]
    for num, name, _fx, _fy, etype, side, sy in PINS:
        nm = SYM_NAME.get(name, name)
        if side == 'left':
            pins.append(sym_pin(etype, -SYM_PIN_X, sy, 0, 5.08, nm, num))
        elif side == 'right':
            pins.append(sym_pin(etype, SYM_PIN_X, sy, 180, 5.08, nm, num))
        elif side == 'top':
            pins.append(sym_pin(etype, sy, SYM_PIN_Y, 270, 2.54, nm, num))
        else:
            pins.append(sym_pin(etype, sy, -SYM_PIN_Y, 90, 2.54, nm, num))
    pins.append('\t\t)')

    return '\n'.join(head + body + pins + ['\t\t(embedded_fonts no)', '\t)'])


def build_symbol_lib():
    out = ('(kicad_symbol_lib\n\t(version 20251024)\n'
           '\t(generator "kicad_symbol_editor")\n'
           '\t(generator_version "10.0")\n%s\n)\n' % build_symbol(PART))
    p = os.path.join(HERE, LIB + '.kicad_sym')
    open(p, 'w', encoding='utf-8').write(out)
    print('wrote', p, len(out), 'bytes')


# ---------------------------------------------------------------------------
# lib_symbol extraction for the sheet
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


def indent(block):
    """shift a .kicad_sym symbol block into a schematic's lib_symbols"""
    nl = chr(10)
    return nl.join(chr(9) + l for l in block.split(nl))


def from_lib(libname, name):
    """Pull a symbol out of a KiCad 10 system library, verbatim."""
    s = open(os.path.join(KSYM, libname + '.kicad_sym'), encoding='utf-8').read()
    key = '\t(symbol "%s"\n' % name
    i = s.index(key)
    blk = _balanced(s, i + 1)
    out = []
    for k, line in enumerate(blk.split('\n')):
        if k == 0:
            line = line.replace('"%s"' % name, '"%s:%s"' % (libname, name), 1)
        out.append(('\t\t' if k == 0 else '\t') + line)
    return '\n'.join(out)


# ---------------------------------------------------------------------------
# sheet emitter
# ---------------------------------------------------------------------------

class Sheet:
    def __init__(self, title, comment, paper='A3'):
        self.title, self.comment, self.paper = title, comment, paper
        self.items, self.libs = [], []
        self.pwr = 0

    def wire(self, x1, y1, x2, y2, tag):
        self.items.append(
            '\t(wire\n\t\t(pts\n\t\t\t(xy %s %s) (xy %s %s)\n\t\t)\n'
            '\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n'
            '\t\t(uuid "%s")\n\t)' % (n(x1), n(y1), n(x2), n(y2), U(tag)))

    def poly(self, pts, tag):
        for k in range(len(pts) - 1):
            self.wire(pts[k][0], pts[k][1], pts[k + 1][0], pts[k + 1][1],
                      '%s.%d' % (tag, k))

    def junction(self, x, y, tag):
        self.items.append(
            '\t(junction\n\t\t(at %s %s)\n\t\t(diameter 0)\n'
            '\t\t(color 0 0 0 0)\n\t\t(uuid "%s")\n\t)'
            % (n(x), n(y), U(tag)))

    def label(self, text, x, y, rot, tag):
        just = 'left' if rot in (0, 90) else 'right'
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
            % (s, n(x), n(y), n(size), n(size), 'yes' if bold else 'no',
               U(tag)))

    def symbol(self, lib_id, ref, value, x, y, tag, footprint='',
               datasheet='', description='', npins=1, rot=0,
               ref_dy=-2.54, val_dy=2.54, hide_ref=False, hide_val=False,
               ref_dx=0.0, val_dx=0.0, justify=None):
        props = []
        jline = ('\t\t\t\t(justify %s)\n' % justify) if justify else ''

        def p(name, val, dx, dy, hide, j=''):
            props.append(
                '\t\t(property "%s" "%s"\n\t\t\t(at %s %s 0)\n%s'
                '\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n'
                '\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n'
                '\t\t\t\t)\n%s\t\t\t)\n\t\t)'
                % (name, val, n(x + dx), n(y + dy),
                   '\t\t\t(hide yes)\n' if hide else '', j))

        p('Reference', ref, ref_dx, ref_dy, hide_ref, j=jline)
        p('Value', value, val_dx, val_dy, hide_val, j=jline)
        p('Footprint', footprint, 0, 0, True)
        p('Datasheet', datasheet, 0, 0, True)
        p('Description', description, 0, 0, True)

        pins = '\n'.join('\t\t(pin "%d"\n\t\t\t(uuid "%s")\n\t\t)'
                         % (k + 1, U('%s.pin%d' % (tag, k + 1)))
                         for k in range(npins))

        self.items.append(
            '\t(symbol\n\t\t(lib_id "%s")\n\t\t(at %s %s %d)\n\t\t(unit 1)\n'
            '\t\t(body_style 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n'
            '\t\t(on_board yes)\n\t\t(in_pos_files yes)\n\t\t(dnp no)\n'
            '\t\t(uuid "%s")\n%s\n%s\n\t\t(instances\n'
            '\t\t\t(project "%s"\n\t\t\t\t(path "/%%SHEETUUID%%"\n'
            '\t\t\t\t\t(reference "%s")\n\t\t\t\t\t(unit 1)\n\t\t\t\t)\n'
            '\t\t\t)\n\t\t)\n\t)'
            % (lib_id, n(x), n(y), rot, U(tag), '\n'.join(props), pins,
               PROJECT, ref))

    def power(self, kind, x, y, tag, rot=0):
        self.pwr += 1
        self.symbol('power:%s' % kind, '#PWR%03d' % self.pwr, kind, x, y, tag,
                    npins=1, rot=rot, ref_dy=2.54, val_dy=3.6, hide_ref=True)

    def render(self, path):
        su = str(_uuid.uuid5(NS, 'sheet:' + os.path.basename(path)))
        body = '\n'.join(self.items).replace('%SHEETUUID%', su)
        out = ('(kicad_sch\n\t(version 20260306)\n\t(generator "eeschema")\n'
               '\t(generator_version "10.0")\n\t(uuid "%s")\n\t(paper "%s")\n'
               '\t(title_block\n\t\t(title "%s")\n\t\t(date "%s")\n'
               '\t\t(rev "A")\n\t\t(comment 1 "%s")\n\t)\n'
               '\t(lib_symbols\n%s\n\t)\n%s\n'
               '\t(sheet_instances\n\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n'
               '\t)\n\t(embedded_fonts no)\n)\n'
               % (su, self.paper, self.title, DATE, self.comment,
                  '\n'.join(self.libs), body))
        open(path, 'w', encoding='utf-8').write(out)
        print('wrote', path, len(out), 'bytes')


# ---------------------------------------------------------------------------
# the one-channel sheet
# ---------------------------------------------------------------------------

FP_C0603 = 'Capacitor_SMD:C_0603_1608Metric'
FP_R0603 = 'Resistor_SMD:R_0603_1608Metric'
FP_CP = 'Capacitor_THT:CP_Radial_D8.0mm_P3.50mm'
FP_MOT = 'Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical'
FP_TP = 'TestPoint:TestPoint_Pad_D1.5mm'

X0, Y0 = 190.5, 152.4                       # socket origin
LX = X0 - SYM_PIN_X                         # left pin column   172.72
RX = X0 + SYM_PIN_X                         # right pin column  208.28
TY = Y0 - SYM_PIN_Y                         # top pin row       129.54
BY = Y0 + SYM_PIN_Y                         # bottom pin row    175.26
HL = 127.0                                  # hierarchical labels, left edge


def py(sy):
    """symbol y -> sheet y"""
    return Y0 - sy


def build_sheet():
    sh = Sheet('TMC2209 SilentStepStick socket -- one channel',
               'Import once per driver; four instances on the hybrid board')
    for lib, nm in (('Device', 'C'), ('Device', 'C_Polarized'),
                    ('Device', 'R'), ('Connector', 'Conn_01x04_Pin'),
                    ('Connector', 'TestPoint'), ('power', 'GND')):
        sh.libs.append(from_lib(lib, nm))
    sh.libs.append(indent(build_symbol('%s:%s' % (LIB, PART))))

    # ---- the socket ------------------------------------------------------
    sh.symbol('%s:%s' % (LIB, PART), 'A1', 'TMC2209_StepStick', X0, Y0,
              's.A1', footprint='%s:%s' % (LIB, PART), datasheet=DS,
              description='TMC2209 SilentStepStick socket',
              npins=len(PINS), ref_dx=-12.7, ref_dy=-25.4,
              val_dx=-12.7, val_dy=-22.86, justify='left')

    # ---- VM rail ---------------------------------------------------------
    sh.poly([(HL, 114.3), (185.42, 114.3), (185.42, TY)], 'w.vm')
    sh.hlabel('VM', 'input', HL, 114.3, 180, 'h.vm')
    for x, ref, val, tag, polar, fp in (
            (146.05, 'C1', '100uF_35V', 'c1', True, FP_CP),
            (158.75, 'C2', '100nF', 'c2', False, FP_C0603)):
        sh.symbol('Device:C_Polarized' if polar else 'Device:C', ref, val,
                  x, 118.11, 's.' + tag, footprint=fp, npins=2,
                  ref_dx=2.54, ref_dy=-1.27, val_dx=2.54, val_dy=1.27,
                  justify='left')
        sh.junction(x, 114.3, 'j.' + tag)
        sh.power('GND', x, 121.92, 'p.' + tag)

    # ---- VIO rail --------------------------------------------------------
    sh.poly([(HL, 104.14), (195.58, 104.14), (195.58, TY)], 'w.vio')
    sh.hlabel('VIO', 'input', HL, 104.14, 180, 'h.vio')
    sh.symbol('Device:C', 'C3', '100nF', 189.23, 107.95, 's.c3',
              footprint=FP_C0603, npins=2, ref_dx=2.54, ref_dy=-1.27,
              val_dx=2.54, val_dy=1.27, justify='left')
    sh.junction(189.23, 104.14, 'j.c3')
    sh.power('GND', 189.23, 111.76, 'p.c3')

    # ---- GND -------------------------------------------------------------
    for x, tag in ((185.42, 'g1'), (195.58, 'g2')):
        sh.wire(x, BY, x, BY + 5.08, 'w.' + tag)
        sh.power('GND', x, BY + 5.08, 'p.' + tag)

    # ---- logic side ------------------------------------------------------
    for name, shape, sy, tag in (('EN', 'input', 15.24, 'en'),
                                 ('STEP', 'input', 10.16, 'step'),
                                 ('DIR', 'input', 7.62, 'dir'),
                                 ('MS1', 'input', 2.54, 'ms1'),
                                 ('MS2', 'input', 0.0, 'ms2'),
                                 ('SPREAD', 'input', -2.54, 'spread')):
        sh.wire(LX, py(sy), HL, py(sy), 'w.' + tag)
        sh.hlabel(name, shape, HL, py(sy), 180, 'h.' + tag)

    # EN pull-up to VIO -- outputs stay off while the MCU pin is still hi-Z
    sh.symbol('Device:R', 'R1', '10k', 152.4, 133.35, 's.r1',
              footprint=FP_R0603, npins=2, ref_dx=2.54, ref_dy=-1.27,
              val_dx=2.54, val_dy=1.27, justify='left')
    sh.junction(152.4, py(15.24), 'j.r1')
    sh.wire(152.4, 129.54, 152.4, 127.0, 'w.r1')
    sh.label('VIO', 152.4, 127.0, 90, 'l.vio')

    # ---- PDN_UART: both module positions on one net, 1k in series --------
    sh.wire(LX, py(-7.62), 143.51, py(-7.62), 'w.pdn13')
    sh.poly([(LX, py(-10.16)), (160.02, py(-10.16)), (160.02, py(-7.62))],
            'w.pdn14')
    sh.junction(160.02, py(-7.62), 'j.pdn')
    sh.symbol('Device:R', 'R2', '1k', 139.7, py(-7.62), 's.r2',
              footprint=FP_R0603, npins=2, rot=90, ref_dy=-3.81,
              val_dy=3.81)
    sh.wire(135.89, py(-7.62), HL, py(-7.62), 'w.uart')
    sh.hlabel('UART', 'bidirectional', HL, py(-7.62), 180, 'h.uart')

    # ---- motor side ------------------------------------------------------
    for name, sy in (('M1A', 10.16), ('M1B', 7.62),
                     ('M2A', 2.54), ('M2B', 0.0)):
        sh.wire(RX, py(sy), 218.44, py(sy), 'w.' + name)
        sh.label(name, 218.44, py(sy), 0, 'l.' + name)

    sh.symbol('Connector:Conn_01x04_Pin', 'J1', 'Motor', 254.0, 147.32, 's.j1',
              footprint=FP_MOT, npins=4, rot=180, ref_dx=2.54, ref_dy=-7.62,
              val_dx=2.54, val_dy=-5.08, justify='left',
              description='Stepper motor, coil 1 = pins 1-2, coil 2 = 3-4')
    for k, name in enumerate(('M1A', 'M1B', 'M2A', 'M2B')):
        y = 147.32 + 2.54 - k * 2.54
        sh.wire(248.92, y, 241.3, y, 'w.j1.%d' % k)
        sh.label(name, 241.3, y, 180, 'l.j1.%d' % k)

    # ---- diagnostics -----------------------------------------------------
    for name, sy, tag in (('DIAG', -7.62, 'diag'), ('INDEX', -10.16, 'index')):
        sh.wire(RX, py(sy), 218.44, py(sy), 'w.' + tag)
        sh.hlabel(name, 'output', 218.44, py(sy), 0, 'h.' + tag)

    sh.wire(RX, py(-15.24), 218.44, py(-15.24), 'w.vref')
    sh.label('VREF', 213.36, py(-15.24), 0, 'l.vref')
    sh.symbol('Connector:TestPoint', 'TP1', 'VREF', 218.44, py(-15.24),
              's.tp1', footprint=FP_TP, npins=1, rot=90, ref_dx=2.54,
              ref_dy=-2.54, val_dx=2.54, val_dy=0, justify='left')

    # ---- notes -----------------------------------------------------------
    sh.text('TMC2209 SilentStepStick socket  --  one driver channel',
            20, 24, 4, 't.title')
    sh.text('Pinout and pad geometry from Watterott SilentStepStick-TMC2209 '
            'v2.0. Import this sheet once per driver.', 20, 31, 2, 't.sub',
            bold=False)

    notes = [
        'A1 is the socket, not the driver: 2 x 1x08 female headers on '
        '2.54 mm, 12.7 mm apart, plus the DIAG / VREF / INDEX aux pads.',
        'Pads 13 and 14 are the two PDN_UART positions on the module. Only '
        'one of them is live -- the module jumper picks which -- so both are',
        '    tied to the same net and the socket works either way. R2 (1k) '
        'is the series resistor Trinamic requires on the single-wire UART;',
        '    the parent joins MCU TX and RX at the UART sheet pin.',
        'MS1 = address bit 0, MS2 = address bit 1. Both have pull-downs on '
        'the module, so an unconnected sheet pin means UART address 0.',
        'SPREAD low = stealthChop, high = spreadCycle (module pull-down). '
        'Leave the sheet pin open for stealthChop.',
        'EN is active low. R1 pulls it up to VIO so the outputs stay off '
        'while the MCU pin is still an input at reset.',
        'C1 is not optional: Watterott asks for at least 100 uF of local '
        'bulk on VM, within ~10 mm of the socket. VM range 5.5 - 28 V,',
        '    VIO 3.3 - 5 V. Both arrive as sheet pins -- the parent has to '
        'drive them.',
        'VREF is the analog current-set reference. Brought out to TP1 only; '
        'the module ships with no pin fitted in that hole.',
        'Motor coils: M1A / M1B are one coil, M2A / M2B the other. Never '
        'unplug a motor with VM live.',
    ]
    for k, s in enumerate(notes):
        sh.text(s, 20, 200 + k * 5.5, 2, 't.n%d' % k, bold=False)

    sh.render(os.path.join(HERE, PROJECT + '.kicad_sch'))


if __name__ == '__main__':
    build_footprint()
    build_symbol_lib()
    build_sheet()
