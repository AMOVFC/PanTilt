# -*- coding: utf-8 -*-
"""Build the 4-layer main-board PCB from design.json.

MUST run under KiCad's bundled python:
  "C:/Program Files/KiCad/9.0/bin/python.exe" build_pcb.py
"""
import json, os
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
FPDIR = r"C:\Program Files\KiCad\9.0\share\kicad\footprints"
MM = pcbnew.FromMM


def V(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))


BX0, BY0, BX1, BY1 = 20.0, 20.0, 170.0, 140.0   # board outline, 150 x 120 mm

# Non-overlapping packing rectangles, one per subsystem. Connector clusters sit
# on the board edges; the four drivers get a 2x2 block clear of the MCU.
CLUSTER_RECT = {
    "mcu":       (30.0, 44.0, 88.0, 64.0),
    "usb":       (128.0, 22.0, 167.0, 43.0),
    "mux":       (30.0, 66.0, 88.0, 79.0),
    "power":     (23.0, 82.0, 88.0, 105.0),
    "drv_slide": (92.0, 23.0, 128.0, 62.0),
    "drv_tilt":  (92.0, 66.0, 128.0, 105.0),
    "drv_pan":   (131.0, 45.0, 167.0, 80.0),
    "drv_z":     (131.0, 82.0, 167.0, 120.0),
    "motorconn": (92.0, 121.0, 167.0, 138.0),
    "ioconn":    (23.0, 108.0, 88.0, 138.0),
}

# The ESP32 module's footprint carries a 21mm antenna keepout off its -Y end,
# so it is pinned at the top edge with the antenna overhanging the board rather
# than sterilising 21mm of interior copper.
FIXED = {
    "U3": (57.0, 27.5, 0.0),
    # 3V3 bypass, hard against U3 pin 2 (3V3) at (48.2, 23.5)
    "C7":  (43.5, 22.5, 90.0),
    "C8":  (43.5, 26.0, 90.0),
    "C9":  (43.5, 29.5, 90.0),
    "C10": (43.5, 33.0, 90.0),   # 10uF bulk ceramic, same column
}

PLACE_CLEAR = 1.0   # mm of breathing room added around every courtyard
# Connectors carry two lines of silkscreen (reference above, function below),
# so they need vertical room reserved for text the courtyard does not include.
SILK_TEXT_MM = 0.9
CONN_TEXT_PAD = 2 * (SILK_TEXT_MM + 1.7)   # one text row each side
STEP = 1.0          # mm search grid for collision-free placement


def load_fp(fpid):
    lib, name = fpid.split(":", 1)
    fp = pcbnew.FootprintLoad(os.path.join(FPDIR, lib + ".pretty"), name)
    if fp is None:
        raise RuntimeError("footprint not found: " + fpid)
    # FootprintLoad drops the library nickname; without restoring it every
    # footprint reads as a symbol/footprint mismatch in schematic parity.
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


def extent_box(fp):
    """Courtyard box if the footprint has one, else its full bounding box."""
    for layer in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
        try:
            poly = fp.GetCourtyard(layer)
            if poly and poly.OutlineCount() > 0:
                return poly.BBox()
        except Exception:
            pass
    return fp.GetBoundingBox(False, False)


def main():
    design = json.load(open(os.path.join(HERE, "design.json"), encoding="utf-8"))
    board = pcbnew.CreateEmptyBoard()
    board.SetCopperLayerCount(4)

    # ---- constraints matched to PCBWay's standard (cheapest) process --------
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(4)
    ds.m_TrackMinWidth = MM(0.15)
    ds.m_MinClearance = MM(0.15)
    ds.m_MinThroughDrill = MM(0.2)      # TMC2209 thermal vias are 0.2mm
    ds.m_ViasMinSize = MM(0.45)
    ds.m_ViasMinAnnularWidth = MM(0.13)
    ds.m_HoleToHoleMin = MM(0.25)
    nc = board.GetAllNetClasses()["Default"]
    nc.SetTrackWidth(MM(0.25))
    nc.SetClearance(MM(0.2))
    nc.SetViaDiameter(MM(0.6))
    nc.SetViaDrill(MM(0.3))

    # ---- footprint placement (global collision check) -----------------------
    boxes = []   # occupied rects in mm

    def free(b):
        return not any(not (b[2] <= o[0] or b[0] >= o[2] or
                            b[3] <= o[1] or b[1] >= o[3]) for o in boxes)

    def find_spot(w, h, rect):
        y = rect[1]
        while y + h <= rect[3]:
            x = rect[0]
            while x + w <= rect[2]:
                cand = (x, y, x + w, y + h)
                if free(cand):
                    return cand
                x += STEP
            y += STEP
        return None

    owner = design.get("owner", {})
    placed, overflow = {}, []
    by_cluster = {}
    for c in design["components"]:
        by_cluster.setdefault(c["cluster"], []).append(c)

    # Pin the fixed parts first so everything else packs around them.
    for c in design["components"]:
        if c["ref"] not in FIXED:
            continue
        fp = load_fp(c["fp"])
        board.Add(fp)
        fp.SetReference(c["ref"]); fp.SetValue(c["value"])
        fp.Value().SetVisible(False)
        x, y, rot = FIXED[c["ref"]]
        fp.SetPosition(V(x, y)); fp.SetOrientationDegrees(rot)
        bb = extent_box(fp)
        boxes.append((bb.GetLeft() / 1e6, bb.GetTop() / 1e6,
                      bb.GetRight() / 1e6, bb.GetBottom() / 1e6))
        placed[c["ref"]] = fp

    def place_at_box(fp, spot, bb):
        cur = fp.GetPosition()
        dx = cur.x - (bb.GetLeft() + bb.GetRight()) // 2
        dy = cur.y - (bb.GetTop() + bb.GetBottom()) // 2
        fp.SetPosition(pcbnew.VECTOR2I(
            MM((spot[0] + spot[2]) / 2.0) + dx,
            MM((spot[1] + spot[3]) / 2.0) + dy))

    def ring_spot(w, h, tx, ty, rect):
        """Nearest free slot to (tx,ty), searched outward in rings."""
        r = 0.0
        while r <= 26.0:
            n = max(1, int(r * 4))
            for k in range(n):
                import math
                ang = 2 * math.pi * k / n
                x = tx + r * math.cos(ang) - w / 2.0
                y = ty + r * math.sin(ang) - h / 2.0
                if rect and not (rect[0] <= x and x + w <= rect[2]
                                 and rect[1] <= y and y + h <= rect[3]):
                    continue
                cand = (x, y, x + w, y + h)
                if free(cand):
                    return cand
            r += 1.0
        return None

    for cluster, items in sorted(by_cluster.items()):
        rect = CLUSTER_RECT[cluster]
        # biggest first packs far more densely than schematic order
        def area(d):
            f = load_fp(d["fp"]); b = extent_box(f)
            return b.GetWidth() * b.GetHeight()
        # ICs first so their bypass caps have something to sit next to
        def order(d):
            return (0 if d["ref"].startswith("U") else 1, -area(d))
        for c in sorted(items, key=order):
            if c["ref"] in FIXED:
                continue
            fp = load_fp(c["fp"])
            board.Add(fp)
            fp.SetReference(c["ref"]); fp.SetValue(c["value"])
            fp.Value().SetVisible(False)

            bb = extent_box(fp)
            w = bb.GetWidth() / 1e6 + PLACE_CLEAR
            h = bb.GetHeight() / 1e6 + PLACE_CLEAR
            if c["ref"].startswith("J"):
                h += CONN_TEXT_PAD
                # long function names can be wider than the connector itself
                w = max(w, len(c["value"]) * SILK_TEXT_MM * 0.72 + PLACE_CLEAR)
            spot = None
            own = owner.get(c["ref"])
            if own and own[0] in placed:
                # A bypass cap is useless far from its pin -- trace inductance
                # swamps it. Put it as close to the owning pad as will fit.
                # Pad nets are not applied until after placement, so the target
                # pad is resolved from the design netlist instead.
                want = set(pn for r, pn in design["nets"].get(own[1], [])
                           if r == own[0])
                opad = None
                for pad in placed[own[0]].Pads():
                    if pad.GetNumber() in want:
                        opad = pad
                        break
                if opad is not None:
                    # search the whole board, not just this cluster's rect
                    board_rect = (BX0 + 1.0, BY0 + 1.0, BX1 - 1.0, BY1 - 1.0)
                    spot = ring_spot(w, h,
                                     opad.GetPosition().x / 1e6,
                                     opad.GetPosition().y / 1e6, board_rect)
            if spot is None:
                spot = find_spot(w, h, rect)
            if spot is None:                       # cluster full -> spill below
                spot = find_spot(w, h, (rect[0], rect[1], rect[2], BY1 + 60))
                overflow.append(c["ref"])
            boxes.append(spot)

            # translate so the footprint's extent lands on the chosen rect
            cur = fp.GetPosition()
            dx = cur.x - (bb.GetLeft() + bb.GetRight()) // 2
            dy = cur.y - (bb.GetTop() + bb.GetBottom()) // 2
            fp.SetPosition(pcbnew.VECTOR2I(
                MM((spot[0] + spot[2]) / 2.0) + dx,
                MM((spot[1] + spot[3]) / 2.0) + dy))
            placed[c["ref"]] = fp

    # The USB-C receptacle's own pads sit 0.15mm apart -- that spacing is fixed
    # by the connector, not something routing can fix -- so it needs a local
    # override rather than loosening clearance for the whole board.
    for ref, fp in placed.items():
        if "USB_C" in fp.GetFPID().GetUniStringLibItemName():
            fp.SetLocalClearance(MM(0.13))

    # Record on each bypass cap which pin it serves. Nothing in the netlist can
    # express this, so without it a layout pass has no way to know that C15
    # belongs beside U4 rather than U7.
    for ref, (ic, rail) in owner.items():
        if ref in placed:
            placed[ref].SetField("Decouples", "%s %s" % (ic, rail))
            # metadata, not artwork: a new footprint field defaults to VISIBLE
            # on F.SilkS, which printed 26 extra texts and took DRC 7 -> 168.
            _f = placed[ref].GetFieldByName("Decouples")
            if _f is not None:
                _f.SetVisible(False)
                _f.SetLayer(pcbnew.F_Fab)

    # ---- silkscreen naming for every connector ------------------------------
    # A bare "J7" tells you nothing while you are holding the board and a
    # fistful of identical 4-pin XH harnesses. Each connector therefore gets
    # its FUNCTION printed on the front silkscreen (Motor_Tilt, AS5600_Pan,
    # Limit_Z_Home ...), reference above the part and function below it.
    # By default pcbnew leaves Value hidden and on F.Fab, which is a
    # fabrication-only layer -- it is not printed on the physical board.
    for ref, fp in sorted(placed.items()):
        if not ref.startswith("J"):
            continue
        bb = extent_box(fp)
        top, bot = bb.GetTop(), bb.GetBottom()
        cx = (bb.GetLeft() + bb.GetRight()) // 2

        rf = fp.Reference()
        rf.SetVisible(True)
        rf.SetLayer(pcbnew.F_SilkS)
        rf.SetTextSize(pcbnew.VECTOR2I(MM(0.9), MM(0.9)))
        rf.SetTextThickness(MM(0.15))
        rf.SetPosition(pcbnew.VECTOR2I(cx, top - MM(0.9)))

        vl = fp.Value()
        vl.SetVisible(True)                 # was hidden -> never printed
        vl.SetLayer(pcbnew.F_SilkS)         # was F.Fab -> not on the board
        vl.SetTextSize(pcbnew.VECTOR2I(MM(0.9), MM(0.9)))
        vl.SetTextThickness(MM(0.15))
        vl.SetPosition(pcbnew.VECTOR2I(cx, bot + MM(0.9)))

    # ---- nets ---------------------------------------------------------------
    netmap = {}
    for name in design["nets"]:
        ni = pcbnew.NETINFO_ITEM(board, name)
        board.Add(ni)
        netmap[name] = ni

    assigned = missing = 0
    for name, pads in design["nets"].items():
        ni = netmap[name]
        for ref, padnum in pads:
            fp = placed.get(ref)
            if fp is None:
                missing += 1
                continue
            # A pad NUMBER can appear many times (the TMC2209 exposed pad and
            # its 17 thermal vias are all pad "29"); net every one of them or
            # the strays read as shorts against the plane.
            hits = [p for p in fp.Pads() if p.GetNumber() == padnum]
            if not hits:
                print("  !! no pad %s on %s" % (padnum, ref)); missing += 1; continue
            for p in hits:
                p.SetNet(ni)
            assigned += len(hits)

    # ---- board outline ------------------------------------------------------
    pts = [(BX0, BY0), (BX1, BY0), (BX1, BY1), (BX0, BY1)]
    for i in range(4):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(V(*pts[i])); seg.SetEnd(V(*pts[(i + 1) % 4]))
        seg.SetLayer(pcbnew.Edge_Cuts); seg.SetWidth(MM(0.1))
        board.Add(seg)

    # ---- copper pours -------------------------------------------------------
    # In1 = solid ground plane directly under the signal layers (return path),
    # In2 = 3V3 plane, plus GND pours on both outer layers.
    def add_zone(layer, netname, inset=0.4):
        z = pcbnew.ZONE(board)
        ls = pcbnew.LSET(); ls.addLayer(layer)
        z.SetLayerSet(ls)
        z.SetNet(netmap[netname])
        z.SetLocalClearance(MM(0.25))
        z.SetMinThickness(MM(0.2))
        o = z.Outline(); o.NewOutline()
        for (x, y) in [(BX0 + inset, BY0 + inset), (BX1 - inset, BY0 + inset),
                       (BX1 - inset, BY1 - inset), (BX0 + inset, BY1 - inset)]:
            o.Append(MM(x), MM(y))
        board.Add(z)

    add_zone(pcbnew.In1_Cu, "GND")
    add_zone(pcbnew.In2_Cu, "+3V3")
    add_zone(pcbnew.F_Cu, "GND")
    add_zone(pcbnew.B_Cu, "GND")
    # NOTE: pcbnew.ZONE_FILLER().Fill() segfaults under the standalone python
    # interpreter (it expects the app's progress reporter). The zone OUTLINES
    # are what persist in the file; KiCad fills them on open, or via 'B'.

    out = os.path.join(HERE, "pantiltslide_integrated.kicad_pcb")
    board.Save(out)
    print("pads assigned: %d, missing: %d" % (assigned, missing))
    if overflow:
        print("OVERFLOWED cluster rect (placed below board):", overflow)
    far = []
    for ref, (ic, rail) in owner.items():
        if ref not in placed or ic not in placed:
            continue
        want = set(pn for r, pn in design["nets"].get(rail, []) if r == ic)
        for pad in placed[ic].Pads():
            if pad.GetNumber() in want:
                d = ((pad.GetPosition().x - placed[ref].GetPosition().x) ** 2 +
                     (pad.GetPosition().y - placed[ref].GetPosition().y) ** 2) ** 0.5 / 1e6
                if d > 10.0:
                    far.append("%s->%s %.0fmm" % (ref, ic, d))
                break
    if far:
        print("BYPASS CAPS STILL FAR FROM OWNER:", ", ".join(sorted(far)))
    print("saved", out)
    print("footprints: %d  nets: %d  layers: %d"
          % (len(placed), len(netmap), board.GetCopperLayerCount()))


if __name__ == "__main__":
    main()
