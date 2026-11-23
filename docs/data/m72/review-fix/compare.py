#!/usr/bin/env python3
"""Compare two Release Lab binaries with the same graphical login and no user input."""
import argparse
import csv
import gzip
import hashlib
import math
import shlex
import subprocess
import tempfile
import time
from pathlib import Path

LAST_FRAME = 1320
WARMUP = 120
OPTIONS = "--scene neon --seed 71 --size 800x450 --view final --bloom on --inspector on --gpu-timing on --previews all".split()


def reached_last_frame(path):
    if not path.exists():
        return False
    with path.open("rb") as file:
        file.seek(max(0, path.stat().st_size - 4096))
        # At both ends the buffer may contain a partial line.
        for line in file.read().split(b"\n")[1:-1]:
            if line.startswith(b"cpu,") and int(line.split(b",", 2)[1]) >= LAST_FRAME:
                return True
    return False


def capture(exe, mode, path):
    args = [str(exe), *OPTIONS, "--metrics", str(path)]
    if mode == "finite":
        args += ["--frames", str(LAST_FRAME)]
    print("$", shlex.join(args), flush=True)
    with path.with_suffix(".log").open("w") as log:
        if mode == "finite":
            subprocess.run(args, check=True, stdout=log, stderr=log, timeout=90)
        else:
            # Interactive mode exercises the real input router and vsync. Stop only
            # our child, after all sample rows have reached disk, outside CPU timing.
            child = subprocess.Popen(args, stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 90
                while not reached_last_frame(path):
                    if child.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError("interactive run exited early or timed out")
                    time.sleep(0.2)
            finally:
                if child.poll() is None:
                    child.terminate()
                child.wait(timeout=10)


def report(path, mode, archive=None):
    if mode == "finite" and path.suffix != ".gz":
        for line in path.with_suffix(".log").read_text().splitlines():
            if line.startswith(("profile=", "timestamp=", "gpu_requested=", "status=")):
                print(line)
    source = gzip.open(path, "rt") if path.suffix == ".gz" else path.open()
    with source as file:
        samples = [r for r in csv.DictReader(line for line in file if line.endswith("\n"))
                   if r["record"] == "cpu" and WARMUP < int(r["frame"]) <= LAST_FRAME]
    assert [int(r["frame"]) for r in samples] == list(range(WARMUP + 1, LAST_FRAME + 1))
    ticks = [int(r["tick"]) for r in samples]
    assert ticks == sorted(ticks) and ticks[-1] > ticks[0], "interactive window must advance"
    if mode == "finite":
        assert ticks == list(range(WARMUP + 1, LAST_FRAME + 1))
    assert all(r["preview_mask"] == "15" and r["preview_draws"] == "4" and
               r["view"] == "final" and r["bloom"] == r["hud"] == r["epoch"] == "1" and
               r["debug_draws"] == "0" for r in samples)
    if archive is not None:
        with gzip.open(archive, "xt", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=samples[0].keys())
            writer.writeheader()
            writer.writerows(samples)
    print(f"frames=121..1320 n={len(samples)} ticks={ticks[0]}..{ticks[-1]}")
    keys = ("width", "height", "draws", "pass_mask", "preview_draws", "gui_draws", "timestamp_bits", "query_status")
    print(" ".join(f"{key}={','.join(sorted({r[key] for r in samples}))}" for key in keys))
    for key in ("total_us", "gui_build_us", "gui_us"):
        data = sorted(float(r[key]) for r in samples)
        print(key, "p50/p95/p99", " / ".join(f"{data[math.ceil(p*len(data))-1]:.3f}" for p in (.5, .95, .99)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="action", required=True)
    run = actions.add_parser("run", help="collect 1200 complete CPU rows per binary and mode")
    run.add_argument("before", type=Path)
    run.add_argument("after", type=Path)
    run.add_argument("--samples-dir", type=Path, required=True, help="new directory for four CSV.gz files")
    summarize = actions.add_parser("summarize", help="recompute the report without launching any windows")
    summarize.add_argument("samples_dir", type=Path)
    args = parser.parse_args()
    order = [("interactive", "before"), ("interactive", "after"), ("finite", "after"), ("finite", "before")]
    if args.action == "summarize":
        for mode, label in order:
            print(f"\n{mode} {label}")
            report(args.samples_dir / f"{mode}-{label}.csv.gz", mode)
    else:
        args.samples_dir.mkdir(parents=True, exist_ok=False)
        binaries = {"before": args.before.resolve(), "after": args.after.resolve()}
        for label, exe in binaries.items():
            print(label, exe, "sha256=" + hashlib.sha256(exe.read_bytes()).hexdigest())
        with tempfile.TemporaryDirectory(prefix="tiny2d-review-perf-") as directory:
            for mode, label in order:
                path = Path(directory) / f"{mode}-{label}.csv"
                print(f"\n{mode} {label}")
                capture(binaries[label], mode, path)
                report(path, mode, args.samples_dir / (path.name + ".gz"))
