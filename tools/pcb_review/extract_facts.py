"""Pull hard, checkable facts out of a .kicad_pcb so a reviewer doesn't have to
guess them from a 700 KB s-expression diff.

This is the deterministic half of the PR review bot. It answers the questions
that are pure measurement -- how big is the board, how many joints of each
technology, does anything physically collide, is anything below fab minimums --
and leaves engineering judgement to the model that reads its output.

Two rules kept this useful:

  1. Never editorialise. Emit the number and the rule it violated. The reviewer
     stage decides whether a 0.6 mm VM trace is a problem on *this* board.
  2. Say what was NOT checked. Real DRC is `kicad-cli pcb drc`; the geometry
     here is bounding-box approximate and is labelled as such wherever it is.

Usage:
    python extract_facts.py BOARD.kicad_pcb [--json out.json] [--md out.md]

Exit status is always 0 -- findings are data, not a build failure. The workflow
decides what blocks a merge.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sexp  # noqa: E402

# Conservative standard-process limits. These are the "no upcharge, no
# engineering query" numbers at JLCPCB/PCBWay for 4-layer 1 oz; a board that
# clears them will not generate a DFM email from any prototype house. Override
# with --fab-profile if a shop's advanced process has been paid for.
FAB = {
    "min_track_mm": 0.127,        # 5 mil
    "min_drill_mm": 0.20,
    "min_annular_ring_mm": 0.13,
    "min_via_diameter_mm": 0.45,
    "min_silk_text_height_mm": 0.80,
    "min_silk_text_thickness_mm": 0.15,
    "min_edge_clearance_mm": 0.20,
}

# Board outlines at or under one of these hits a cheaper price tier at most
# prototype houses. Purely informational.
PRICE_BREAKS_MM = [(100, 100), (100, 150), (150, 150), (200, 200)]

POWER_NET_HINTS = ("VM", "VMOT", "24V", "12V", "VIN", "VBUS", "5V", "3V3", "3.3V", "VCC", "GND")

CRITICAL, MILD, AWARE, RECOMMEND = "critical", "mild", "aware", "recommendation"


class Findings:
    def __init__(self):
        self.items = []

    def add(self, severity, rule, message, evidence=None):
        self.items.append(
            {"severity": severity, "rule": rule, "message": message, "evidence": evidence or []}
        )

    def by_severity(self, severity):
        return [f for f in self.items if f["severity"] == severity]


# --------------------------------------------------------------------------
# geometry helpers
# --------------------------------------------------------------------------

def _xy_pairs(node):
    """Every (x, y) coordinate mentioned anywhere under *node*."""
    out = []
    for key in ("start", "end", "center", "mid", "xy"):
        for item in sexp.descend(node, key):
            if len(item) >= 3:
                out.append((sexp.num(item[1]), sexp.num(item[2])))
    return out


def _rotate(px, py, x0, y0, deg):
    """KiCad footprint-local -> board coordinates. Y is screen-down, so a
    positive rotation is clockwise on screen and the sine term is negated."""
    th = math.radians(deg)
    c, s = math.cos(th), math.sin(th)
    return (x0 + px * c + py * s, y0 - px * s + py * c)


def _bbox(points):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return (min(xs), max(xs), min(ys), max(ys))


def _overlap(a, b):
    ox = min(a[1], b[1]) - max(a[0], b[0])
    oy = min(a[3], b[3]) - max(a[2], b[2])
    return (ox, oy) if ox > 0 and oy > 0 else None


def net_of(node):
    """Net name from a pad/segment/via, across KiCad revisions.

    KiCad <=9 writes `(net 3 "GND")` -- code then name. KiCad 10 writes
    `(net "GND")` -- name only, with no board-level code table. Both collapse
    to the name, which is the only key any check here needs.
    """
    net = sexp.first(node, "net")
    if not net or len(net) < 2:
        return ""
    if len(net) >= 3 and isinstance(net[2], str):
        return net[2]
    val = net[1]
    if isinstance(val, str) and not val.replace(".", "").replace("-", "").isdigit():
        return val
    return ""


# --------------------------------------------------------------------------
# parsing
# --------------------------------------------------------------------------

def read_board(path):
    """Parse and sanity-check. A file that does not parse, or parses to
    something that is not a board, must raise -- silently reporting a corrupt
    file as a board with zero findings would read as a clean review."""
    if not os.path.exists(path):
        raise SystemExit(f"extract_facts: no such file: {path}")
    try:
        root = sexp.load(path)
    except ValueError as exc:
        raise SystemExit(f"extract_facts: {path} is not valid s-expression: {exc}")
    if not isinstance(root, list) or not root or root[0] != "kicad_pcb":
        raise SystemExit(f"extract_facts: {path} is not a kicad_pcb document")
    top = [n for n in root if isinstance(n, list)]
    if not any(n and n[0] == "footprint" for n in top) and \
       not any(n and n[0] == "layers" for n in top):
        raise SystemExit(f"extract_facts: {path} has no layers and no footprints")
    return root, top


def board_outline(top):
    """Bounding box of top-level Edge.Cuts graphics, plus the item count so a
    reviewer can tell a single rect from a stitched-together outline."""
    pts, items = [], 0
    for node in top:
        if not node or node[0] not in (
            "gr_line", "gr_rect", "gr_arc", "gr_circle", "gr_poly", "gr_curve", "gr_bbox"
        ):
            continue
        layer = sexp.first(node, "layer")
        if not layer or len(layer) < 2 or layer[1] != "Edge.Cuts":
            continue
        items += 1
        pts.extend(_xy_pairs(node))
    if not pts:
        return None
    x0, x1, y0, y1 = _bbox(pts)
    return {
        "items": items,
        "width_mm": round(x1 - x0, 2),
        "height_mm": round(y1 - y0, 2),
        "area_mm2": round((x1 - x0) * (y1 - y0), 1),
        "bbox": [round(v, 3) for v in (x0, x1, y0, y1)],
    }


def copper_layers(top):
    node = None
    for n in top:
        if n and n[0] == "layers":
            node = n
            break
    if node is None:
        return []
    names = []
    for entry in node[1:]:
        if isinstance(entry, list) and len(entry) >= 3 and entry[2] == "signal":
            names.append(entry[1])
    return names


def parse_footprints(top):
    out = []
    for fp in top:
        if not fp or fp[0] != "footprint":
            continue
        lib = fp[1] if len(fp) > 1 and isinstance(fp[1], str) else "?"
        at = sexp.first(fp, "at")
        x0, y0 = sexp.num(at[1]) if at else 0.0, sexp.num(at[2]) if at else 0.0
        rot = sexp.num(at[3]) if at and len(at) > 3 else 0.0
        layer = sexp.first(fp, "layer")
        side = "B" if layer and len(layer) > 1 and str(layer[1]).startswith("B") else "F"

        ref = value = None
        for prop in sexp.children(fp, "property"):
            if len(prop) >= 3 and prop[1] == "Reference":
                ref = prop[2]
            elif len(prop) >= 3 and prop[1] == "Value":
                value = prop[2]

        attrs = sexp.first(fp, "attr") or []
        tech = "SMD" if "smd" in attrs else "THT"

        pads = []
        for pad in sexp.children(fp, "pad"):
            ptype = pad[2] if len(pad) > 2 else "?"
            pat = sexp.first(pad, "at")
            px = sexp.num(pat[1]) if pat else 0.0
            py = sexp.num(pat[2]) if pat else 0.0
            size = sexp.first(pad, "size")
            sw = sexp.num(size[1]) if size else 0.0
            sh = sexp.num(size[2]) if size and len(size) > 2 else sw
            drill_node = sexp.first(pad, "drill")
            drill = None
            if drill_node:
                vals = [sexp.num(v) for v in drill_node[1:] if not isinstance(v, list)
                        and str(v) not in ("oval",)]
                vals = [v for v in vals if v]
                drill = max(vals) if vals else None
            gx, gy = _rotate(px, py, x0, y0, rot)
            pads.append({
                "number": pad[1] if len(pad) > 1 else "?",
                "type": ptype,
                "x": gx, "y": gy,
                "w": sw, "h": sh,
                "drill": drill,
                "net": net_of(pad),
            })

        court = []
        for shape in fp:
            if not isinstance(shape, list) or not shape[0].startswith("fp_"):
                continue
            lay = sexp.first(shape, "layer")
            if lay and len(lay) > 1 and str(lay[1]).endswith("CrtYd"):
                court.extend(_rotate(px, py, x0, y0, rot) for px, py in _xy_pairs(shape))

        pad_pts = [(p["x"], p["y"]) for p in pads]
        out.append({
            "ref": ref or "?",
            "value": value or "",
            "lib": lib,
            "side": side,
            "tech": tech,
            "x": x0, "y": y0, "rot": rot,
            "pads": pads,
            "courtyard_bbox": _bbox(court) if court else (_bbox(pad_pts) if pad_pts else None),
            "pad_bbox": _bbox(pad_pts) if pad_pts else None,
        })
    return out


def parse_copper(top):
    tracks, vias, zones = [], [], 0
    for node in top:
        if not node:
            continue
        if node[0] == "segment" or node[0] == "arc":
            width = sexp.first(node, "width")
            layer = sexp.first(node, "layer")
            tracks.append({
                "width": sexp.num(width[1]) if width else 0.0,
                "layer": layer[1] if layer and len(layer) > 1 else "?",
                "net": net_of(node),
            })
        elif node[0] == "via":
            size = sexp.first(node, "size")
            drill = sexp.first(node, "drill")
            vias.append({
                "diameter": sexp.num(size[1]) if size else 0.0,
                "drill": sexp.num(drill[1]) if drill else 0.0,
                "net": net_of(node),
            })
        elif node[0] == "zone":
            zones += 1
    return tracks, vias, zones


def parse_nets(top, footprints, tracks, vias):
    """Every net name on the board. KiCad 10 has no board-level net table, so
    this is derived from what pads and copper actually reference."""
    names = set()
    for node in top:
        if node and node[0] == "net" and len(node) >= 3 and isinstance(node[2], str):
            names.add(node[2])
    for f in footprints:
        names.update(p["net"] for p in f["pads"] if p["net"])
    names.update(t["net"] for t in tracks if t["net"])
    names.update(v["net"] for v in vias if v["net"])
    names.discard("")
    return sorted(names)


def parse_silk_text(top, footprints_raw):
    """Silkscreen text sizes, board-level and inside footprints."""
    out = []
    for node in sexp.descend(top, "effects"):
        font = sexp.first(node, "font")
        if not font:
            continue
        size = sexp.first(font, "size")
        thick = sexp.first(font, "thickness")
        if size and len(size) > 2:
            out.append({
                "height": sexp.num(size[1]),
                "width": sexp.num(size[2]),
                "thickness": sexp.num(thick[1]) if thick else None,
            })
    return out


# --------------------------------------------------------------------------
# checks
# --------------------------------------------------------------------------

def run_checks(board, fab, find):
    outline = board["outline"]
    fps = board["footprints"]

    # ---- outline -----------------------------------------------------
    if outline is None:
        find.add(CRITICAL, "outline.missing",
                 "No Edge.Cuts geometry found. The board has no outline and cannot be fabricated.")
    else:
        if outline["items"] == 1:
            find.add(AWARE, "outline.single_item",
                     f"Outline is a single Edge.Cuts item ({outline['width_mm']} x "
                     f"{outline['height_mm']} mm). Confirm it is a closed shape and that "
                     f"any internal cutouts are intentional omissions.")
        w, h = outline["width_mm"], outline["height_mm"]
        fits = [f"{a}x{b}" for a, b in PRICE_BREAKS_MM
                if (w <= a and h <= b) or (w <= b and h <= a)]
        if fits:
            find.add(AWARE, "outline.price_break",
                     f"{w} x {h} mm fits standard price tier(s): {', '.join(fits)}.")
        else:
            nearest = min(PRICE_BREAKS_MM, key=lambda p: max(w - p[0], h - p[1]))
            over = round(max(w - nearest[0], h - nearest[1]), 1)
            find.add(RECOMMEND, "outline.price_break",
                     f"{w} x {h} mm misses the {nearest[0]}x{nearest[1]} mm tier by {over} mm on "
                     f"the long axis. Check whether the overage is reclaimable before ordering.")

    # ---- mounting holes ----------------------------------------------
    holes = [f for f in fps
             if "mountinghole" in f["lib"].lower()
             or any(p["type"] == "np_thru_hole" for p in f["pads"])]
    if not holes:
        find.add(CRITICAL, "mech.no_mounting_holes",
                 "Zero mounting holes and zero non-plated through-holes on the board. "
                 "Nothing mechanically retains it, and connector insertion force has no "
                 "load path except the solder joints.")
    else:
        find.add(AWARE, "mech.mounting_holes", f"{len(holes)} mounting hole(s): "
                 f"{', '.join(sorted(h['ref'] for h in holes))}.")

    # ---- designators --------------------------------------------------
    refs = [f["ref"] for f in fps]
    dupes = [r for r, c in Counter(refs).items() if c > 1]
    if dupes:
        find.add(CRITICAL, "bom.duplicate_refs",
                 f"{len(dupes)} duplicated designator(s) -- the BOM and CPL cannot be "
                 f"unambiguously matched to placements.", sorted(dupes))
    unann = sorted(r for r in refs if r.endswith("?") or r in ("", "?"))
    if unann:
        find.add(CRITICAL, "bom.unannotated",
                 f"{len(unann)} unannotated designator(s).", unann)

    # ---- footprint collisions (bbox approximation) ---------------------
    hits = []
    boxed = [f for f in fps if f["courtyard_bbox"]]
    for i, a in enumerate(boxed):
        for b in boxed[i + 1:]:
            if a["side"] != b["side"]:
                continue
            ov = _overlap(a["courtyard_bbox"], b["courtyard_bbox"])
            if ov:
                hits.append((a["ref"], b["ref"], a["side"], ov[0], ov[1]))
    if hits:
        hits.sort(key=lambda h: -(h[3] * h[4]))
        find.add(CRITICAL, "dfm.courtyard_overlap",
                 f"{len(hits)} same-side courtyard overlap(s) -- parts collide and cannot both "
                 f"be fitted. (Bounding-box approximation; kicad-cli DRC is authoritative.)",
                 [f"{a} <-> {b} on {s}: {ox:.2f} x {oy:.2f} mm" for a, b, s, ox, oy in hits[:20]])

    # ---- parts outside the outline -------------------------------------
    if outline:
        x0, x1, y0, y1 = outline["bbox"]
        outside = []
        for f in fps:
            bb = f["courtyard_bbox"]
            if not bb:
                continue
            if bb[0] < x0 - 0.01 or bb[1] > x1 + 0.01 or bb[2] < y0 - 0.01 or bb[3] > y1 + 0.01:
                outside.append(f["ref"])
        if outside:
            find.add(CRITICAL, "dfm.outside_outline",
                     f"{len(outside)} footprint(s) extend past the board outline.",
                     sorted(outside)[:30])

    # ---- drills and annular rings ---------------------------------------
    small_drill, thin_ring = [], []
    for f in fps:
        for p in f["pads"]:
            if not p["drill"]:
                continue
            if p["drill"] < fab["min_drill_mm"] - 1e-6:
                small_drill.append(f"{f['ref']}.{p['number']} drill {p['drill']:.3f} mm")
            if p["type"] == "thru_hole":
                ring = (min(p["w"], p["h"]) - p["drill"]) / 2.0
                if ring < fab["min_annular_ring_mm"] - 1e-6:
                    thin_ring.append(f"{f['ref']}.{p['number']} ring {ring:.3f} mm")
    if small_drill:
        find.add(CRITICAL, "dfm.drill_below_min",
                 f"{len(small_drill)} hole(s) below the {fab['min_drill_mm']} mm standard-process "
                 f"minimum.", small_drill[:20])
    if thin_ring:
        find.add(CRITICAL, "dfm.annular_ring",
                 f"{len(thin_ring)} pad(s) below the {fab['min_annular_ring_mm']} mm annular ring "
                 f"minimum -- drill breakout risk.", thin_ring[:20])

    # ---- track widths ----------------------------------------------------
    tracks = board["tracks"]
    if tracks:
        widths = sorted({round(t["width"], 3) for t in tracks})
        thin = [t for t in tracks if t["width"] < fab["min_track_mm"] - 1e-6]
        if thin:
            find.add(CRITICAL, "dfm.track_below_min",
                     f"{len(thin)} track segment(s) below the {fab['min_track_mm']} mm minimum.")
        find.add(AWARE, "route.track_widths",
                 f"{len(tracks)} segments across {len(widths)} distinct widths: "
                 f"{', '.join(f'{w} mm' for w in widths[:12])}.")

        # power nets on thin copper
        by_net = defaultdict(list)
        for t in tracks:
            if t["net"]:
                by_net[t["net"]].append(t["width"])
        weak = []
        for name, ws in by_net.items():
            bare = name.split("/")[-1].upper()
            if any(bare.startswith(h) for h in POWER_NET_HINTS) and min(ws) < 0.5:
                weak.append(f"{name}: narrowest {min(ws):.3f} mm")
        if weak:
            find.add(MILD, "route.power_track_width",
                     f"{len(weak)} power-ish net(s) routed narrower than 0.5 mm. Confirm against "
                     f"the actual current, especially motor-supply rails.", weak[:15])

    # ---- vias --------------------------------------------------------------
    for v in board["vias"]:
        if v["drill"] and v["drill"] < fab["min_drill_mm"] - 1e-6:
            find.add(CRITICAL, "dfm.via_drill",
                     f"Via drill {v['drill']} mm below {fab['min_drill_mm']} mm minimum.")
            break
    for v in board["vias"]:
        if v["diameter"] and v["diameter"] < fab["min_via_diameter_mm"] - 1e-6:
            find.add(MILD, "dfm.via_diameter",
                     f"Via diameter {v['diameter']} mm below the {fab['min_via_diameter_mm']} mm "
                     f"standard-process minimum.")
            break

    # ---- routing completeness ----------------------------------------------
    pad_nets = Counter()
    for f in fps:
        for p in f["pads"]:
            if p["net"]:
                pad_nets[p["net"]] += 1
    copper_nets = {t["net"] for t in tracks} | {v["net"] for v in board["vias"]}
    multi = {c for c, n in pad_nets.items() if n >= 2}
    unrouted = sorted(multi - copper_nets)
    if unrouted:
        names = unrouted
        find.add(CRITICAL, "route.unrouted",
                 f"At least {len(unrouted)} of {len(multi)} multi-pad nets have no copper at all. "
                 f"(Lower bound -- partially routed nets are not detected here; "
                 f"kicad-cli DRC reports the true ratsnest.)", sorted(names)[:30])
    elif tracks:
        find.add(AWARE, "route.state",
                 f"All {len(multi)} multi-pad nets have some copper. Completeness still needs DRC.")

    # ---- silkscreen text ----------------------------------------------------
    tiny = [t for t in board["text"]
            if t["height"] and t["height"] < fab["min_silk_text_height_mm"] - 1e-6]
    if tiny:
        find.add(MILD, "dfm.silk_text_height",
                 f"{len(tiny)} text item(s) below the {fab['min_silk_text_height_mm']} mm "
                 f"silkscreen legibility minimum -- may print illegibly or be dropped.")

    # ---- assembly economics --------------------------------------------------
    inv = board["inventory"]
    if inv["smd_front"] and inv["smd_back"]:
        find.add(MILD, "asm.double_sided_reflow",
                 f"Surface-mount parts on both sides ({inv['smd_front']} front / "
                 f"{inv['smd_back']} back) requires two reflow passes, raising assembly setup "
                 f"cost and complicating hand assembly.")
    find.add(AWARE, "asm.joint_counts",
             f"{inv['tht_joints']} through-hole joints vs {inv['smd_joints']} surface-mount "
             f"joints. Through-hole is the expensive line on an assembly quote and the slow "
             f"line by hand.")
    if inv["smd_back"] and not inv["smd_front"]:
        find.add(AWARE, "asm.order_of_operations",
                 f"All {inv['smd_back']} surface-mount parts are on the back, so it is a "
                 f"single reflow pass. Reflow before any through-hole goes in -- connector "
                 f"and socket plastics will not survive a hotplate or oven.")
    if inv["tht_back"] and inv["tht_front"]:
        find.add(AWARE, "asm.tht_both_sides",
                 f"Through-hole parts on both sides ({inv['tht_front']} front / "
                 f"{inv['tht_back']} back). The board cannot lie flat while the second side "
                 f"is soldered, and wave/selective soldering is not an option.")


# --------------------------------------------------------------------------
# report
# --------------------------------------------------------------------------

def build(path, fab):
    root, top = read_board(path)
    fps = parse_footprints(top)
    tracks, vias, zones = parse_copper(top)

    inv = {
        "footprints": len(fps),
        "smd_front": sum(1 for f in fps if f["tech"] == "SMD" and f["side"] == "F"),
        "smd_back": sum(1 for f in fps if f["tech"] == "SMD" and f["side"] == "B"),
        "tht_front": sum(1 for f in fps if f["tech"] == "THT" and f["side"] == "F"),
        "tht_back": sum(1 for f in fps if f["tech"] == "THT" and f["side"] == "B"),
        "smd_joints": sum(len([p for p in f["pads"] if p["type"] == "smd"])
                          for f in fps),
        "tht_joints": sum(len([p for p in f["pads"] if p["type"] == "thru_hole"])
                          for f in fps),
    }

    board = {
        "path": path,
        "outline": board_outline(top),
        "copper_layers": copper_layers(top),
        "footprints": fps,
        "tracks": tracks,
        "vias": vias,
        "zones": zones,
        "net_names": parse_nets(top, fps, tracks, vias),
        "text": parse_silk_text(top, fps),
        "inventory": inv,
    }

    find = Findings()
    run_checks(board, fab, find)
    return board, find


def render_md(board, find, fab):
    o = board["outline"]
    inv = board["inventory"]
    L = []
    A = L.append
    A(f"### Measured facts -- `{os.path.basename(board['path'])}`")
    A("")
    A("| | |")
    A("|---|---|")
    if o:
        A(f"| Outline | {o['width_mm']} x {o['height_mm']} mm ({o['area_mm2']:.0f} mm2) |")
    else:
        A("| Outline | **none found** |")
    A(f"| Copper layers | {len(board['copper_layers'])} "
      f"({', '.join(board['copper_layers'])}) |")
    A(f"| Footprints | {inv['footprints']} |")
    A(f"| SMD | {inv['smd_front']} front / {inv['smd_back']} back "
      f"({inv['smd_joints']} joints) |")
    A(f"| Through-hole | {inv['tht_front']} front / {inv['tht_back']} back "
      f"({inv['tht_joints']} joints) |")
    A(f"| Copper | {len(board['tracks'])} segments, {len(board['vias'])} vias, "
      f"{board['zones']} zones |")
    A(f"| Nets | {len(board['net_names'])} |")
    A("")

    labels = [
        (CRITICAL, "Critical"),
        (MILD, "Mild"),
        (AWARE, "Be aware"),
        (RECOMMEND, "Recommendation"),
    ]
    A("### Deterministic checks")
    A("")
    for sev, label in labels:
        items = find.by_severity(sev)
        if not items:
            continue
        A(f"**{label}**")
        A("")
        for f in items:
            A(f"- `{f['rule']}` -- {f['message']}")
            for ev in f["evidence"][:12]:
                A(f"  - {ev}")
            if len(f["evidence"]) > 12:
                A(f"  - ...and {len(f['evidence']) - 12} more")
        A("")
    A("_Not checked here: copper clearance, silkscreen-over-pad, impedance, thermal relief, "
      "net connectivity. Those come from `kicad-cli pcb drc`. Geometry above is bounding-box "
      "approximate._")
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("board")
    ap.add_argument("--json", dest="json_out")
    ap.add_argument("--md", dest="md_out")
    args = ap.parse_args()

    board, find = build(args.board, FAB)
    md = render_md(board, find, FAB)

    if args.md_out:
        with open(args.md_out, "w", encoding="utf-8") as fh:
            fh.write(md + "\n")
    else:
        print(md)

    if args.json_out:
        payload = {
            "board": os.path.basename(args.board),
            "outline": board["outline"],
            "copper_layers": board["copper_layers"],
            "inventory": board["inventory"],
            "counts": {
                "tracks": len(board["tracks"]),
                "vias": len(board["vias"]),
                "zones": board["zones"],
                "nets": len(board["net_names"]),
            },
            "findings": find.items,
        }
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
