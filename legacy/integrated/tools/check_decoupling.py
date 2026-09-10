# -*- coding: utf-8 -*-
"""Decoupling quality: distance from each bypass cap to the pin it OWNS."""
import pcbnew, json, collections

b = pcbnew.LoadBoard('pantiltslide_integrated.kicad_pcb')
design = json.load(open('design.json', encoding='utf-8'))
owner = design.get('owner', {})
nets = design['nets']

fps = {f.GetReference(): f for f in b.GetFootprints()}
rows = []
for ref, (ic, rail) in sorted(owner.items()):
    if ref not in fps or ic not in fps:
        continue
    want = set(pn for r, pn in nets.get(rail, []) if r == ic)
    tgt = None
    for pad in fps[ic].Pads():
        if pad.GetNumber() in want:
            tgt = pad.GetPosition()
            break
    if tgt is None:
        continue
    cp = fps[ref].GetPosition()
    d = ((tgt.x - cp.x) ** 2 + (tgt.y - cp.y) ** 2) ** 0.5 / 1e6
    rows.append((d, ref, fps[ref].GetValue(), ic, rail))

rows.sort(reverse=True)
print('%-6s %-11s %-5s %-12s %s' % ('CAP', 'VALUE', 'IC', 'RAIL', 'DIST mm'))
for d, ref, val, ic, rail in rows:
    flag = '   <-- too far' if d > 10 else ''
    print('%-6s %-11s %-5s %-12s %6.1f%s' % (ref, val, ic, rail, d, flag))
ds = [r[0] for r in rows]
print()
print('caps: %d   worst: %.1f mm   median: %.1f mm   over 10mm: %d'
      % (len(ds), max(ds), sorted(ds)[len(ds) // 2], sum(1 for x in ds if x > 10)))
