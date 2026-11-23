"""Run the M7.4b Release matrix, or verify and summarize its archived CSV files."""
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
LAB = "build/m74b-release/lab/tiny2d_lab"
CONFIGS = {"A": ("off", "none"), "B": ("on", "none"), "C": ("on", "demo"), "D": ("on", "demo")}


def run(size, key):
    shadows, walls = CONFIGS[key]
    raw = HERE / f"{size}-{key}.csv"
    tools = key == "D"
    cmd = [LAB, "--seed", "71", "--size", size, "--frames", "1320", "--view", "final",
           "--lighting", "on", "--normal-mapping", "on", "--normal-material", "tile",
           "--shadows", shadows, "--occluders", walls, "--shadow-lights", "4", "--bloom", "on",
           "--inspector", "on" if tools else "off", "--previews", "all" if tools else "off",
           "--gpu-timing", "on", "--metrics", str(raw.relative_to(ROOT))]
    with raw.with_suffix(".log").open("w") as output:
        print(shlex.join(cmd), file=output, flush=True)
        subprocess.run(cmd, cwd=ROOT, check=True, stdout=output, stderr=subprocess.STDOUT)
    with raw.open("rb") as source, gzip.GzipFile(str(raw) + ".gz", "wb", mtime=0) as archive:
        shutil.copyfileobj(source, archive)
    raw.unlink()


def summarize(size, key):
    with gzip.open(HERE / f"{size}-{key}.csv.gz", "rt") as source:
        rows = list(csv.DictReader(source))
    assert all(None not in row and None not in row.values() for row in rows)
    cpu = [r for r in rows if r["record"] == "cpu"]
    assert [(int(r["frame"]), int(r["tick"])) for r in cpu] == [(i,i) for i in range(1,1321)]
    sample = [r for r in cpu if r["measurement"] == "1"]
    assert len(sample) == 1200 and sample[0]["frame"] == "121"
    retired = {r["frame"]: r for r in rows if r["record"] == "gpu" and r["measurement"] == "1"}
    statuses = Counter(retired.get(r["frame"], r)["query_status"] for r in sample)
    valid = [r for r in retired.values() if r["query_status"] == "completed"]
    first = sample[0]
    w, h = int(first["width"]), int(first["height"])
    tools, enabled = key == "D", key != "A"
    for r in sample:
        assert r["view"] == "final" and r["lighting"] == r["normal_mapping"] == r["bloom"] == r["hud"] == "1"
        assert r["normal_material"] == "tile" and r["shadow_light_budget"] == "4" and r["timing"] == "1"
        assert int(r["shadows"]) == enabled and r["occluders"] == CONFIGS[key][1]
        assert int(r["inspector"]) == tools and int(r["preview_mask"]) == (127 if tools else 0)
        assert int(r["width"]) == w and int(r["height"]) == h
        assert int(r["hdr_bytes"]) == 8*(5*w*h + 3*((w+1)//2)*((h+1)//2))
        assert int(r["shadow_bytes"]) == 4*w*h*enabled
        assert int(r["material_bytes"]) == 4*64*64 and r["material_upload_bytes"] == "0"
        assert int(r["active_casters"]) == 4*enabled and int(r["mask_rebuilds"]) == 4*enabled
        assert int(r["inspect_rebuilds"]) == tools and int(r["preview_draws"]) == 7*tools
        assert int(r["occluder_count"]) == 2*(key in ("C","D"))
        assert int(r["mask_draw_calls"])+int(r["masked_light_draw_calls"]) == int(r["shadow_batch_draws"])
        assert sum(int(v) for k,v in r.items() if k.endswith("_draws")) == int(r["draws"])
        r["bloom_cpu_us"] = sum(float(r[k]) for k in ("bright_us","horizontal_us","vertical_us"))
        r["upload_bytes"] = sum(int(r[k]) for k in ("batch_vertex_bytes","gui_vertex_bytes","gui_index_bytes",
                                "font_upload_bytes","light_uniform_bytes","material_upload_bytes","shadow_vertex_bytes"))
    fields = ["total_us","geometry_us","mask_submit_us","light_submit_us","shadow_batch_us","shadow_inspect_us",
              "normal_us","light_us","scene_compose_us","bloom_cpu_us","output_us","preview_us","gui_us","gui_build_us","draws","fullscreen","active_lights",
              "active_casters","skipped_casters","occluder_count","edge_count","mask_rebuilds","inspect_rebuilds",
              "mask_draw_calls","masked_light_draw_calls","inspect_draw_calls","preview_draws","shadow_vertex_bytes",
              "upload_bytes","hdr_bytes","shadow_bytes","preview_bytes","material_bytes"]
    lines = [f"## {w}×{h} · {key}", "",
             f"Timestamp bits {first['timestamp_bits']}. Valid GPU samples {len(valid)}. Outcomes {dict(statuses)}.", "",
             "| CPU metric | n | p50 | p95 | p99 |", "|---|---:|---:|---:|---:|"]
    def table(records, keys):
        for k in keys:
            data = sorted(float(r[k]) for r in records if r[k] != "")
            values = [f"{data[math.ceil(len(data)*q)-1]:.3f}" for q in (.5,.95,.99)] if data else ["—"]*3
            lines.append(f"| {k} | {len(data)} | " + " | ".join(values) + " |")
    table(sample, fields)
    if valid:
        lines += ["", "| GPU interval (µs) | n | p50 | p95 | p99 |", "|---|---:|---:|---:|---:|"]
        table(valid, ["total_us"] + [k[:-6]+"_us" for k in first if k.endswith("_draws")])
    return lines + [""]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("run","summarize"))
    action = parser.parse_args().action
    report = ["# M7.4b Release measurements", "",
              "Nearest-rank quantiles of frames 121–1320. CPU times in µs, resident storage and uploads in bytes.", ""]
    for logical in ("800x450","1120x630"):
        for config in CONFIGS:
            if action == "run":
                run(logical, config)
            report += summarize(logical, config)
            print(f"Verified {logical} {config}", flush=True)
    (HERE / "quantiles.md").write_text("\n".join(report))
