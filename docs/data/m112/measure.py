"""Reproduce the final Release fixtures on a Mac. Each collection writes to a new directory."""
import argparse
import csv
import gzip
import math
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SCENES = {
    "lighting": (["--scene", "neon", "--lighting", "on", "--normal-mapping", "on",
                  "--shadows", "on", "--shadow-lights", "4", "--occluders", "demo"],
                 ("total_us", "geometry_us", "mask_submit_us", "light_submit_us", "draws", "hdr_bytes")),
    "particles": (["--scene", "particles", "--particle-script", "none", "--particle-capacity", "8192",
                   "--particle-steady", "8192", "--lighting", "on"],
                  ("total_us", "instance_upload_us", "active_instances", "particle_draw_calls", "instance_upload_bytes")),
    "map": (["--scene", "map", "--map-size", "128x96", "--map-script", "walk"],
            ("total_us", "visible_tile_prepare_us", "total_tiles", "visible_tiles", "tile_draw_calls")),
    "navigation": (["--scene", "navigation", "--map-size", "128x96", "--nav-enemies", "256", "--nav-script", "walk"],
                   ("total_us", "nav_enemy_update_us", "nav_active_enemies", "enemy_draw_calls", "nav_field_build_events")),
}


def archive(source, destination):
    with source.open("rb") as src, gzip.GzipFile(destination, "wb", mtime=0) as dst:
        shutil.copyfileobj(src, dst)


def collect(build, destination):
    destination.mkdir(parents=True)
    with tempfile.TemporaryDirectory() as scratch:
        work = Path(scratch)
        with (destination / "sprite-trials.csv").open("x") as results, (destination / "sprite-environment.log").open("x") as log:
            subprocess.run([build / "tiny2d_benchmark", "--samples", "40"], cwd=work,
                           stdout=results, stderr=log, check=True)
        for source in ("m4-frames.csv", "m4-gpu-frames.csv"):
            archive(work / source, destination / (source + ".gz"))
        for key, (options, _) in SCENES.items():
            raw = work / (key + ".csv")
            command = [build / "lab/tiny2d_lab", *options, "--size", "800x450", "--frames", "360",
                       "--view", "final", "--bloom", "on", "--seed", "71", "--inspector", "off",
                       "--gpu-timing", "on", "--metrics", raw]
            with (destination / (key + ".log")).open("x") as log:
                subprocess.run(command, cwd=work, stdout=log, stderr=subprocess.STDOUT, check=True)
            archive(raw, destination / (key + ".csv.gz"))
        io = work / "memory-vs-file-io"
        io.mkdir()
        for name, command, summarizer in (
            ("snapshot", [build / "tiny2d_m101_benchmark", io], "m101"),
            ("seek", [build / "tiny2d_m102_benchmark"], "m102"),
        ):
            raw = work / (name + ".csv")
            with raw.open("x") as stream:
                subprocess.run(command, stdout=stream, check=True)
            archive(raw, destination / (name + ".csv.gz"))
            with (destination / (name + "-quantiles.csv")).open("x") as out:
                subprocess.run([sys.executable, ROOT / f"docs/data/{summarizer}/quantiles.py", raw], stdout=out, check=True)
        subprocess.run([sys.executable, ROOT / "scripts/measure_showcase.py", build, destination / "presentation"], check=True)


def summarize(directory):
    def rows(name):
        with gzip.open(directory / (name + ".csv.gz"), "rt", newline="") as stream:
            return list(csv.DictReader(stream))

    with (directory / "render-quantiles.csv").open("w", newline="") as out:
        writer = csv.writer(out, lineterminator="\n")
        writer.writerow(["scenario", "metric", "samples", "p50", "p95", "p99"])

        def report(scenario, metric, data):
            ordered = sorted(float(n) for n in data)
            if not ordered:
                raise ValueError(f"{scenario} {metric} lacks measurements")
            writer.writerow([scenario, metric, len(ordered), *(ordered[math.ceil(q * len(ordered)) - 1]
                                                             for q in (.5, .95, .99))])

        sprite = rows("m4-frames")
        for mode in ("immediate", "batch"):
            times = [r["cpu_us"] for r in sprite if r["workload"] == "cpu" and r["sprites"] == "500" and r["renderer"] == mode]
            if len(times) != 200:
                raise ValueError("expected five 40-frame trials")
            report("500-" + mode, "cpu_submit_us", times)
        for key, (_, metrics) in SCENES.items():
            source = rows(key)
            data = [r for r in source if r["record"] == "cpu" and r["measurement"] == "1"]
            if len(data) != 240 or not all(r["width"] == "1600" and r["height"] == "900" for r in data):
                raise ValueError(f"{key} expected 240 Retina framebuffer samples")
            if key == "particles" and any(r["active_instances"] != "8192" or r["particle_draw_calls"] != "1" for r in data):
                raise ValueError("particle fixture lost its instanced load")
            if key == "map" and any(int(r["visible_tiles"]) >= int(r["total_tiles"]) for r in data):
                raise ValueError("map culling fixture did not cull")
            for metric in metrics:
                report(key, metric, [r[metric] for r in data if r[metric]])
            gpu = [r for r in source if r["record"] == "gpu" and r["measurement"] == "1" and r["query_status"] == "completed"]
            if gpu:
                report(key, "gpu_total_us", [r["total_us"] for r in gpu])
            else:
                writer.writerow([key, "gpu_total_us", 0, "N/A", "N/A", "N/A"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--summarize", action="store_true", help="recompute render percentiles from retained samples")
    options = parser.parse_args()
    if not options.summarize:
        collect(options.build.resolve(strict=True), options.output.resolve())
    summarize(options.output)
