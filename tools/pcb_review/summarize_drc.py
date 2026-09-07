"""Fold kicad-cli DRC/ERC JSON down to something a reviewer can read.

`kicad-cli pcb drc --format json` emits every violation with full geometry. On a
board with an unrouted plane that is thousands of entries and tens of thousands
of tokens, most of it the same rule repeated. This groups by rule, keeps a few
concrete examples of each, and reports the totals.

Missing or unparseable input is not an error -- DRC is best-effort in CI, and a
board that could not be opened should still get the rest of its review.

Usage:
    python summarize_drc.py BOARD.kicad_pcb [drc.json] [erc.json]
"""
from __future__ import annotations

import json
import os
import sys
from collections import defaultdict

MAX_EXAMPLES = 4
MAX_RULES = 25


def load(path):
    if not path or not os.path.exists(path):
        return None
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            return json.load(fh)
    except (json.JSONDecodeError, OSError):
        return None


def _where(item):
    """A human-readable location for one violation."""
    bits = []
    for item_ref in item.get("items", []) or []:
        desc = item_ref.get("description") or item_ref.get("uuid") or ""
        if desc:
            bits.append(str(desc)[:90])
    if bits:
        return " / ".join(bits[:2])
    pos = item.get("pos") or {}
    if "x" in pos and "y" in pos:
        return f"at ({pos['x']}, {pos['y']})"
    return ""


def summarize(payload, heading, out):
    if payload is None:
        out.append(f"**{heading}:** not produced (board or schematic did not open).")
        out.append("")
        return

    buckets = defaultdict(list)
    severities = {}
    for group in ("violations", "unconnected_items", "schematic_parity"):
        for item in payload.get(group, []) or []:
            rule = item.get("type") or item.get("rule") or "unknown"
            key = (group, rule)
            severities[key] = item.get("severity", "?")
            buckets[key].append(item)

    total = sum(len(v) for v in buckets.values())
    if total == 0:
        out.append(f"**{heading}:** clean — no errors or warnings reported.")
        out.append("")
        return

    errors = sum(len(v) for k, v in buckets.items() if severities.get(k) == "error")
    out.append(f"**{heading}:** {total} item(s), {errors} at error severity.")
    out.append("")
    ranked = sorted(buckets.items(), key=lambda kv: -len(kv[1]))
    for (group, rule), items in ranked[:MAX_RULES]:
        sev = severities.get((group, rule), "?")
        label = group.replace("_", " ")
        out.append(f"- `{rule}` ({sev}, {label}) x{len(items)}")
        for item in items[:MAX_EXAMPLES]:
            desc = (item.get("description") or "").strip()
            where = _where(item)
            line = " — ".join(x for x in (desc[:140], where) if x)
            if line:
                out.append(f"  - {line}")
        if len(items) > MAX_EXAMPLES:
            out.append(f"  - ...and {len(items) - MAX_EXAMPLES} more of this rule")
    if len(ranked) > MAX_RULES:
        out.append(f"- ...and {len(ranked) - MAX_RULES} further rule(s)")
    out.append("")


def main():
    if len(sys.argv) < 2:
        print("usage: summarize_drc.py BOARD.kicad_pcb [drc.json] [erc.json]", file=sys.stderr)
        return 2
    board = sys.argv[1]
    drc_path = sys.argv[2] if len(sys.argv) > 2 else None
    erc_path = sys.argv[3] if len(sys.argv) > 3 else None

    out = [f"### DRC / ERC — `{os.path.basename(board)}`", ""]
    summarize(load(drc_path), "DRC", out)
    summarize(load(erc_path), "ERC", out)
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
