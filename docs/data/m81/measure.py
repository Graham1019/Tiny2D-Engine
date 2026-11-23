"""Collect or verify the eight M8.1 Release runs. Execute from any directory."""
import argparse
import csv
import gzip
import math
import shlex
import shutil
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
LAB = "build/m81-release/lab/tiny2d_lab"
SIZES = {"800x450": (1600, 900), "1120x630": (2240, 1260)}
LOADS = (0, 1024, 4096, 8192)
METRICS = ("total_us", "update_us", "force_update_us", "snapshot_prepare_us",
           "instance_upload_us", "particle_emission_us", "scene_compose_us", "bright_us",
           "horizontal_us", "vertical_us", "output_us", "draws", "instance_upload_bytes")


def collect(logical, count, archive):
    raw = archive.with_suffix("")
    args = [LAB, "--scene", "particles", "--seed", "71", "--particle-script", "none",
            "--particle-capacity", "8192", "--particle-steady", str(count),
            "--size", logical, "--frames", "1320", "--view", "final", "--lighting", "on",
            "--bloom", "on", "--inspector", "off", "--gpu-timing", "on", "--metrics", str(raw.relative_to(ROOT))]
    with raw.with_suffix(".log").open("w") as out:
        print(shlex.join(args), file=out, flush=True)
        subprocess.run(args, cwd=ROOT, check=True, stdout=out, stderr=subprocess.STDOUT, timeout=240)
    with raw.open("rb") as src, gzip.GzipFile(archive, "wb", mtime=0) as dest:
        shutil.copyfileobj(src, dest)
    raw.unlink()


def summarize(archive, count, dimensions):
    with gzip.open(archive, "rt") as src:
        reader = csv.DictReader(src)
        with gzip.open(HERE.parent / "m74b/800x450-A.csv.gz", "rt") as legacy:
            old = next(csv.reader(legacy))
        assert reader.fieldnames[:len(old)] == old, "M7.4b column order"
        rows = list(reader)
    assert all(None not in row and None not in row.values() for row in rows)
    cpu = [r for r in rows if r["record"] == "cpu"]
    assert [(int(r["frame"]), int(r["tick"])) for r in cpu] == [(i, i) for i in range(1, 1321)]
    sample = [r for r in cpu if r["measurement"] == "1"]
    assert len(sample) == 1200 and int(sample[0]["frame"]) == 121
    measured_gpu = [r for r in rows if r["record"] == "gpu" and r["query_status"] == "completed" and r["measurement"] == "1"]
    for r in sample:
        assert r["scene"] == "particles" and r["seed"] == "71" and r["particle_script"] == "none"
        assert int(r["particle_capacity"]) == 8192 and int(r["particle_steady"]) == count
        assert (int(r["width"]), int(r["height"])) == dimensions, "unexpected real framebuffer dimensions"
        assert int(r["active_particles"]) == int(r["active_instances"]) == count
        assert int(r["active_fields"]) == 4 and int(r["dropped_particles"]) == 0
        assert int(r["particle_draw_calls"]) == bool(count) and int(r["instance_upload_bytes"]) == 32 * count
        assert sum(int(v) for k, v in r.items() if k.endswith("_draws")) + int(r["particle_draw_calls"]) == int(r["draws"])
        assert r["timing"] == r["bloom"] == r["lighting"] == "1" and r["inspector"] == "0"
        if r["timestamp_bits"] == "0":
            assert r["query_status"] == "unavailable" and r["valid_gpu_samples"] == "0" and not measured_gpu
    def quantiles(records, key):
        data = sorted(float(r[key]) for r in records if r[key])
        return [len(data)] + ([data[math.ceil(p * len(data)) - 1] for p in (.5, .95, .99)] if data else [""] * 3)
    print(archive.name, len(sample), "CPU samples,", len(measured_gpu), "valid measured GPU frames")
    for kind, records, keys in (("cpu", sample, METRICS), ("gpu", measured_gpu, ("total_us", "particle_emission_us"))):
        for key in keys:
            yield [*dimensions, count, sample[0]["timestamp_bits"], len(measured_gpu), kind, key, *quantiles(records, key)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("run", "summarize"))
    args = parser.parse_args()
    summary = []
    for logical, dimensions in SIZES.items():
        for count in LOADS:
            archive = HERE / f"{logical}-{count}.csv.gz"
            if args.action == "run":
                collect(logical, count, archive)
            summary.extend(summarize(archive, count, dimensions))
    with (HERE / "quantiles.csv").open("w", newline="") as out:
        writer = csv.writer(out, lineterminator="\n")
        writer.writerow(["width", "height", "particles", "timestamp_bits", "valid_measurement_gpu_samples",
                         "clock", "metric", "n", "p50", "p95", "p99"])
        writer.writerows(summary)


if __name__ == "__main__":
    main()
