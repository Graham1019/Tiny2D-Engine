"""Run or summarize the eight serial M7.4a Release fixtures from any working directory."""
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
LAB = Path("build/m74a-release/lab/tiny2d_lab")
CONFIGS = {"A": ("off", "tile"), "B": ("on", "flat"), "C": ("on", "tile"), "D": ("on", "tile")}


def run(size, config):
    normal, material = CONFIGS[config]
    base = HERE / f"{size}-{config}"
    raw = base.with_suffix(".csv")
    cmd = [str(LAB), "--seed", "71", "--size", size, "--frames", "1320", "--view", "final",
           "--lighting", "on", "--normal-mapping", normal, "--normal-material", material,
           "--bloom", "on", "--inspector", "on" if config == "D" else "off",
           "--previews", "all" if config == "D" else "off", "--gpu-timing", "on",
           "--metrics", str(raw.relative_to(ROOT))]
    with base.with_suffix(".log").open("w") as out:
        print(shlex.join(cmd), file=out, flush=True)
        subprocess.run(cmd, cwd=ROOT, check=True, stdout=out, stderr=subprocess.STDOUT)
    with raw.open("rb") as source, gzip.GzipFile(str(raw) + ".gz", "wb", mtime=0) as archive:
        shutil.copyfileobj(source, archive)
    raw.unlink()


def quantile(values, q):
    return sorted(values)[math.ceil(len(values) * q) - 1]


def summarize(size, config):
    with gzip.open(HERE / f"{size}-{config}.csv.gz", "rt") as source:
        rows = list(csv.DictReader(source))
    assert all(None not in row and None not in row.values() for row in rows)
    frames = [r for r in rows if r["record"] == "cpu"]
    assert [(int(r["frame"]), int(r["tick"])) for r in frames] == [(i, i) for i in range(1, 1321)]
    sample = [r for r in frames if r["measurement"] == "1"]
    assert len(sample) == 1200 and sample[0]["frame"] == "121"
    first = sample[0]
    w, h = int(first["width"]), int(first["height"])
    normal = config != "A"
    tools = config == "D"
    retired = {r["frame"]: r for r in rows if r["record"] == "gpu" and r["measurement"] == "1"}
    status = Counter(retired.get(r["frame"], r)["query_status"] for r in sample)
    gpu = [r for r in retired.values() if r["query_status"] == "completed"]
    for r in sample:
        assert r["view"] == "final" and r["lighting"] == r["bloom"] == r["hud"] == r["timing"] == "1"
        assert int(r["normal_mapping"]) == normal and r["normal_material"] == CONFIGS[config][1]
        assert int(r["inspector"]) == tools and int(r["preview_mask"]) == (63 if tools else 0)
        assert int(r["width"]) == w and int(r["height"]) == h
        assert int(r["normal_w"]) == w * normal and int(r["normal_h"]) == h * normal
        assert int(r["hdr_bytes"]) == 8 * ((4 + normal) * w * h + 3 * ((w+1)//2) * ((h+1)//2))
        assert int(r["material_bytes"]) == (64*64*4 if normal else 0)
        assert r["material_upload_bytes"] == "0" and float(r["normal_prepare_us"]) == 0
        assert int(r["normal_draws"]) == (config in ("C", "D")) and (bool(r["normal_us"]) == normal)
        assert r["light_draws"] == "1" and int(r["light_uniform_bytes"]) == 32 * int(r["active_lights"])
        assert int(r["preview_draws"]) == (6 if tools else 0) and r["dropped_lights"] == "0"
        assert sum(int(v) for k,v in r.items() if k.endswith("_draws")) == int(r["draws"])
        r["bloom_cpu_us"] = sum(float(r[k]) for k in ("bright_us","horizontal_us","vertical_us"))
        r["tools_cpu_us"] = sum(float(r[k] or 0) for k in ("gui_us","preview_us","gui_build_us"))
        r["upload_bytes"] = sum(int(r[k]) for k in ("batch_vertex_bytes","gui_vertex_bytes","gui_index_bytes",
                                "font_upload_bytes","light_uniform_bytes","material_upload_bytes"))
    fields = ["total_us","normal_prepare_us","normal_us","light_us","scene_compose_us","bloom_cpu_us",
              "tools_cpu_us","draws","upload_bytes","hdr_bytes","preview_bytes","material_bytes"]
    fields += [k for k in first if k not in fields and k.endswith(("_us","_draws","_bytes"))]
    fields += ["passes","active_lights","dropped_lights"]
    startup = frames[0]
    lines = [f"## {w}×{h} · {config}", "",
             f"Frame 1 material preparation {startup['normal_prepare_us']} µs, upload {startup['material_upload_bytes']} B. "
             f"Timestamp bits {first['timestamp_bits']}, valid GPU samples {len(gpu)}. Outcomes {dict(status)}.", "",
             "| CPU metric | n | p50 | p95 | p99 |", "|---|---:|---:|---:|---:|"]
    def table(records, keys):
        for k in keys:
            data = [float(r[k]) for r in records if r[k] != ""]
            values = " | ".join(f"{quantile(data, q):.3f}" for q in (.5,.95,.99)) if data else "— | — | —"
            lines.append(f"| {k} | {len(data)} | {values} |")
    table(sample, fields)
    if gpu:
        lines.extend(["", "| GPU interval (µs) | n | p50 | p95 | p99 |", "|---|---:|---:|---:|---:|"])
        table(gpu, ["total_us"] + [k[:-6] + "_us" for k in first if k.endswith("_draws")])
    return lines + [""]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("run", "summarize"))
    args = parser.parse_args()
    report = ["# M7.4a Release measurements", "",
              "Nearest-rank quantiles for frames 121–1320. Durations in microseconds, storage and payloads in bytes.", ""]
    for logical in ("800x450", "1120x630"):
        for label in CONFIGS:
            if args.action == "run":
                run(logical, label)
            report += summarize(logical, label)
            print(f"Verified {logical} {label}", flush=True)
    (HERE / "quantiles.md").write_text("\n".join(report))
