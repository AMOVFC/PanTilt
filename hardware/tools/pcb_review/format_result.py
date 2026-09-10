"""Split `claude -p --output-format json` into the review text and a cost line.

Exists so the workflow does not have to trust a jq filter against a payload
shape nobody verified. Every field is read defensively: an unexpected schema
costs the cost footer, never the review itself.

The cost line matters more than it looks. "How much does this cost per review"
is otherwise a guess, and a guess is what stops people turning the bot on.
Reporting real spend on every run replaces the guess with a number.

Usage:
    python format_result.py result.json --review review.md --cost cost.txt
"""
from __future__ import annotations

import argparse
import json
import sys


def read(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            return json.load(fh)
    except (OSError, json.JSONDecodeError):
        return None


def review_text(payload):
    """The review body. `result` is the documented field; fall back to a plain
    string payload, or to a `content` list, before giving up."""
    if payload is None:
        return ""
    if isinstance(payload, str):
        return payload
    if not isinstance(payload, dict):
        return ""
    if payload.get("is_error"):
        return ""
    for key in ("result", "text", "output"):
        val = payload.get(key)
        if isinstance(val, str) and val.strip():
            return val
    content = payload.get("content")
    if isinstance(content, list):
        parts = [b.get("text", "") for b in content
                 if isinstance(b, dict) and b.get("type") == "text"]
        return "\n".join(p for p in parts if p)
    return ""


def _int(value):
    return value if isinstance(value, int) else 0


def cost_line(payload, model_hint=""):
    if not isinstance(payload, dict):
        return ""
    bits = []
    if model_hint:
        bits.append(f"`{model_hint}`")

    turns = payload.get("num_turns")
    if isinstance(turns, int):
        bits.append(f"{turns} turn{'s' if turns != 1 else ''}")

    usage = payload.get("usage")
    if isinstance(usage, dict):
        billed = (_int(usage.get("input_tokens"))
                  + _int(usage.get("cache_creation_input_tokens"))
                  + _int(usage.get("cache_read_input_tokens")))
        out = _int(usage.get("output_tokens"))
        if billed or out:
            bits.append(f"{billed:,} in / {out:,} out")

    ms = payload.get("duration_ms")
    if isinstance(ms, (int, float)) and ms > 0:
        bits.append(f"{round(ms / 1000)}s")

    cost = payload.get("total_cost_usd")
    if isinstance(cost, (int, float)):
        bits.append(f"**${cost:.4f}**")

    return " · ".join(bits)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("result_json")
    ap.add_argument("--review", required=True)
    ap.add_argument("--cost", required=True)
    ap.add_argument("--model", default="")
    args = ap.parse_args()

    payload = read(args.result_json)
    text = review_text(payload)
    with open(args.review, "w", encoding="utf-8") as fh:
        fh.write(text)
    with open(args.cost, "w", encoding="utf-8") as fh:
        line = cost_line(payload, args.model)
        fh.write(line + "\n" if line else "")

    # Non-zero only when there is no review at all, so the workflow can fall
    # back to the deterministic report.
    return 0 if text.strip() else 1


if __name__ == "__main__":
    sys.exit(main())
