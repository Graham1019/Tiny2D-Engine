"""Collect the Release gallery runs and screenshots, or recompute their quantiles."""
import argparse
import csv
import gzip
import math
from pathlib import Path
import shlex
import shutil
import struct
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
MEDIA = ROOT / "docs/media/m82"
EFFECTS = ("aurora", "orbitals", "contours")
FRAME_COUNT = 1320
WARMUP = 120
CPU_METRICS = ("total_us", "shader_gallery_us", "bright_us", "horizontal_us",
               "vertical_us", "output_us", "draws")
GPU_METRICS = ("total_us", "shader_gallery_us")


def collect(exe, effect, archive):
    MEDIA.mkdir(parents=True, exist_ok=True)
    raw = archive.with_suffix("")
    args = [exe, "--scene", "shaders", "--effect", effect, "--size", "800x450",
            "--frames", str(FRAME_COUNT), "--view", "final", "--bloom", "on",
            "--inspector", "off", "--gpu-timing", "on",
            "--metrics", str(raw.relative_to(ROOT)),
            "--capture", str((MEDIA / f"{effect}.png").relative_to(ROOT))]
    with (HERE / f"{effect}.log").open("w") as log:
        print(shlex.join(args), file=log, flush=True)
        subprocess.run(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=240)
    with raw.open("rb") as src, gzip.GzipFile(archive, "wb", mtime=0) as dest:
        shutil.copyfileobj(src, dest)
    raw.unlink()


def summarize(effect, archive):
    with gzip.open(archive, "rt") as stream:
        reader = csv.DictReader(stream)
        with gzip.open(HERE.parent / "m81/800x450-0.csv.gz", "rt") as previous:
            legacy = next(csv.reader(previous))
        assert reader.fieldnames[:len(legacy)] == legacy, "M8.1 column order"
        rows = list(reader)
    assert all(None not in row and None not in row.values() for row in rows)
    cpu = [row for row in rows if row["record"] == "cpu"]
    assert [int(row["frame"]) for row in cpu] == list(range(1, FRAME_COUNT + 1))
    with (MEDIA / f"{effect}.png").open("rb") as img:
        header = img.read(24)
    assert header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR"
    width, height = struct.unpack(">II", header[16:])
    for row in cpu:
        assert row["frame"] == row["tick"] and row["measurement"] == str(int(int(row["tick"]) > WARMUP))
        assert row["scene"] == "shaders" and row["gallery_effect"] == effect and row["shader_version"] == "1"
        assert (int(row["width"]), int(row["height"])) == (width, height)
        assert (row["logical_w"], row["logical_h"]) == ("800", "450")
        assert row["view"] == "final" and row["inspector"] == row["lighting"] == "0"
        assert row["timing"] == row["bloom"] == "1" and row["shader_gallery_us"]
        assert int(row["pass_mask"]) & (1 << 17) and not row["world_us"] and not row["particle_emission_us"]

    sample = cpu[WARMUP:]
    gpu = [r for r in rows if r["record"] == "gpu" and r["query_status"] == "completed" and r["measurement"] == "1"]
    bits = sample[0]["timestamp_bits"]
    assert all(r["timestamp_bits"] == bits for r in sample)
    if bits == "0":
        assert not gpu and all(r["query_status"] == "unavailable" and r["valid_gpu_samples"] == "0" for r in sample)
    for clock, records, metrics in (("cpu", sample, CPU_METRICS), ("gpu", gpu, GPU_METRICS)):
        for metric in metrics:
            values = sorted(float(r[metric]) for r in records)
            quantiles = [values[math.ceil(p * len(values)) - 1] for p in (.50, .95, .99)] if values else [""] * 3
            yield [effect, width, height, bits, clock, metric, len(values), *quantiles]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("run", "summarize"))
    parser.add_argument("--lab", default="build/m82-release/lab/tiny2d_lab",
                        help="absolute path or path relative to the repository root")
    args = parser.parse_args()
    summary = []
    for effect in EFFECTS:
        archive = HERE / f"{effect}.csv.gz"
        if args.action == "run":
            collect(args.lab, effect, archive)
        summary.extend(summarize(effect, archive))
    with (HERE / "quantiles.csv").open("w", newline="") as out:
        writer = csv.writer(out, lineterminator="\n")
        writer.writerow(["effect", "width", "height", "timestamp_bits", "clock", "metric", "n", "p50", "p95", "p99"])
        writer.writerows(summary)


if __name__ == "__main__":
    main()
