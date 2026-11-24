"""Measure native Showcase composition and the two Survivor presentations on one host."""
import argparse
import csv
import gzip
import math
import shutil
import subprocess
from pathlib import Path

FRAMES = 180
WARMUP = 30


def summarize(directory):
    with (directory / "quantiles.csv").open("w", newline="") as out:
        writer = csv.writer(out, lineterminator="\n")
        writer.writerow(["scene", "metric", "samples", "p50", "p95", "p99"])
        for name in ("showcase", "classic", "enhanced"):
            with gzip.open(directory / f"{name}.csv.gz", "rt", newline="") as raw:
                rows = list(csv.DictReader(raw))
            if [int(r["frame"]) for r in rows] != list(range(1, FRAMES + 1)):
                raise ValueError(f"{name} frame sequence differs")
            sample = rows[WARMUP:]
            for column in rows[0]:
                if column.endswith("_us") and column != "gpu_us":
                    values = sorted(float(r[column]) for r in sample if r[column] and r[column] != "N/A")
                    if values:
                        writer.writerow([name, column, len(values), *(values[math.ceil(q * len(values)) - 1]
                                                                    for q in (.5, .95, .99))])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path, help="a new directory for the dedicated measurement runs")
    parser.add_argument("--summarize", action="store_true", help="only recompute percentiles from captured samples")
    args = parser.parse_args()
    target = args.output
    if not args.summarize:
        target.mkdir(parents=True)
        build = args.build.resolve(strict=True)
        survivor = build / "Tiny2DSurvivor.app/Contents/MacOS/Tiny2DSurvivor"
        scenarios = {"showcase": [build / "lab/tiny2d_lab", "--scene", "showcase"],
                     "classic": [survivor, "--presentation", "classic", "--difficulty", "3"],
                     "enhanced": [survivor, "--presentation", "enhanced", "--difficulty", "3"]}
        for name, command in scenarios.items():
            raw = target / f"{name}.csv"
            with (target / f"{name}.log").open("x") as stream:
                subprocess.run([*command, "--frames", str(FRAMES), "--metrics", raw],
                               stdout=stream, stderr=subprocess.STDOUT, check=True)
            with raw.open("rb") as stream, gzip.GzipFile(target / f"{name}.csv.gz", "wb", mtime=0) as dest:
                shutil.copyfileobj(stream, dest)
            raw.unlink()
    summarize(target)


if __name__ == "__main__":
    main()
