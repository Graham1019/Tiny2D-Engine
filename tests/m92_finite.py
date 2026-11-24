"""Exercise offers, scripted replay and terminal draws through the installed Lab entry."""
import csv
import gzip
from pathlib import Path
import subprocess
import sys
import tempfile


def exercise(exe, directory):
    for args in (["--nav-enemies", "16"], ["--scene", "map", "--nav-script", "walk"],
                 ["--scene", "navigation", "--nav-enemies", "-1"],
                 ["--scene", "navigation", "--nav-enemies", "257"],
                 ["--scene", "navigation", "--nav-choice", "random"],
                 ["--scene", "navigation", "--nav-script", "mouse"]):
        result = subprocess.run([exe, *args, "--frames", "1"], capture_output=True, timeout=8)
        assert result.returncode and b"Tiny2D Lab:" in result.stderr, args

    old = Path(__file__).resolve().parents[1]/"docs/data/m91/64x40-1600x900.csv.gz"
    with gzip.open(old, "rt") as stream:
        prefix = next(csv.reader(stream))

    def capture(name, frames=680, count=0, seed=71, choice="first", grid="64x40", debug="off"):
        out = directory / (name + ".csv")
        png = directory / (name + ".png")
        run = subprocess.run([exe, "--scene", "navigation", "--seed", str(seed), "--map-size", grid,
                              "--size", "256x144", "--frames", str(frames), "--nav-enemies", str(count),
                              "--nav-script", "walk", "--nav-choice", choice, "--nav-debug", debug,
                              "--bloom", "on", "--inspector", "off", "--gpu-timing", "off",
                              "--metrics", out, "--capture", png], cwd=directory, capture_output=True,
                             check=True, timeout=90)
        with out.open() as stream:
            reader = csv.DictReader(stream)
            assert reader.fieldnames[:len(prefix)] == prefix
            rows = list(reader)
        assert len(rows) == frames and b"scene=navigation" in run.stdout
        for i, row in enumerate(rows, 1):
            assert None not in row and None not in row.values()
            assert row["frame"] == str(i) and row["scene"] == "navigation"
            assert row["epoch"] == "1" and row["seed"] == str(seed) and row["inspector"] == "0"
            assert int(row["nav_active_enemies"]) <= count == int(row["nav_requested_enemies"])
            assert int(row["nav_field_rebuild_count"]) + int(row["nav_cache_reuse_count"]) == int(row["tick"])+1
            assert int(row["enemy_vertex_upload_bytes"]) == int(row["submitted_enemy_quads"])*144
        return png.read_bytes(), rows

    base, rows = capture("first")
    repeat, again = capture("repeat")
    assert base == repeat
    stable = ["tick", "player_x", "player_y", "nav_phase", "nav_hp", "nav_target_x", "nav_target_y",
              "nav_last_choice", "nav_offer_serial", "nav_choice_serial", "nav_field_rebuild_count"]
    assert all([a[f] for f in stable] == [b[f] for f in stable] for a, b in zip(rows, again))
    assert rows[599]["nav_phase"] == "AwaitingUpgrade" and rows[599]["tick"] == "600"
    assert rows[600]["nav_phase"] == "Playing" and rows[600]["tick"] == "601"
    assert rows[-1]["nav_last_choice"] == "Vitality" and rows[-1]["nav_hp"] == rows[-1]["nav_max_hp"] == "6"
    last, alternate = capture("last", choice="last")
    assert last != base and alternate[-1]["nav_last_choice"] == "Fleet"
    assert float(alternate[-1]["nav_player_speed"]) == 132
    assert any((a["player_x"], a["player_y"]) != (b["player_x"], b["player_y"])
               for a, b in zip(rows[600:], alternate[600:]))
    field, shown = capture("overlay", frames=12, count=16, grid="21x13", debug="all")
    clean, hidden = capture("clean", frames=12, count=16, grid="21x13")
    assert field != clean and int(shown[-1]["submitted_enemy_quads"]) > 0
    assert all(a["nav_field_rebuild_count"] == b["nav_field_rebuild_count"] for a, b in zip(shown, hidden))
    other, changed = capture("seed", frames=12, count=16, grid="21x13", seed=72)
    assert other != clean
    _, dead = capture("terminal", frames=250, count=256, grid="5x5")
    assert dead[-1]["nav_phase"] == "GameOver" and int(dead[-1]["tick"]) < 249
    assert dead[-1]["tick"] == dead[-2]["tick"] and dead[-1]["nav_hp"] == "0"
    print("Finite replay matches PNG and tick state; alternative choice changes movement; "
          "HUD, field/path, seed, terminal freeze and M9.1 schema prefix passed.")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as tmp:
        exercise(Path(sys.argv[1]).resolve(), Path(tmp))
