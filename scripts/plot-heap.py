#!/usr/bin/env python3
# /// script
# requires-python = ">=3.9"
# dependencies = ["matplotlib"]
# ///
"""Plot the heap samples that `wokwi-cli --heap-csv` writes.

Usage: scripts/plot-heap.py heap.csv [--limit BYTES] [--out heap.png]
"""

import argparse
import csv
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument("csv", help="CSV file from wokwi-cli --heap-csv")
parser.add_argument("--limit", type=int, help="draw the --heap-min-free limit")
parser.add_argument(
    "--out", default="heap.png", help="output image (default: heap.png)"
)
args = parser.parse_args()

boards = defaultdict(lambda: ([], [], []))
with open(args.csv) as f:
    for row in csv.DictReader(f):
        t, free, min_free = boards[row["part"]]
        t.append(float(row["time_s"]))
        free.append(int(row["free"]))
        min_free.append(int(row["min_free"]))

fig, ax = plt.subplots(figsize=(10, 5))
for part, (t, free, min_free) in boards.items():
    prefix = f"{part}: " if len(boards) > 1 else ""
    ax.plot(t, free, marker="o", markersize=3, label=f"{prefix}free")
    ax.plot(t, min_free, linestyle=":", label=f"{prefix}lowest since boot")
if args.limit:
    ax.axhline(
        args.limit,
        color="tab:red",
        linestyle="--",
        label=f"limit ({args.limit:,} bytes)",
    )

ax.set_title("Free heap")
ax.set_xlabel("simulated time (s)")
ax.set_ylabel("bytes")
ax.yaxis.set_major_formatter(lambda v, _: f"{v:,.0f}")
ax.grid(alpha=0.3)
ax.legend()
fig.tight_layout()
fig.savefig(args.out, dpi=120)
print(f"wrote {args.out}")
