#!/usr/bin/env python3
"""Print the effective optimization flags per component from a CMake
`compile_commands.json`. Useful to verify that an optimization flag (e.g.
`-ffast-math`) actually reached the vendored OSQP / qdldl / wrapper.

Usage:
    python bench/scan_flags.py <compile_commands.json> [<more.json> ...]
"""
import json
import sys


def scan(path):
    out = {}
    for e in json.load(open(path)):
        f = e["file"]
        if "solver-osqp/src/core" in f:
            tag = "WRAPPER"
        elif "solver-osqp/osqp/" in f:
            tag = "OSQP"
        elif "qdldl/src/qdldl.c" in f:
            tag = "QDLDL"
        else:
            tag = "OTHER"
        toks = e["command"].split()
        opt = [x for x in toks
               if x in ("-O3", "-O2", "-O1", "-Ofast", "-O0")
               or x == "-ffast-math"
               or x.startswith("-march")
               or "flto" in x]
        out.setdefault(tag, set()).update(opt)
    return {k: sorted(v) for k, v in out.items()}


paths = sys.argv[1:] or ["build/compile_commands.json"]
for p in paths:
    try:
        s = scan(p)
    except OSError:
        print(f"!! {p} not found")
        continue
    print(f"--- {p} ---")
    for k in ("OSQP", "QDLDL", "WRAPPER"):
        print(f"  {k:8} {s.get(k, [])}")
