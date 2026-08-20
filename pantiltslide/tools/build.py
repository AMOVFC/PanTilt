"""Rebuild pantiltslide_full.kicad_sch deterministically from the base schematic.

Run:  python tools/build.py        (from the pantiltslide/ directory)

This is the ONLY supported way to regenerate the file. Rebuilding by hand is
what introduced a silent, board-killing bug once already: the symbol-to-sheet
coordinate transform was wrong (see COORDINATE CONVENTION in gen_wiring.py),
which mirrored every multi-pin part vertically and scrambled its net
assignments while still *looking* plausible.

Pipeline:
  1. copy BASE -> OUT (never edits BASE)
  2. rewrite the embedded project name to match OUT
  3. set the sheet size
  4. splice the TMC2209 symbol into the schematic's lib_symbols cache
  5. splice in the generated symbols/wires/labels from gen_wiring.py
  6. run verify_wiring.py, which self-tests its coordinate math against the
     original KiCad-authored file before checking anything else

Step 6 is not optional: it is the step that would have caught the mirror bug.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)

BASE = os.path.join(HERE, 'base.kicad_sch')
OUT = os.path.join(PROJ, 'pantiltslide_full.kicad_sch')
SYMLIB = os.path.join(PROJ, 'New_Library.kicad_sym')
BODY = os.path.join(HERE, '_body.generated.txt')

PROJECT_NAME = 'pantiltslide_full'
PAPER = 'A2'

LIBSYM_ANCHOR = '\t\t\t(embedded_fonts no)\n\t\t)\n\t)\n\t(junction\n'
BODY_ANCHOR = '\t(sheet_instances\n'

# Footprints for the hand-drawn parts in base.kicad_sch, which were all left
# unassigned. Without these, "Update PCB from Schematic" refuses them with
# "no footprint assigned" and their nets never reach the board.
#
# J1/J2 are the two 22-pin rows the ESP32-S3-DevKitC plugs into, so they get a
# socket strip each rather than one combined module footprint -- two symbols
# cannot share a single footprint, and sockets suit a plug-together kit.
# Every name below was checked against the installed KiCad libraries.
BASE_FOOTPRINTS = {
    'J1':  'Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical',
    'J2':  'Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical',
    'J3':  'Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical',
    'J4':  'Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical',
    'J7':  'Connector_PinSocket_2.54mm:PinSocket_1x12_P2.54mm_Vertical',
    'J8':  'Connector_PinSocket_2.54mm:PinSocket_1x12_P2.54mm_Vertical',
    'J9':  'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    'J10': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    'J11': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    'J12': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    'J13': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    'J14': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    'J15': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    'J16': 'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    '5Vin':  'TerminalBlock:TerminalBlock_MaiXu_MX126-5.0-02P_1x02_P5.00mm',
    '24Vin': 'TerminalBlock:TerminalBlock_MaiXu_MX126-5.0-02P_1x02_P5.00mm',
    # 5x20mm clip holder on the 24V input. Swap for a blade holder or PTC if
    # that suits the enclosure better -- this is a mechanical preference.
    'F1': 'Fuse:Fuseholder_Clip-5x20mm_Littelfuse_111_Inline_P20.00x5.00mm_D1.05mm_Horizontal',
}


# A reference designator must start with a letter; "5Vin" / "24Vin" do not, so
# KiCad treats them as unannotated (it reports them as "5Vin1"/"24Vin1" and
# raises an annotation error, which blocks a clean PCB update). Rename them and
# move the human-readable name into Value, matching the other connectors.
BASE_RENAMES = {
    '5Vin':  ('J37', '5V_In'),
    '24Vin': ('J38', '24V_In'),
}


def patch_base_symbols(content):
    """Assign footprints, and fix the two invalid reference designators.

    One forward pass is enough because KiCad writes a symbol's fields in a
    fixed order: Reference, Value, Footprint, ... then the instances block that
    repeats the reference. `cur` always holds the ORIGINAL reference so lookups
    keep working after the rename is written out.
    """
    out = []
    cur = None
    filled = 0
    renamed = 0
    for line in content.split('\n'):
        s = line.strip()
        if s.startswith('(property "Reference" "'):
            cur = s.split('"')[3]
            if cur in BASE_RENAMES:
                line = line.replace('"Reference" "%s"' % cur,
                                    '"Reference" "%s"' % BASE_RENAMES[cur][0])
        elif s.startswith('(property "Value" "') and cur in BASE_RENAMES:
            old = s.split('"')[3]
            line = line.replace('"Value" "%s"' % old,
                                '"Value" "%s"' % BASE_RENAMES[cur][1])
        elif s.startswith('(property "Footprint" ""') and cur in BASE_FOOTPRINTS:
            line = line.replace('(property "Footprint" ""',
                                '(property "Footprint" "%s"' % BASE_FOOTPRINTS[cur])
            filled += 1
        elif cur in BASE_RENAMES and s == '(reference "%s")' % cur:
            line = line.replace('(reference "%s")' % cur,
                                '(reference "%s")' % BASE_RENAMES[cur][0])
            renamed += 1
        out.append(line)
    if filled != len(BASE_FOOTPRINTS):
        sys.exit('build: expected to fill %d base footprints, filled %d'
                 % (len(BASE_FOOTPRINTS), filled))
    if renamed != len(BASE_RENAMES):
        sys.exit('build: expected to rename %d references, renamed %d'
                 % (len(BASE_RENAMES), renamed))
    print('build: assigned %d base footprints, renamed %d references'
          % (filled, renamed))
    return '\n'.join(out)


def extract_tmc_symbol():
    """Pull the tmc2209 symbol out of New_Library and re-indent it one level
    so it nests inside the schematic's (lib_symbols ...) block."""
    with open(SYMLIB, encoding='utf-8') as f:
        lines = f.readlines()
    start = next(i for i, l in enumerate(lines)
                 if l.strip().startswith('(symbol "tmc2209"'))
    depth = 0
    end = None
    for i in range(start, len(lines)):
        depth += lines[i].count('(') - lines[i].count(')')
        if depth == 0:
            end = i
            break
    if end is None:
        sys.exit('build: could not find end of tmc2209 symbol block')
    block = lines[start:end + 1]
    block[0] = block[0].replace('(symbol "tmc2209"',
                                '(symbol "New_Library:tmc2209"')
    return ''.join('\t' + l if l.strip() else l for l in block)


def splice(content, anchor, insertion, what):
    n = content.count(anchor)
    if n != 1:
        sys.exit(f'build: expected exactly 1 {what} anchor, found {n}')
    return content.replace(anchor, insertion, 1)


def main():
    for path in (BASE, SYMLIB):
        if not os.path.exists(path):
            sys.exit(f'build: missing required input {path}')

    with open(BASE, encoding='utf-8') as f:
        content = f.read()

    content = content.replace('(project "pantiltslide"',
                              f'(project "{PROJECT_NAME}"')
    content = content.replace('(paper "A4")', f'(paper "{PAPER}")')
    content = patch_base_symbols(content)

    tmc = extract_tmc_symbol()
    content = splice(content, LIBSYM_ANCHOR,
                     '\t\t\t(embedded_fonts no)\n\t\t)\n' + tmc + '\t)\n\t(junction\n',
                     'lib_symbols')

    subprocess.run([sys.executable, os.path.join(HERE, 'gen_wiring.py')], check=True)
    with open(BODY, encoding='utf-8') as f:
        body = f.read()
    content = splice(content, BODY_ANCHOR, body + BODY_ANCHOR, 'sheet_instances')

    with open(OUT, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f'build: wrote {OUT}')

    for check in ('verify_wiring.py', 'verify_footprints.py'):
        r = subprocess.run([sys.executable, os.path.join(HERE, check)])
        if r.returncode != 0:
            sys.exit(f'build: {check} FAILED -- do not use this file')
    print('build: OK')


if __name__ == '__main__':
    main()
