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

    r = subprocess.run([sys.executable, os.path.join(HERE, 'verify_wiring.py')])
    if r.returncode != 0:
        sys.exit('build: VERIFICATION FAILED -- do not use this file')
    print('build: OK')


if __name__ == '__main__':
    main()
