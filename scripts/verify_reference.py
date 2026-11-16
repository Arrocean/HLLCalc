"""Developer-only parity check; the application has no Python dependency.
SPDX-License-Identifier: GPL-3.0-only
Usage: python verify_reference.py path/to/Aimer_HLL_v6.py build/parity.csv
"""
import ast
import csv
import sys
from pathlib import Path

source, output = map(Path, sys.argv[1:])
tree = ast.parse(source.read_text(encoding="utf-8-sig"))
params = None
for node in ast.walk(tree):
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Attribute) and t.attr == "FACTION_DATA" for t in node.targets):
        params = ast.literal_eval(node.value)
        break
if params is None:
    raise RuntimeError("Upstream FACTION_DATA not found")
nations = ["GER", "US", "USSR", "UK"]
count = 0
with output.open(newline="", encoding="utf-8-sig") as file:
    for row in csv.reader(file):
        n, mode, distance, inclination, actual = map(int, row)
        p = params[nations[n]]
        expected = round(p["spg_base"] - p["spg_factor"] * distance - inclination) if mode == 0 else round(p["std_base"] - p["std_factor"] * distance)
        if actual != expected:
            raise AssertionError(f"Mismatch {row}: Python={expected}")
        count += 1
assert count == 4 * 2 * 1700 * 5, count
print(f"PASS: {count:,} calculations exactly match original Python formulas and round().")
