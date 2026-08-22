# -*- coding: utf-8 -*-
"""Build the AS5600 satellite sensor PCB (2-layer, 22 x 22 mm).

Run under KiCad's python:
  "C:/Program Files/KiCad/9.0/bin/python.exe" build_pcb_sat.py
"""
import json, os
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
FPDIR = r"C:\Program Files\KiCad\9.0\share\kicad\footprints"
MM = pcbnew.FromMM

CX, CY = 110.0, 110.0     # rotation axis == board centre
HALF = 11.0               # 22 x 22 mm board
HOLE_OFF = 8.0            # M2 holes on the diagonal, clear of the sensor


def V(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))


def load_fp(fpid):
    lib, name = fpid.split(":", 1)
    fp = pcbnew.FootprintLoad(os.path.join(FPDIR, lib + ".pretty"), name)
    if fp is None:
        raise RuntimeError("footprint not found: " + fpid)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


# ref -> (x, y, rotation). The AS5600 sits exactly on the axis; everything
# else is pushed to the edges so nothing fouls the magnet clearance.
PLACE = {
    "U1": (CX, CY, 0.0),
    "C1": (CX + 5.5, CY, 90.0),
    # PinHeader's origin sits on PAD 1, not the footprint centre, and at
    # rot 90 the pads run +X from there -- so shift left by half the 7.62mm
    # pad span to actually centre the connector on the board.
    "J1": (CX - 3.81, CY + 7.5, 90.0),
}


def main():
    design = json.load(open(os.path.join(HERE, "design_sat.json"), encoding="utf-8"))
    board = pcbnew.CreateEmptyBoard()
    board.SetCopperLayerCount(2)

    ds = board.GetDesignSettings()
    ds.m_TrackMinWidth = MM(0.15)
    ds.m_MinClearance = MM(0.15)
    ds.m_MinThroughDrill = MM(0.2)
    ds.m_ViasMinSize = MM(0.45)
    ds.m_HoleToHoleMin = MM(0.25)
    nc = board.GetAllNetClasses()["Default"]
    nc.SetTrackWidth(MM(0.25)); nc.SetClearance(MM(0.2))
    nc.SetViaDiameter(MM(0.6)); nc.SetViaDrill(MM(0.3))

    placed = {}
    for c in design["components"]:
        fp = load_fp(c["fp"])
        board.Add(fp)
        fp.SetReference(c["ref"]); fp.SetValue(c["value"])
        fp.Value().SetVisible(False)
        x, y, rot = PLACE[c["ref"]]
        fp.SetPosition(V(x, y)); fp.SetOrientationDegrees(rot)
        placed[c["ref"]] = fp

    # two M2 mounting holes on the diagonal
    # both holes along the TOP edge: the bottom half belongs to the connector
    for i, (dx, dy) in enumerate([(-HOLE_OFF, -HOLE_OFF), (HOLE_OFF, -HOLE_OFF)]):
        h = load_fp("MountingHole:MountingHole_2.2mm_M2")
        board.Add(h)
        h.SetReference("H%d" % (i + 1))
        h.SetPosition(V(CX + dx, CY + dy))
        # mechanical-only: keeps schematic parity from reporting it as extra
        h.SetAttributes(h.GetAttributes() | pcbnew.FP_BOARD_ONLY
                        | pcbnew.FP_EXCLUDE_FROM_POS_FILES
                        | pcbnew.FP_EXCLUDE_FROM_BOM)
        placed["H%d" % (i + 1)] = h

    netmap = {}
    for name in design["nets"]:
        ni = pcbnew.NETINFO_ITEM(board, name); board.Add(ni); netmap[name] = ni
    assigned = 0
    for name, pads in design["nets"].items():
        for ref, padnum in pads:
            fp = placed.get(ref)
            if fp is None:
                continue
            for p in fp.Pads():
                if p.GetNumber() == padnum:
                    p.SetNet(netmap[name]); assigned += 1

    # outline
    pts = [(CX - HALF, CY - HALF), (CX + HALF, CY - HALF),
           (CX + HALF, CY + HALF), (CX - HALF, CY + HALF)]
    for i in range(4):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(*pts[i])); s.SetEnd(V(*pts[(i + 1) % 4]))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1))
        board.Add(s)

    # Silkscreen crosshair + circle marking the rotation axis, so the board can
    # actually be aligned to the shaft during assembly.
    # markers kept outside the SOIC-8 pad field so they do not sit on copper
    for (x1, y1, x2, y2) in [(CX - 5.6, CY, CX - 4.4, CY), (CX + 4.4, CY, CX + 5.6, CY),
                             (CX, CY - 5.6, CX, CY - 4.4), (CX, CY + 4.4, CX, CY + 5.6)]:
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(x1, y1)); s.SetEnd(V(x2, y2))
        s.SetLayer(pcbnew.B_SilkS); s.SetWidth(MM(0.15))
        board.Add(s)
    c = pcbnew.PCB_SHAPE(board)
    c.SetShape(pcbnew.SHAPE_T_CIRCLE)
    c.SetCenter(V(CX, CY)); c.SetEnd(V(CX + 6.5, CY))
    c.SetLayer(pcbnew.B_SilkS); c.SetWidth(MM(0.15)); c.SetFilled(False)
    board.Add(c)

    # GND pours both sides
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        ls = pcbnew.LSET(); ls.addLayer(layer)
        z.SetLayerSet(ls); z.SetNet(netmap["GND"])
        z.SetLocalClearance(MM(0.25)); z.SetMinThickness(MM(0.2))
        o = z.Outline(); o.NewOutline()
        for (x, y) in [(CX - HALF + .7, CY - HALF + .7), (CX + HALF - .7, CY - HALF + .7),
                       (CX + HALF - .7, CY + HALF - .7), (CX - HALF + .7, CY + HALF - .7)]:
            o.Append(MM(x), MM(y))
        board.Add(z)

    out = os.path.join(HERE, "as5600_sensor.kicad_pcb")
    board.Save(out)
    print("pads assigned:", assigned, " footprints:", len(placed))
    print("saved", out)


if __name__ == "__main__":
    main()
