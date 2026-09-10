"""Minimal s-expression reader for KiCad files.

Deliberately dependency-free: this runs in CI, where the kicad-happy plugin
cache that `pantiltslide/tools/*.py` imports from does not exist. Stdlib only,
Python 3.9+.

A KiCad file parses to nested lists of str/float. Atoms keep their source form
(`"F.Cu"` and `F.Cu` both become the str `F.Cu`) because every consumer here
compares against known keywords rather than round-tripping the file.
"""
from __future__ import annotations

_WS = " \t\r\n"


def loads(text: str) -> list:
    """Parse the first complete s-expression in *text*."""
    pos = 0
    n = len(text)

    def skip():
        nonlocal pos
        while pos < n and text[pos] in _WS:
            pos += 1

    def node():
        nonlocal pos
        skip()
        if pos >= n:
            raise ValueError("unexpected end of input")
        ch = text[pos]
        if ch == "(":
            pos += 1
            out = []
            while True:
                skip()
                if pos >= n:
                    raise ValueError("unterminated list")
                if text[pos] == ")":
                    pos += 1
                    return out
                out.append(node())
        if ch == ")":
            raise ValueError(f"unexpected ')' at {pos}")
        if ch == '"':
            pos += 1
            buf = []
            while pos < n:
                c = text[pos]
                if c == "\\" and pos + 1 < n:
                    nxt = text[pos + 1]
                    buf.append({"n": "\n", "t": "\t", "r": "\r"}.get(nxt, nxt))
                    pos += 2
                    continue
                if c == '"':
                    pos += 1
                    return "".join(buf)
                buf.append(c)
                pos += 1
            raise ValueError("unterminated string")
        start = pos
        while pos < n and text[pos] not in _WS and text[pos] not in "()":
            pos += 1
        return text[start:pos]

    return node()


def load(path: str) -> list:
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return loads(fh.read())


def children(node, key: str):
    """Direct children of *node* whose head atom is *key*."""
    if not isinstance(node, list):
        return
    for item in node:
        if isinstance(item, list) and item and item[0] == key:
            yield item


def first(node, key: str):
    for item in children(node, key):
        return item
    return None


def descend(node, key: str):
    """Every list named *key* anywhere beneath *node*, depth-first."""
    stack = [node]
    while stack:
        cur = stack.pop()
        if not isinstance(cur, list):
            continue
        if cur and cur[0] == key:
            yield cur
        for item in cur:
            if isinstance(item, list):
                stack.append(item)


def num(value, default=0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default
