"""Rerun the M9 authority comparisons and seed fixtures against a built revision."""
import argparse
import csv
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, help="Release build with tests enabled")
    parser.add_argument("output", type=Path, help="directory for new comparison logs")
    args = parser.parse_args()
    build = args.build.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    def record(name, command):
        with (output / name).open("x") as stream:
            subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)

    record("map-regression.log", [sys.executable, "docs/data/m92/check_map_regression.py"])
    record("navigation-regression.log", [sys.executable, "docs/data/m101/check_navigation.py"])
    record("bfs.csv", [build / "tiny2d_m92_search"])
    with (output / "bfs.csv").open(newline="") as stream:
        counts = defaultdict(lambda: [0, 0])
        for row in csv.DictReader(stream):
            if row["enemies"] == "256" and row["walk"] == "1":
                pair = counts[(row["width"], row["height"])]
                pair[0] += int(row["matching_hops"])
                pair[1] += int(row["forward_searches"])
    if counts != {("64", "40"): [4096, 4096], ("128", "96"): [4096, 4096]}:
        raise ValueError(f"Shared and independent BFS mismatch: {dict(counts)}")

    with (output / "cpu-regression.log").open("x") as stream:
        for command in ([build / "tiny2d_m91_generation"], [build / "tiny2d_m92_upgrade"],
                        [build / "tiny2d_m101_evidence", "verify", "docs/data/m101/cross-process/A/events.replay"]):
            subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)
        print("256-enemy walking shared/forward BFS", dict(counts), file=stream)


if __name__ == "__main__":
    main()
