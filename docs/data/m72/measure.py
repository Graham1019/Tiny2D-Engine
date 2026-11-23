#!/usr/bin/env python3
"""Run eight sequential finite Lab captures and verify/reduce their original frame records."""
import argparse
import csv
import math
import shlex
import subprocess
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
CONFIGS = [("off", "off", "off"), ("on", "off", "off"),
           ("on", "all", "off"), ("on", "all", "on")]
SIZES = ["800x450", "1120x630"]


def run():
    for size in SIZES:
        for index, (inspector, previews, timer) in enumerate(CONFIGS, 1):
            stem = f"{size}-{index}"
            path = HERE / f"{stem}.csv"
            cmd = ["./build/m72-release/lab/tiny2d_lab", "--scene", "neon", "--seed", "71",
                   "--size", size, "--frames", "1320", "--view", "final", "--bloom", "on",
                   "--inspector", inspector, "--previews", previews, "--gpu-timing", timer,
                   "--metrics", str(path.relative_to(ROOT))]
            with (HERE / f"{stem}.log").open("w") as log:
                print(shlex.join(cmd), file=log, flush=True)
                subprocess.run(cmd, cwd=ROOT, check=True, stdout=log, stderr=subprocess.STDOUT)
            print(f"Finished {stem}", flush=True)


def quantiles(rows, key):
    values = sorted(float(row[key]) for row in rows if row[key])
    if not values:
        return "0 | — | — | —"
    return " | ".join([str(len(values))] +
                      [f"{values[math.ceil(len(values)*p)-1]:.3f}" for p in (.50, .95, .99)])


def summarize():
    lines = ["# M7.2 Release measurements", "",
             "Nearest-rank quantiles by originating frame. CPU and GPU times use microseconds.", ""]
    for size in SIZES:
        for index, (inspector, previews, timing) in enumerate(CONFIGS, 1):
            stem = f"{size}-{index}"
            with (HERE / f"{stem}.csv").open() as source:
                rows = list(csv.DictReader(source))
            assert all(None not in r and None not in r.values() for r in rows), "ragged CSV"
            cpu = [r for r in rows if r["record"] == "cpu"]
            assert [(int(r["frame"]), int(r["tick"])) for r in cpu] == [(i, i) for i in range(1, 1321)]
            sample = [r for r in cpu if r["measurement"] == "1"]
            assert len(sample) == 1200 and sample[0]["frame"] == "121"
            expected = 4 if previews == "all" else 0
            mask = 63 | (256 if inspector == "on" else 0) | (128 if expected else 0)
            for row in sample:
                assert row["preview_mask"] == ("15" if expected else "0") and int(row["pass_mask"]) == mask
                assert int(row["preview_draws"]) == expected and (int(row["gui_draws"]) > 0) == (inspector == "on")
                assert int(row["draws"]) == sum(int(v) for k, v in row.items() if k.endswith("_draws"))
                assert int(row["batch_vertex_bytes"]) == sum(int(v) for k, v in row.items() if k.endswith("_batch_bytes"))
                assert row["view"] == "final" and row["bloom"] == "1" and row["hud"] == "1" and row["debug_draws"] == "0"
            gpu = {r["frame"]: r for r in rows if r["record"] == "gpu"}
            states = Counter(gpu.get(r["frame"], r)["query_status"] for r in sample)
            valid = [r for r in gpu.values() if r["measurement"] == "1" and r["query_status"] == "completed"]
            first = sample[0]
            lines.extend([f"## {first['width']}×{first['height']} · configuration {index}", "",
                          f"Inspector {inspector}, previews {previews}, GPU timing {timing}. "
                          f"Measurement GPU states {dict(states)}. Pass mask {mask}.", "",
                          "| CPU metric | n | p50 | p95 | p99 |", "|---|---:|---:|---:|---:|"])
            keys = [k for k in first if k.endswith("_us")] + ["passes", "draws", "world_sprites",
                    "world_draws", "debug_draws", "preview_draws", "gui_draws", "batch_vertex_bytes",
                    "gui_vertices", "gui_indices", "gui_vertex_bytes", "gui_index_bytes", "font_upload_bytes",
                    "hdr_bytes", "preview_bytes", "font_bytes"]
            lines.extend(f"| {k} | {quantiles(sample, k)} |" for k in keys)
            lines.extend(["", "| GPU interval | n | p50 | p95 | p99 |", "|---|---:|---:|---:|---:|"])
            for key in ["total_us", "world_us", "bright_us", "horizontal_us", "vertical_us",
                        "output_us", "hud_us", "debug_us", "preview_us", "gui_us"]:
                lines.append(f"| {key} | {quantiles(valid, key)} |")
            lines.append("")
    (HERE / "quantiles.md").write_text("\n".join(lines))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=["run", "summarize"])
    if parser.parse_args().operation == "run":
        run()
    summarize()
