"""Run the six Release map benchmarks, or verify and summarize archived frame records."""
import argparse
import csv
import gzip
import math
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
GRIDS = ((64, 40), (96, 64), (128, 96))
FRAMEBUFFERS = ((1600, 900), (2240, 1260))
METRICS = ("total_us", "update_us", "collision_us", "visible_tile_prepare_us", "visible_tiles",
           "submitted_tile_quads", "tile_draw_calls", "tile_vertex_upload_bytes")
WARMUP = 120
COUNT = 1200


def collect(exe, grid, pixels, archive):
    with tempfile.TemporaryDirectory() as tmp:
        raw = Path(tmp) / "samples.csv"
        args = [exe, "--scene", "map", "--map-size", "%dx%d" % grid, "--size",
                "%dx%d" % (pixels[0] // 2, pixels[1] // 2), "--frames", str(WARMUP + COUNT),
                "--seed", "71", "--map-script", "walk", "--view", "final", "--bloom", "on",
                "--gpu-timing", "on", "--inspector", "off", "--metrics", str(raw)]
        with archive.with_suffix("").with_suffix(".log").open("w") as log:
            print(shlex.join(map(str, args)), file=log, flush=True)
            subprocess.run(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
        with raw.open("rb") as source, gzip.GzipFile(archive, "wb", mtime=0) as result:
            shutil.copyfileobj(source, result)


def quantiles(records, field):
    values = sorted(float(r[field]) for r in records)
    return [values[math.ceil(q * len(values)) - 1] for q in (.50, .95, .99)] if values else [""] * 3


def summarize(archive, grid, pixels):
    with gzip.open(archive, "rt") as stream:
        rows = list(csv.DictReader(stream))
    assert all(None not in r and None not in r.values() for r in rows)
    cpu = [r for r in rows if r["record"] == "cpu"]
    assert len(cpu) == COUNT + WARMUP
    track = []
    for i, r in enumerate(cpu, 1):
        assert r["scene"] == "map" and r["seed"] == "71" and r["map_script"] == "walk"
        assert (int(r["width"]), int(r["height"])) == pixels
        assert (int(r["map_width"]), int(r["map_height"])) == grid
        assert int(r["frame"]) == int(r["tick"]) == i and int(r["measurement"]) == (i > WARMUP)
        assert r["inspector"] == r["lighting"] == "0" and r["bloom"] == r["timing"] == "1"
        assert bool(r["map_generation_us"]) == bool(r["bfs_validation_us"]) == (i == 1)
        assert int(r["total_tiles"]) == grid[0] * grid[1]
        assert 0 < int(r["visible_tiles"]) < int(r["total_tiles"])
        assert r["visible_tiles"] == r["submitted_tile_quads"]
        assert int(r["tile_vertex_upload_bytes"]) == int(r["visible_tiles"]) * 144
        track.append(tuple(r[f] for f in ("camera_x", "camera_y", "visible_tiles")))
    assert len(set(track)) > 1
    gpu = [r for r in rows if r["record"] == "gpu" and r["measurement"] == "1" and r["query_status"] == "completed"]
    bits = int(cpu[0]["timestamp_bits"])
    if bits == 0:
        assert not gpu and all(r["valid_gpu_samples"] == "0" and r["query_status"] == "unavailable" for r in cpu)
    prefix = ["%dx%d" % grid, "%dx%d" % pixels, bits, len(gpu)]
    summary = [prefix + [f, COUNT, *quantiles(cpu[WARMUP:], f)] for f in METRICS]
    summary.extend(prefix + [f, 1, *quantiles(cpu[:1], f)] for f in ("map_generation_us", "bfs_validation_us"))
    summary.append(prefix + ["gpu_total_us", len(gpu), *quantiles(gpu, "total_us")])
    return track, summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("run", "summarize"))
    parser.add_argument("--lab", default="build/m91-release/lab/tiny2d_lab")
    args = parser.parse_args()
    result = []
    reference = None
    for grid in GRIDS:
        for pixels in FRAMEBUFFERS:
            stem = "%dx%d-" % grid + "%dx%d" % pixels
            archive = HERE / (stem + ".csv.gz")
            if args.action == "run":
                collect(args.lab, grid, pixels, archive)
            track, summary = summarize(archive, grid, pixels)
            if reference is None:
                reference = track
            assert track == reference, "camera path or visible tile coverage differs between runs"
            result.extend(summary)
    with (HERE / "quantiles.csv").open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("map", "framebuffer", "timestamp_bits", "completed_gpu_measurements", "metric", "n", "p50", "p95", "p99"))
        writer.writerows(result)


if __name__ == "__main__":
    main()
