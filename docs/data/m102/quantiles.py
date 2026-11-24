"""Compute nearest-rank percentiles grouping the measured seeks by map and interval."""
import collections
import csv
import math
import sys
groups=collections.defaultdict(list)
with open(sys.argv[1],newline="") as f:
    for r in csv.DictReader(f):
        groups[tuple(r[k] for k in ("width","height","enemies","interval"))].append(r)
w=csv.writer(sys.stdout,lineterminator="\n")
w.writerow(["width","height","enemies","interval","samples","metric","p50","p95","p99","max"])
for key,rows in groups.items():
    for col in ("replayed","checkpoint_count","checkpoint_bytes","max_gap","worst_replay","build_us","restore_us","replay_us","seek_us"):
        values=sorted(float(r[col]) for r in rows)
        w.writerow([*key,len(rows),col,*(values[math.ceil(len(rows)*q)-1] for q in (.5,.95,.99,1))])
