"""Collect or recompute M9.2 Release navigation measurements (Retina scale 2)."""
import argparse
import csv
import gzip
import math
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
GRIDS = ((64, 40), (128, 96))
SIZES = (0, 16, 64, 256)
SCRIPTS = ("none", "walk")


def quantiles(rows, column):
    values = sorted(float(row[column]) for row in rows if row[column])
    if not values:
        return "", "", ""
    return tuple(values[math.ceil(len(values)*q)-1] for q in (.50, .95, .99))


def write_csv(path, records):
    with path.open("w", newline="") as out:
        writer = csv.DictWriter(out, fieldnames=records[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(records)


def collect(binary, grid, enemies, script, pixels, archive):
    with tempfile.TemporaryDirectory() as temp:
        raw = Path(temp) / "frames.csv"
        args = [binary, *map(str, grid), str(enemies), script,
                str(pixels[0]//2), str(pixels[1]//2), raw]
        subprocess.run(args, check=True, cwd=ROOT, timeout=240)
        with raw.open("rb") as src, gzip.GzipFile(archive, "wb", mtime=0) as dst:
            shutil.copyfileobj(src, dst)


def analyze(path, grid, enemies, script, pixels):
    with gzip.open(path, "rt") as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 1320
    trace = []
    for i, row in enumerate(rows, 1):
        assert int(row["frame"]) == i and int(row["enemies"]) == enemies and row["script"] == script
        assert tuple(int(row[c]) for c in ("width", "height")) == grid
        assert tuple(int(row[c]) for c in ("fb_width", "fb_height")) == pixels
        target = tuple(row[c] for c in ("target_x", "target_y"))
        rebuild = i == 1 or trace[-1] != target
        assert bool(row["field_us"]) == rebuild
        assert int(row["rebuilds"]) + int(row["reuses"]) == i+1  # includes spawn field
        assert int(row["rebuilds"]) == (int(rows[i-2]["rebuilds"]) if i > 1 else 0) + rebuild
        assert int(row["nodes"]) == grid[0]*grid[1]
        assert 0 < int(row["reachable"]) == int(row["expanded"]) == int(row["visited"])
        assert int(row["tile_draws"]) == 1 and int(row["tile_bytes"]) == int(row["visible_tiles"])*144
        submitted = int(row["submitted_enemies"])
        assert 0 <= submitted <= enemies and int(row["enemy_bytes"]) == submitted*144
        assert int(row["enemy_draws"]) == bool(submitted)
        assert int(row["timestamp_bits"]) or int(row["valid_gpu_samples"]) == 0
        trace.append(target)
    assert (len(set(trace)) > 1) == (script == "walk")
    last = rows[-1]
    measured = rows[120:]
    record = dict(map="%dx%d" % grid, framebuffer="%dx%d" % pixels, enemies=enemies, script=script,
                  nodes=last["nodes"], reachable=last["reachable"], rebuilds=last["rebuilds"], reuses=last["reuses"],
                  timestamp_bits=last["timestamp_bits"], gpu_samples=last["valid_gpu_samples"],
                  graph_us=rows[0]["graph_us"], initial_field_us=rows[0]["field_us"],
                  measured_field_events=sum(bool(r["field_us"]) for r in measured))
    for column in ("field_us", "enemy_us", "collision_us", "frame_us", "visible_prepare_us",
                   "tile_draws", "tile_bytes", "enemy_draws", "enemy_bytes"):
        for label, value in zip(("p50", "p95", "p99"), quantiles(measured, column)):
            record[f"{column}_{label}"] = value
    return trace, record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("run", "summarize"))
    parser.add_argument("--build", type=Path, default=ROOT/"build/m92-release")
    args = parser.parse_args()
    benchmark = args.build.resolve() / "lab/tiny2d_m92_benchmark"
    if args.action == "run":
        with (HERE/"search.csv").open("w") as out:
            subprocess.run([args.build.resolve()/"tiny2d_m92_search"], check=True, stdout=out)
    results, tracks = [], {}
    for grid in GRIDS:
        for enemies in SIZES:
            for script in SCRIPTS:
                for pixels in ((1600, 900), (2240, 1260)):
                    if pixels[0] == 2240 and (grid != GRIDS[-1] or enemies != max(SIZES)):
                        continue
                    name = f"{grid[0]}x{grid[1]}-{enemies}-{script}-{pixels[0]}x{pixels[1]}.csv.gz"
                    if args.action == "run":
                        collect(benchmark, grid, enemies, script, pixels, HERE/name)
                    trace, record = analyze(HERE/name, grid, enemies, script, pixels)
                    assert tracks.setdefault((grid, script), trace) == trace
                    results.append(record)
    write_csv(HERE/"quantiles.csv", results)
    with (HERE/"search.csv").open() as stream:
        searches = list(csv.DictReader(stream))
    assert len(searches) == 2*4*2*16
    summaries = []
    for grid in GRIDS:
        for enemies in SIZES:
            for walk in (0, 1):
                group = [r for r in searches if (int(r["width"]), int(r["height"])) == grid
                         and int(r["enemies"]) == enemies and int(r["walk"]) == walk]
                assert len(group) == 16 and all(int(r["matching_hops"]) == enemies for r in group)
                assert (len({(r["target_x"], r["target_y"]) for r in group}) > 1) == bool(walk)
                record = dict(map="%dx%d" % grid, enemies=enemies, script=SCRIPTS[walk])
                for c in ("shared_builds", "shared_expanded", "forward_searches", "forward_expanded", "matching_hops"):
                    record[c] = sum(int(r[c]) for r in group)
                for c in ("shared_us", "forward_us"):
                    record[c] = round(sum(float(r[c]) for r in group), 3)
                summaries.append(record)
    write_csv(HERE/"search-summary.csv", summaries)


if __name__ == "__main__":
    main()
