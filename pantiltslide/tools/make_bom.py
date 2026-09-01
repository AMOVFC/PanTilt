#!/usr/bin/env python3
"""Regenerate the PCBWay BOM and placement file from the schematic.

    python pantiltslide/tools/make_bom.py

The sourcing data (MPN, Manufacturer, LCSC, AssemblyQty, Notes) lives as
fields on the symbols themselves, so this reads the design rather than a
hand-maintained spreadsheet.  Edit the fields in Eeschema via
Tools -> Edit Symbol Fields..., then re-run this.

Two parts are one footprint but more than one physical part, which no BOM
exporter can know on its own -- hence the `AssemblyQty` field:

    F1      one footprint, TWO fuse clips
    U1-U4   one footprint each, TWO 1x8 socket strips each

The Qty column written here is the real number to order.
"""

import csv
import subprocess
import sys
from pathlib import Path

PROJ = Path(__file__).resolve().parent.parent
NAME = "pantiltslide_full_turnkey"
CLI = Path(r"C:/Program Files/KiCad/10.0/bin/kicad-cli.exe")

RAW = PROJ / "tools" / "_bom_raw.csv"
BOM = PROJ / "bom_pcbway.csv"
CPL = PROJ / "cpl_pcbway.csv"

FIELDS = "Reference,Footprint,MPN,Manufacturer,LCSC,QUANTITY,AssemblyQty,Notes"
LABELS = "Designator,Package,MPN,Manufacturer,LCSC,Qty,QtyPerPos,Notes"


def run(*args):
    subprocess.run([str(CLI)] + list(args), check=True, stdout=subprocess.DEVNULL)


def main() -> None:
    run("sch", "export", "bom", "--group-by", "MPN", "--fields", FIELDS,
        "--labels", LABELS, "--output", str(RAW), str(PROJ / (NAME + ".kicad_sch")))

    rows = list(csv.DictReader(RAW.open(encoding="utf-8")))
    out = []
    for i, r in enumerate(rows, 1):
        per = int(r["QtyPerPos"] or 1)
        positions = int(r["Qty"])
        out.append({
            "Item": i,
            "Designator": r["Designator"],
            "Qty": positions * per,
            "Package": (r["Package"] or "").split(":")[-1],
            "Manufacturer": r["Manufacturer"],
            "MPN": r["MPN"],
            "LCSC": r["LCSC"],
            "Type": "SMD" if "_SMD:" in (r["Package"] or "") else "THT",
            "Notes": r["Notes"],
        })

    with BOM.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(out[0]))
        w.writeheader()
        w.writerows(out)
    RAW.unlink(missing_ok=True)

    run("pcb", "export", "pos", "--format", "csv", "--units", "mm",
        "--side", "both", "--output", str(CPL), str(PROJ / (NAME + ".kicad_pcb")))

    smd = sum(r["Qty"] for r in out if r["Type"] == "SMD")
    tht = sum(r["Qty"] for r in out if r["Type"] == "THT")
    print("wrote %s  (%d unique parts)" % (BOM.name, len(out)))
    print("wrote %s" % CPL.name)
    print()
    print("PCBWay quote form:")
    print("  Number of Unique Parts     : %d" % len(out))
    print("  Number of SMD Parts        : %d" % smd)
    print("  Number of BGA/QFP Parts    : 0")
    print("  Number of Through-Hole Parts: %d" % tht)

    blank = [r["Designator"] for r in out if not r["MPN"]]
    if blank:
        print("\nWARNING - no MPN on:", ", ".join(blank))
        sys.exit(1)


if __name__ == "__main__":
    main()
