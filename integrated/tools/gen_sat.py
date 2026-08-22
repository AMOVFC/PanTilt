# -*- coding: utf-8 -*-
"""AS5600 satellite sensor board -- schematic + design.json.

One of these mounts at each of the pan and tilt joints, with the AS5600
centred on the rotation axis under a diametrically-magnetised magnet, and a
4-wire cable back to the main board's AS5600_Pan / AS5600_Tilt header.
"""
import json, os, subprocess, tempfile, re
import gen_sch as g

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = "as5600_sensor"


def build():
    B = g.B
    B.components = []
    B.nets = {}
    B.no_connects = []
    B.nc_pins = set()
    B.lib_needed = set()
    B.custom_syms = {}
    B.counters = {}

    U1 = B.place("U", "New_Library:AS5600", "AS5600", "AS5600-ASOT",
                 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", 100, 80,
                 datasheet="https://ams-osram.com/products/sensor-ics/position-sensors/ams-as5600-position-sensor",
                 description="12-bit magnetic angle sensor, on-axis over shaft magnet")
    B.custom_syms["New_Library:AS5600"] = g.custom_AS5600()

    C1 = g.C("100nF", 120, 80)
    J1 = g.PINHDR4("To_Main_Board", 70, 80)

    # 3.3V mode: VDD3V3 is the supply, VDD5V unused. DIR->GND selects
    # clockwise-increasing counts. PGO and the analog OUT stay unused --
    # the main board reads angle over I2C.
    B.net("+3V3", (U1, "2"), (C1, 1), (J1, 1))
    B.net("GND", (U1, "4"), (U1, "8"), (C1, 2), (J1, 2))
    B.net("SDA", (U1, "6"), (J1, 3))
    B.net("SCL", (U1, "7"), (J1, 4))
    B.nc(U1, "1"); B.nc(U1, "3"); B.nc(U1, "5")

    for rail, x in (("+3V3", 60), ("GND", 75)):
        f = B.place("#FLG", B.use_lib("power.kicad_sym", "PWR_FLAG"),
                    "PWR_FLAG", "PWR_FLAG", "", x, 60)
        B.net(rail, (f, 1))
    p3 = g.PWR("+3V3", 90, 60); B.net("+3V3", (p3, 1))
    gg = g.PWR("GND", 90, 100); B.net("GND", (gg, 1))

    g.main(project_name=PROJ,
           title="AS5600 Satellite Sensor Board (pan / tilt joint)",
           out_dir=HERE)

    # design.json for the PCB builder -- nets from KiCad's netlist, as with
    # the main board, so the PCB can never disagree with the schematic.
    netfile = os.path.join(tempfile.gettempdir(), "_sat.net")
    subprocess.run([r"C:\Program Files\KiCad\9.0\bin\kicad-cli.exe", "sch",
                    "export", "netlist", os.path.join(HERE, PROJ + ".kicad_sch"),
                    "-o", netfile], check=True, capture_output=True)
    ntxt = open(netfile, encoding="utf-8").read()
    nets = {}
    for m in re.finditer(r'\(net \(code "\d+"\) \(name "([^"]+)"\)(.*?)(?=\n    \(net \(code|\Z)',
                         ntxt, re.S):
        pads = [[r, p] for r, p in
                re.findall(r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)', m.group(2))]
        if pads:
            nets[m.group(1)] = pads
    comps = [{"ref": c.ref, "value": c.value, "fp": c.footprint}
             for c in B.components if not c.ref.startswith("#")]
    json.dump({"components": comps, "nets": nets},
              open(os.path.join(HERE, "design_sat.json"), "w", encoding="utf-8"), indent=1)
    print("satellite:", len(comps), "components,", len(nets), "nets")


if __name__ == "__main__":
    build()
