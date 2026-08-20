import sys
sys.path.insert(0, r'C:\Users\acard\.claude\plugins\cache\kicad-happy\kicad-happy\2.1.0\skills\kicad\scripts')
from sexp_parser import parse_file, find_all, find_first

import os
_HERE = os.path.dirname(os.path.abspath(__file__))
_PROJ = os.path.dirname(_HERE)

ORIG = os.path.join(_HERE, 'base.kicad_sch')
SCH  = os.path.join(_PROJ, 'pantiltslide_full.kicad_sch')

def rec(node, kw, acc=None):
    if acc is None:
        acc = []
    if isinstance(node, list):
        if len(node) > 0 and node[0] == kw:
            acc.append(node)
        for c in node:
            rec(c, kw, acc)
    return acc

def rkey(x, y):
    return (round(x, 3), round(y, 3))

def load(path):
    doc = parse_file(path)
    lib_pins = {}
    for s in find_all(find_first(doc, 'lib_symbols'), 'symbol'):
        m = {}
        for p in rec(s, 'pin'):
            at = find_first(p, 'at')
            m[find_first(p, 'number')[1]] = (float(at[1]), float(at[2]), float(at[3]))
        if m:
            lib_pins[s[1]] = m
    wire_index = {}
    for w in find_all(doc, 'wire'):
        xy = find_all(find_first(w, 'pts'), 'xy')
        a = rkey(float(xy[0][1]), float(xy[0][2]))
        b = rkey(float(xy[1][1]), float(xy[1][2]))
        wire_index.setdefault(a, []).append(b)
        wire_index.setdefault(b, []).append(a)
    label_at = {}
    for g in find_all(doc, 'global_label'):
        at = find_first(g, 'at')
        label_at[rkey(float(at[1]), float(at[2]))] = g[1]
    nc_at = set()
    for n in find_all(doc, 'no_connect'):
        at = find_first(n, 'at')
        nc_at.add(rkey(float(at[1]), float(at[2])))
    syms = {}
    for s in find_all(doc, 'symbol'):
        ref = None
        for p in find_all(s, 'property'):
            if p[1] == 'Reference':
                ref = p[2]
        at = find_first(s, 'at')
        mir = find_first(s, 'mirror')
        syms[ref] = (find_first(s, 'lib_id')[1], float(at[1]), float(at[2]),
                     [p[1] for p in find_all(s, 'pin')],
                     float(at[3]) if len(at) > 3 else 0.0,
                     mir[1] if mir else None)
    return lib_pins, wire_index, label_at, nc_at, syms

# --- symbol-local -> sheet coordinate transform ----------------------------
# Handles the GENERAL case (rotation + mirror), not just the identity case,
# so this stays correct if a symbol is ever rotated or flipped later.
#   1. mirror in symbol space   ('y' flips x, 'x' flips y)
#   2. rotate by the instance angle (CCW in symbol space)
#   3. sheet_x = origin_x + x' ; sheet_y = origin_y - y'   <- Y IS INVERTED
# Validated by the self-test below against KiCad's own output, which includes
# a mirrored symbol (J2 carries `(mirror y)`).
import math

def sheet_pos(cx, cy, lx, ly, rot=0.0, mirror=None):
    if mirror == 'y':
        lx = -lx
    elif mirror == 'x':
        ly = -ly
    if rot:
        t = math.radians(rot)
        lx, ly = lx * math.cos(t) - ly * math.sin(t), lx * math.sin(t) + ly * math.cos(t)
    return rkey(cx + lx, cy - ly)

def resolve(lib_pins, wire_index, label_at, nc_at, syms, ref, pn):
    """Walk the wire graph out from a pin until a label or no-connect is found.

    Must be a traversal, not a single hop: a net may be drawn as a chain of
    wire segments, and KiCad also allows a label to sit directly on the pin
    with no wire at all (the original file does both).
    """
    lib_id, cx, cy, _, rot, mir = syms[ref]
    lx, ly, ang = lib_pins[lib_id][pn]
    start = sheet_pos(cx, cy, lx, ly, rot, mir)
    seen = {start}
    queue = [start]
    saw_wire = False
    while queue:
        node = queue.pop(0)
        if node in label_at:
            return ('LABEL', label_at[node])
        if node in nc_at:
            return ('NC', None)
        for nxt in wire_index.get(node, []):
            saw_wire = True
            if nxt not in seen:
                seen.add(nxt)
                queue.append(nxt)
    return ('DANGLING' if saw_wire else 'NOWIRE', None)

# ===========================================================================
# SELF-TEST: validate the sign convention against the ORIGINAL KiCad-authored
# file, which this script did not produce. If the formula is wrong, this fails
# and the whole run aborts -- so the check cannot silently agree with itself.
# ===========================================================================
o = load(ORIG)
GROUND_TRUTH = [('J1', '4', 'GPIO_4'), ('J1', '5', 'GPIO_5'), ('J1', '16', 'GPIO_10'),
                ('J2', '14', 'GPIO_0'), ('J2', '3', 'U0RXD')]
print('--- self-test against original KiCad-authored file ---')
failed = False
for ref, pn, expect in GROUND_TRUTH:
    kind, got = resolve(*o, ref, pn)
    ok = (kind == 'LABEL' and got == expect)
    print(f'  {ref}.{pn:<3} -> {got!r:12} expect {expect!r:12} {"OK" if ok else "FAIL"}')
    if not ok:
        failed = True
if failed:
    sys.exit('SELF-TEST FAILED: coordinate convention is wrong; aborting.')
print('  self-test passed: sheet_y = origin_y - local_y\n')

# ===========================================================================
# Now check the generated schematic against the intended net map.
# ===========================================================================
EXPECTED = {
    # U1 = slide, UART slave address 0 (MS1=0 pin15=GND, MS2=0 pin14=GND)
    ('U1','1'):'GND', ('U1','2'):'3.3v', ('U1','3'):'SLIDE_M1B', ('U1','4'):'SLIDE_M1A',
    ('U1','5'):'SLIDE_M2A', ('U1','6'):'SLIDE_M2B', ('U1','7'):'GND', ('U1','8'):'+24V',
    ('U1','9'):'GPIO_5', ('U1','10'):'GPIO_4',
    ('U1','11'):'GPIO_42', ('U1','12'):'GPIO_41',
    ('U1','14'):'GND', ('U1','15'):'GND',
    ('U1','16'):'GPIO_10',
    # U2 = pan, UART slave address 1 (MS1=1 pin15=3.3v, MS2=0 pin14=GND)
    ('U2','1'):'GND', ('U2','2'):'3.3v', ('U2','3'):'PAN_M1B', ('U2','4'):'PAN_M1A',
    ('U2','5'):'PAN_M2A', ('U2','6'):'PAN_M2B', ('U2','7'):'GND', ('U2','8'):'+24V',
    ('U2','9'):'GPIO_7', ('U2','10'):'GPIO_6',
    ('U2','11'):'GPIO_42', ('U2','12'):'GPIO_41',
    ('U2','14'):'GND', ('U2','15'):'3.3v',
    ('U2','16'):'GPIO_10',
    # U3 = tilt, UART slave address 2 (MS1=0 pin15=GND, MS2=1 pin14=3.3v)
    ('U3','1'):'GND', ('U3','2'):'3.3v', ('U3','3'):'TILT_M1B', ('U3','4'):'TILT_M1A',
    ('U3','5'):'TILT_M2A', ('U3','6'):'TILT_M2B', ('U3','7'):'GND', ('U3','8'):'+24V',
    ('U3','9'):'GPIO_9', ('U3','10'):'GPIO_8',
    ('U3','11'):'GPIO_42', ('U3','12'):'GPIO_41',
    ('U3','14'):'3.3v', ('U3','15'):'GND',
    ('U3','16'):'GPIO_10',
    # U4 = 4th/aux axis, UART slave address 3 (MS1=1 pin15=3.3v, MS2=1 pin14=3.3v)
    ('U4','1'):'GND', ('U4','2'):'3.3v', ('U4','3'):'AUX_M1B', ('U4','4'):'AUX_M1A',
    ('U4','5'):'AUX_M2A', ('U4','6'):'AUX_M2B', ('U4','7'):'GND', ('U4','8'):'+24V',
    ('U4','9'):'GPIO_48', ('U4','10'):'GPIO_47',
    ('U4','11'):'GPIO_42', ('U4','12'):'GPIO_41',
    ('U4','14'):'3.3v', ('U4','15'):'3.3v',
    ('U4','16'):'GPIO_10',
    # motor connectors
    ('J17','1'):'SLIDE_M1A', ('J17','2'):'SLIDE_M1B', ('J17','3'):'SLIDE_M2A', ('J17','4'):'SLIDE_M2B',
    ('J18','1'):'PAN_M1A', ('J18','2'):'PAN_M1B', ('J18','3'):'PAN_M2A', ('J18','4'):'PAN_M2B',
    ('J19','1'):'TILT_M1A', ('J19','2'):'TILT_M1B', ('J19','3'):'TILT_M2A', ('J19','4'):'TILT_M2B',
    ('J28','1'):'AUX_M1A', ('J28','2'):'AUX_M1B', ('J28','3'):'AUX_M2A', ('J28','4'):'AUX_M2B',
    # absolute angle sensors + display
    ('J21','1'):'3.3v', ('J21','2'):'GND', ('J21','3'):'MUX_CH0_SDA', ('J21','4'):'MUX_CH0_SCL',
    ('J22','1'):'3.3v', ('J22','2'):'GND', ('J22','3'):'MUX_CH1_SDA', ('J22','4'):'MUX_CH1_SCL',
    ('J23','1'):'3.3v', ('J23','2'):'GND', ('J23','3'):'GPIO_13', ('J23','4'):'GPIO_14',
    # one manual control encoder per axis (pin 4 reserved for the push switch)
    ('J29','1'):'GPIO_15', ('J29','2'):'GPIO_16', ('J29','3'):'GND',
    ('J30','1'):'GPIO_17', ('J30','2'):'GPIO_18', ('J30','3'):'GND',
    ('J31','1'):'GPIO_21', ('J31','2'):'GPIO_38', ('J31','3'):'GND',
    ('J32','1'):'GPIO_19', ('J32','2'):'GPIO_20', ('J32','3'):'GND',
    # I2C mux
    ('J20','1'):'3.3v', ('J20','2'):'GND', ('J20','3'):'GPIO_11', ('J20','4'):'GPIO_12',
    ('J20','5'):'MUX_CH0_SDA', ('J20','6'):'MUX_CH0_SCL', ('J20','7'):'MUX_CH1_SDA', ('J20','8'):'MUX_CH1_SCL',
    # slide limit switches
    ('J26','1'):'GPIO_39', ('J26','2'):'GND',
    ('J27','1'):'GPIO_40', ('J27','2'):'GND',
    # keyframe/transport buttons, all active-low to GND
    ('J33','1'):'GPIO_1',  ('J33','2'):'GND',   # set keyframe
    ('J34','1'):'GPIO_3',  ('J34','2'):'GND',   # delete/clear keyframe
    ('J35','1'):'GPIO_45', ('J35','2'):'GND',   # play/pause
    ('J36','1'):'GPIO_46', ('J36','2'):'GND',   # reset
}
NC_EXPECTED = set()
for u in ('U1', 'U2', 'U3', 'U4'):
    for n in (13, 17, 18):   # SPRD (stealthChop via pulldown), INDEX, DIAG
        NC_EXPECTED.add((u, str(n)))
for j in ('J29', 'J30', 'J31', 'J32'):
    NC_EXPECTED.add((j, '4'))   # encoder push switch, reserved

g = load(SCH)
lib_pins, wire_index, label_at, nc_at, syms = g
NEW_REFS = ['U1','U2','U3','U4',
            'J17','J18','J19','J28',
            'J21','J22','J23',
            'J29','J30','J31','J32',
            'J20','J26','J27',
            'J33','J34','J35','J36']

print('--- generated schematic vs intended net map ---')
problems = []
checked = 0
for ref in NEW_REFS:
    for pn in sorted(syms[ref][3], key=int):  # syms[ref][3] == pin-number list
        checked += 1
        kind, got = resolve(*g, ref, pn)
        exp = EXPECTED.get((ref, pn))
        if exp is not None:
            if not (kind == 'LABEL' and got == exp):
                problems.append(f'{ref}.{pn}: got {kind}/{got!r}, expected label {exp!r}')
        elif (ref, pn) in NC_EXPECTED:
            if kind != 'NC':
                problems.append(f'{ref}.{pn}: got {kind}/{got!r}, expected no-connect')
        else:
            problems.append(f'{ref}.{pn}: unaccounted-for pin (got {kind}/{got!r})')

print(f'checked {checked} pins, problems: {len(problems)}')
for p in problems:
    print('  ', p)
print('ALL CLEAR' if not problems else 'ISSUES FOUND')
