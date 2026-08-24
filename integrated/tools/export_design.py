# -*- coding: utf-8 -*-
"""Dump the verified schematic model to design.json for the pcbnew builder.

Runs on system python (imports gen_sch); build_pcb.py runs on KiCad's bundled
python, which cannot import gen_sch, hence the JSON hand-off.
"""
import json, os
import gen_sch as g

# Which subsystem each reference belongs to -- drives PCB placement clusters.
def cluster_of(c):
    r = c.ref
    if r.startswith("#"):
        return None  # power/flag symbols are virtual, never on the PCB
    v = (c.value or "")
    if r in ("J1",) or v in ("24V_In",):
        return "power"
    if r in ("U1", "U2") or v in ("LM2596S-5", "AMS1117-3.3"):
        return "power"
    if r == "F1" or r == "L1" or r == "D1":
        return "power"
    if v.startswith("Motor_"):
        return "motorconn"
    if v.startswith("AS5600_") or v in ("OLED", "Enc_Jog", "Enc_Angle"):
        return "ioconn"
    if v.startswith("Limit_"):
        return "ioconn"
    if v == "USB_PROG":
        return "usb"
    if r == "U3":
        return "mcu"
    return None  # decided below by net association


def main():
    comps = []
    for c in g.B.components:
        if c.ref.startswith("#"):
            continue
        comps.append({
            "ref": c.ref,
            "value": c.value,
            "fp": c.footprint,
            "cluster": cluster_of(c),
            "sch_x": c.x,
            "sch_y": c.y,
        })

    # Anything not explicitly clustered gets assigned to the cluster of the
    # driver/IC it shares the most nets with, so decoupling caps and sense
    # resistors land next to their own chip rather than in a random pile.
    owner = {}
    for c in g.B.components:
        if c.ref in ("U4", "U5", "U6", "U7", "U8", "U3"):
            owner[c.ref] = {"U4": "drv_slide", "U5": "drv_pan", "U6": "drv_tilt",
                            "U7": "drv_z", "U8": "mux", "U3": "mcu"}[c.ref]
    net_members = {n: [c.ref for c, p in m] for n, m in g.B.nets.items()}
    GLOBAL = {"GND", "+3V3", "+5V", "+24V"}
    for cd in comps:
        if cd["cluster"]:
            continue
        score = {}
        for n, refs in net_members.items():
            if n in GLOBAL or cd["ref"] not in refs:
                continue
            for r in refs:
                if r in owner:
                    score[owner[r]] = score.get(owner[r], 0) + 1
        cd["cluster"] = max(score, key=score.get) if score else None
    for r, cl in owner.items():
        for cd in comps:
            if cd["ref"] == r:
                cd["cluster"] = cl

    # Bulk/bypass caps touch only GND and a power rail, so nets can't tell us
    # whose they are. The schematic already groups them beside their own chip,
    # so fall back to nearest already-clustered neighbour in schematic space --
    # this is what keeps each TMC2209's VS bulk cap with that driver.
    anchored = [c for c in comps if c["cluster"]]
    for cd in comps:
        if cd["cluster"]:
            continue
        near = min(anchored, key=lambda a: (a["sch_x"] - cd["sch_x"]) ** 2
                                           + (a["sch_y"] - cd["sch_y"]) ** 2)
        cd["cluster"] = near["cluster"]

    # Nets come from KiCad's own exported netlist, NOT from g.B.nets. The model
    # omits things KiCad derives itself -- the USB-C stacked VBUS/GND pads that
    # share a coordinate, and the "unconnected-(...)" nets KiCad assigns to
    # no-connect pins. Feeding the model straight to the PCB left all of those
    # pads netless, which schematic-parity correctly flagged.
    import subprocess, tempfile
    sch = os.path.join(os.path.dirname(__file__), "pantiltslide_integrated.kicad_sch")
    netfile = os.path.join(tempfile.gettempdir(), "_parity.net")
    subprocess.run([r"C:\Program Files\KiCad\9.0\bin\kicad-cli.exe", "sch",
                    "export", "netlist", sch, "-o", netfile],
                   check=True, capture_output=True)
    ntxt = open(netfile, encoding="utf-8").read()

    import re
    nets = {}
    for m in re.finditer(r'\(net \(code "\d+"\) \(name "([^"]+)"\)(.*?)(?=\n    \(net \(code|\Z)',
                         ntxt, re.S):
        name = m.group(1)
        pads = [[r, p] for r, p in
                re.findall(r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)', m.group(2))]
        if pads:
            nets[name] = pads

    # cross-check the netlist's component list against ours
    nl_refs = set(re.findall(r'\(comp \(ref "([^"]+)"\)', ntxt))
    ours = set(c["ref"] for c in comps)
    if nl_refs != ours:
        print("  WARNING ref mismatch; netlist-only:", sorted(nl_refs - ours),
              " ours-only:", sorted(ours - nl_refs))

    out = {"components": comps, "nets": nets, "owner": g.B.owner}
    p = os.path.join(os.path.dirname(__file__), "design.json")
    json.dump(out, open(p, "w", encoding="utf-8"), indent=1)
    from collections import Counter
    print("wrote", p, len(comps), "components,", len(nets), "nets")
    print("clusters:", dict(Counter(c["cluster"] for c in comps)))


if __name__ == "__main__":
    main()
