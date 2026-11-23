#!/usr/bin/env python3
"""Reproduce the eight sequential Release runs or reduce their archived frame records."""
import argparse
import csv
import gzip
import math
import shlex
import shutil
import subprocess
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
LAB = ROOT / "build/m73-release/lab/tiny2d_lab"
CONFIGS = {"A": ("off", "off", "off"), "B": ("on", "off", "off"),
           "C": ("on", "on", "off"), "D": ("on", "on", "all")}
SIZES = ("800x450", "1120x630")


def run(size, label):
    lighting, inspector, previews = CONFIGS[label]
    stem = f"{size}-{label}"
    path = HERE / f"{stem}.csv"
    cmd = [str(LAB.relative_to(ROOT)), "--scene", "neon", "--seed", "71",
           "--size", size, "--frames", "1320", "--lighting", lighting, "--bloom", "on",
           "--view", "final", "--inspector", inspector, "--previews", previews,
           "--gpu-timing", "on", "--metrics", str(path.relative_to(ROOT))]
    with (HERE / f"{stem}.log").open("w") as log:
        print(shlex.join(cmd), file=log, flush=True)
        subprocess.run(cmd, cwd=ROOT, check=True, stdout=log, stderr=subprocess.STDOUT)
    with path.open("rb") as source, gzip.GzipFile(str(path) + ".gz", "wb", mtime=0) as archive:
        shutil.copyfileobj(source, archive)
    path.unlink()  # This run's uncompressed working copy.
    print(f"Finished {stem}", flush=True)


def quantiles(rows, key):
    values = sorted(float(row[key]) for row in rows if row[key] != "")
    if not values:
        return "0 | — | — | —"
    q = [values[math.ceil(len(values) * p) - 1] for p in (.50, .95, .99)]
    return f"{len(values)} | " + " | ".join(f"{v:.3f}" for v in q)


def summarize(size, label):
    stem = f"{size}-{label}"
    with gzip.open(HERE / f"{stem}.csv.gz", "rt") as source:
        rows = list(csv.DictReader(source))
    assert all(None not in r and None not in r.values() for r in rows)
    frames = [r for r in rows if r["record"] == "cpu"]
    assert [(int(r["frame"]), int(r["tick"])) for r in frames] == [(i, i) for i in range(1, 1321)]
    sample = [r for r in frames if r["measurement"] == "1"]
    assert len(sample) == 1200 and sample[0]["frame"] == "121"
    info = sample[0]
    width, height = int(info["width"]), int(info["height"])
    lighting = label != "A"
    inspector = label in ("C", "D")
    previews = label == "D"
    hdr = 8 * (width * height * (1 + 3 * lighting) + 3 * ((width+1)//2) * ((height+1)//2))
    for row in sample:
        assert (int(row["width"]), int(row["height"])) == (width, height)
        assert int(row["lighting"]) == lighting and int(row["inspector"]) == inspector
        assert row["view"] == "final" and row["bloom"] == row["hud"] == row["timing"] == "1"
        assert int(row["hdr_bytes"]) == hdr
        assert int(row["preview_mask"]) == (31 if previews else 0)
        assert int(row["preview_draws"]) == (5 if previews else 0) and (int(row["gui_draws"]) > 0) == inspector
        assert int(row["draws"]) == sum(int(v) for k,v in row.items() if k.endswith("_draws"))
        assert int(row["batch_vertex_bytes"]) == sum(int(v) for k,v in row.items() if k.endswith("_batch_bytes"))
        assert int(row["world_draws"]) == int(not lighting) and int(row["light_draws"]) == int(lighting)
        assert (int(row["light_w"]), int(row["light_h"])) == ((width,height) if lighting else (0,0))
        count = int(row["active_lights"])
        assert (5 <= count <= 25 if lighting else count == 0) and row["dropped_lights"] == "0"
        assert int(row["light_uniform_bytes"]) == 32 * count
        row["upload_bytes"] = sum(int(row[k]) for k in ("batch_vertex_bytes","gui_vertex_bytes",
                                    "gui_index_bytes","font_upload_bytes","light_uniform_bytes"))
    retired = {r["frame"]: r for r in rows if r["record"] == "gpu"}
    statuses = Counter(retired.get(r["frame"],r)["query_status"] for r in sample)
    gpu = [r for r in retired.values() if r["measurement"] == "1" and r["query_status"] == "completed"]
    lines = [f"## {width}×{height} · {label}", "",
             f"GPU timestamp bits {info['timestamp_bits']}. Measurement outcomes {dict(statuses)}. "
             f"Pass mask {info['pass_mask']}. LightBuffer {info['light_w']}×{info['light_h']}.", "",
             "| CPU metric | n | p50 | p95 | p99 |", "|---|---:|---:|---:|---:|"]
    keys = [k for k in info if k.endswith(("_us","_draws","_bytes"))]
    keys += ["passes","draws","active_lights","dropped_lights"]
    lines.extend(f"| {k} | {quantiles(sample,k)} |" for k in keys)
    lines.extend(["", f"Valid GPU timestamp samples {len(gpu)}.", ""])
    if gpu:
        lines.extend(["| GPU interval (µs) | n | p50 | p95 | p99 |", "|---|---:|---:|---:|---:|"])
        for key in ["total_us"] + [k[:-6] + "_us" for k in info if k.endswith("_draws")]:
            lines.append(f"| {key} | {quantiles(gpu,key)} |")
        lines.append("")
    return lines


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("run", "summarize"))
    args = parser.parse_args()
    lines = ["# M7.3 Release measurements", "",
             "Nearest-rank quantiles over originating frames 121–1320. Times in µs, payload and storage in bytes.", ""]
    for logical in SIZES:
        for config in CONFIGS:
            if args.operation == "run":
                run(logical, config)
            lines.extend(summarize(logical, config))
    (HERE / "quantiles.md").write_text("\n".join(lines))
