"""Nearest-rank p50/p95/p99 from benchmark_m101's raw CSV. Print a tidy CSV."""
import csv
import itertools
import math
import sys

with open(sys.argv[1], newline="") as source:
    reader = csv.DictReader(source)
    output = csv.writer(sys.stdout, lineterminator="\n")
    output.writerow(["width", "height", "enemies", "boundary", "samples", "metric", "p50", "p95", "p99"])
    for group, rows in itertools.groupby(reader, lambda r: tuple(r[k] for k in ("width", "height", "enemies", "boundary"))):
        rows = list(rows)
        for key in reader.fieldnames[6:]:
            values = sorted(float(r[key]) for r in rows)
            output.writerow([*group, len(rows), key, *(f"{values[math.ceil(len(rows)*p)-1]:.4f}" for p in (.50,.95,.99))])
