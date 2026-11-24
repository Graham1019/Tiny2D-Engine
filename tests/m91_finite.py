"""Check deterministic map runs and the append-only CSV through the real Lab executable."""
import csv
import gzip
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def exercise(lab, out):
    for args in (["--map-script", "walk"], ["--map-size", "64x40"],
                 ["--scene", "map", "--map-script", "random"],
                 ["--scene", "map", "--map-size", "4x5"],
                 ["--scene", "map", "--map-size", "128x97"],
                 ["--scene", "map", "--map-size", "4294967295x8"],
                 ["--scene", "map", "--view", "normal"],
                 ["--scene", "map", "--lighting", "on"]):
        run = subprocess.run([lab, *args, "--frames", "1"], capture_output=True, timeout=10)
        assert run.returncode and b"Tiny2D Lab:" in run.stderr, args

    legacy = Path(__file__).resolve().parents[1] / "docs/data/m82/aurora.csv.gz"
    with gzip.open(legacy, "rt") as stream:
        old_header = next(csv.reader(stream))

    def capture(label, seed=71, script="walk", view="final", bloom="on"):
        image = out / (label + ".png")
        metrics = out / (label + ".csv")
        subprocess.run([lab, "--scene", "map", "--size", "320x180", "--map-size", "64x40",
                        "--seed", str(seed), "--map-script", script, "--view", view,
                        "--bloom", bloom, "--gpu-timing", "off", "--inspector", "off",
                        "--frames", "96", "--capture", image, "--metrics", metrics],
                       cwd=out, check=True, timeout=30, stdout=subprocess.DEVNULL)
        with metrics.open() as stream:
            reader = csv.DictReader(stream)
            assert reader.fieldnames[:len(old_header)] == old_header
            rows = list(reader)
        assert len(rows) == 96
        for i, row in enumerate(rows, 1):
            assert None not in row and None not in row.values()
            assert (row["scene"], row["seed"], row["map_script"]) == ("map", str(seed), script)
            assert row["tick"] == row["frame"] == str(i) and row["epoch"] == "1"
            assert row["map_width"] == "64" and row["map_height"] == "40"
            assert int(row["total_tiles"]) == 2560
            assert 0 < int(row["visible_tiles"]) < 2560
            assert row["visible_tiles"] == row["submitted_tile_quads"]
            assert int(row["tile_vertex_upload_bytes"]) == int(row["visible_tiles"]) * 144
            assert int(row["tile_draw_calls"]) > 0
            assert bool(row["map_generation_us"]) == bool(row["bfs_validation_us"]) == (i == 1)
            assert row["view"] == view and row["lighting"] == "0"
            assert not row["particle_emission_us"] and not row["shader_gallery_us"]
        return image.read_bytes(), rows

    image, rows = capture("baseline")
    same, replay = capture("repeat")
    assert image == same
    for before, after in zip(rows, replay):
        for field in ("tick", "epoch", "player_x", "player_y", "hit_x", "hit_y",
                      "camera_x", "camera_y", "visible_tiles", "tile_draw_calls"):
            assert before[field] == after[field]
    assert len({(r["player_x"], r["player_y"]) for r in rows}) > 1
    assert any(r["hit_x"] == "1" or r["hit_y"] == "1" for r in rows)
    assert len({(r["camera_x"], r["camera_y"]) for r in rows}) > 1
    assert capture("other-seed", seed=72)[0] != image
    # Compare visible map geometry away from the seed label. Decode a central
    # crop via the system image tool and skip BMP metadata before comparing.
    def world_crop(label):
        bmp = out / (label + ".bmp")
        subprocess.run(["/usr/bin/sips", "-c", "80", "200", "-s", "format", "bmp",
                        out / (label + ".png"), "--out", bmp],
                       check=True, stdout=subprocess.DEVNULL)
        data = bmp.read_bytes()
        assert data[:2] == b"BM"
        return data[struct.unpack_from("<I", data, 10)[0]:]

    assert world_crop("baseline") != world_crop("other-seed")
    still, idle = capture("no-input", script="none")
    assert still != image and len({(r["player_x"], r["player_y"]) for r in idle}) == 1
    assert capture("no-bloom", bloom="off")[0] != image
    for view in ("bright", "blur", "scene"):
        assert capture(view, view=view)[0] != image
    print("96-tick state/PNG repeat, scripted contact, idle, different seed, HDR views and M8.2 CSV prefix passed")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as directory:
        exercise(Path(sys.argv[1]).resolve(), Path(directory))
