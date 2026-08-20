"""Check that every symbol in the built schematic has a footprint AND that the
footprint actually exists on disk.

This is the gate for the two failure modes that "Update PCB from Schematic"
reports as `Cannot add <ref>`:
  1. no footprint assigned at all
  2. footprint assigned but the LIBRARY:NAME does not resolve

Both are invisible until F8 is run, and neither is caught by netlist checks.
Library paths come from KiCad's fp-lib-table (global, plus a project one if
present), so a project-local .pretty resolves the same way KiCad resolves it.
"""
import os
import re
import sys

sys.path.insert(0, r'C:\Users\acard\.claude\plugins\cache\kicad-happy\kicad-happy\2.1.0\skills\kicad\scripts')
from sexp_parser import parse_file, find_all, find_first

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)
SCH = os.path.join(PROJ, 'pantiltslide_full.kicad_sch')

GLOBAL_TABLE = os.path.join(os.environ.get('APPDATA', ''), 'kicad', '10.0', 'fp-lib-table')
PROJECT_TABLE = os.path.join(PROJ, 'fp-lib-table')

# KICAD10_FOOTPRINT_DIR is not always exported to this process; fall back to
# the default install location so the check works from a plain shell.
DEFAULT_FP_DIR = r'C:\Program Files\KiCad\10.0\share\kicad\footprints'


def load_lib_table(path):
    """Map library nickname -> directory. Handles the "KiCad default" Table
    entry by expanding it to the stock footprint directory."""
    libs = {}
    if not os.path.exists(path):
        return libs
    doc = parse_file(path)
    for lib in find_all(doc, 'lib'):
        name = uri = typ = None
        for f in lib:
            if isinstance(f, list) and f[0] == 'name':
                name = f[1]
            elif isinstance(f, list) and f[0] == 'uri':
                uri = f[1]
            elif isinstance(f, list) and f[0] == 'type':
                typ = f[1]
        if not name or not uri:
            continue
        if typ == 'Table':
            # the stock library set: every *.pretty under the footprint dir
            base = DEFAULT_FP_DIR
            if os.path.isdir(base):
                for d in os.listdir(base):
                    if d.endswith('.pretty'):
                        libs.setdefault(d[:-len('.pretty')], os.path.join(base, d))
            continue
        uri = uri.replace('${KIPRJMOD}', PROJ).replace('$(KIPRJMOD)', PROJ)
        # lambda replacement: a Windows path is full of backslashes, which
        # re.sub would otherwise interpret as escape sequences
        uri = re.sub(r'\$\{KICAD\d+_FOOTPRINT_DIR\}', lambda m: DEFAULT_FP_DIR, uri)
        uri = os.path.expandvars(uri)
        if '${' in uri or '$(' in uri:
            continue  # unresolved var (e.g. 3rd-party PCM path) - skip, not our concern
        libs[name] = uri
    return libs


def main():
    libs = {}
    libs.update(load_lib_table(GLOBAL_TABLE))
    libs.update(load_lib_table(PROJECT_TABLE))

    doc = parse_file(SCH)
    problems = []
    checked = 0
    for s in find_all(doc, 'symbol'):
        ref = fp = None
        for p in find_all(s, 'property'):
            if p[1] == 'Reference':
                ref = p[2]
            elif p[1] == 'Footprint':
                fp = p[2]
        if ref is None or ref.startswith('#PWR'):
            continue   # power symbols legitimately have no footprint
        checked += 1
        if not fp:
            problems.append(f'{ref}: no footprint assigned')
            continue
        if ':' not in fp:
            problems.append(f'{ref}: malformed footprint {fp!r} (expected LIBRARY:NAME)')
            continue
        libname, fpname = fp.split(':', 1)
        d = libs.get(libname)
        if d is None:
            problems.append(f'{ref}: library {libname!r} not in any fp-lib-table  ({fp})')
        elif not os.path.isfile(os.path.join(d, fpname + '.kicad_mod')):
            problems.append(f'{ref}: {fpname!r} not found in library {libname!r}')

    print(f'checked {checked} symbols against {len(libs)} libraries, '
          f'problems: {len(problems)}')
    for p in problems:
        print('  ', p)
    if problems:
        sys.exit('FOOTPRINT CHECK FAILED')
    print('ALL FOOTPRINTS RESOLVE')


if __name__ == '__main__':
    main()
