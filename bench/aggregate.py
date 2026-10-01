#!/usr/bin/env python3
"""Aggregate bench/results/*.txt into a comparison table (vs the -O3 baseline)."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "results")


def parse(path):
    d = {}
    for line in open(path):
        if not line.startswith("RESULT"):
            continue
        toks = line.split()
        name = toks[1]
        rec = d.setdefault(name, {})
        for t in toks[2:]:
            if "=" in t:
                k, v = t.split("=", 1)
                rec[k] = v
    return d


order = ["o0", "o1", "o2", "o3", "ofast", "o3_native", "o3_native_lto", "ofast_native_lto"]
base = parse(os.path.join(RES, "o3.txt"))
bs = float(base["small"]["solve_time_avg"])
bl = float(base["large"]["solve_time_avg"])
bu = float(base["large"]["setup_time"])

print(f"{'variant':18} {'small solve/s':>13} {'vs -O3':>7} {'large solve/s':>13} {'vs -O3':>7} {'large setup/s':>13} {'vs -O3':>7}")
for v in order:
    d = parse(os.path.join(RES, v + ".txt"))
    so = float(d["small"]["solve_time_avg"])
    ls = float(d["large"]["solve_time_avg"])
    lu = float(d["large"]["setup_time"])
    print(f"{v:18} {so:13.6f} {so/bs:6.3f}x {ls:13.6f} {ls/bl:6.3f}x {lu:13.6f} {lu/bu:6.3f}x")
