"""Cross-process, on-disk snapshot continuation and verified GL replay."""
import csv
import gzip
from pathlib import Path
import subprocess
import sys
import tempfile


def exercise(exe, out):
    with gzip.open(Path(__file__).resolve().parents[1] / "docs/data/m92/survival.csv.gz", "rt") as stream:
        prefix = next(csv.reader(stream))

    def invoke(*args, valid=True):
        run = subprocess.run([exe, "--scene", "replay", "--size", "256x144", *map(str, args)],
                             cwd=out, capture_output=True, text=True, timeout=100)
        if valid:
            assert run.returncode == 0, run.stderr
            assert "status=ok scene=replay" in run.stdout
        else:
            assert run.returncode != 0 and "Tiny2D Lab:" in run.stderr, (args, run.stderr)
        return run

    def render(name, count, *args):
        invoke("--frames", count, "--metrics", name + ".csv", "--capture", name + ".png",
               "--snapshot-out", name + ".snap", *args)
        with (out / (name + ".csv")).open() as stream:
            reader = csv.DictReader(stream)
            assert reader.fieldnames[:len(prefix)] == prefix
            rows = list(reader)
        assert len(rows) == count and all(None not in row.values() and row["scene"] == "replay" for row in rows)
        return rows

    initial = ("--nav-script", "walk", "--nav-enemies", 16, "--seed", 71)
    whole = render("whole", 71, *initial)
    render("middle", 29, *initial)
    continued = render("continued", 42, "--snapshot-in", "middle.snap", "--nav-script", "walk", "--record", "walk.replay")
    played = render("played", 42, "--replay", "walk.replay")
    for i, (ref, saved, verified) in enumerate(zip(whole[29:], continued, played), 1):
        for column in ("state_hash", "tick", "epoch", "nav_hp", "nav_phase", "player_x", "player_y"):
            assert ref[column] == saved[column] == verified[column], (i, column)
        assert saved["event_sequence"] == verified["event_sequence"] == verified["verified_events"] == str(i)
    assert played[-1]["replay_mode"] == "Playback finished"
    for suffix in (".png", ".snap"):
        expected = (out / ("whole" + suffix)).read_bytes()
        assert expected == (out / ("continued" + suffix)).read_bytes() == (out / ("played" + suffix)).read_bytes()

    # The finite live frame can choose then advance. Playback exposes both event boundaries.
    before = render("offer", 600, "--nav-enemies", 0)
    chosen = render("chosen", 1, "--snapshot-in", "offer.snap", "--nav-choice", "first", "--record", "choice.replay")
    events = render("choices", 3, "--replay", "choice.replay")
    assert before[-1]["nav_phase"] == "AwaitingUpgrade" and before[-1]["tick"] == "600"
    assert events[0]["nav_last_choice"] == "Vitality" and events[0]["tick"] == "600"
    assert events[0]["nav_hp"] == events[0]["nav_max_hp"] == "6"
    assert [r["verified_events"] for r in events] == ["1", "2", "2"]
    assert events[-1]["state_hash"] == chosen[0]["state_hash"]
    for suffix in (".png", ".snap"):
        assert (out / ("choices" + suffix)).read_bytes() == (out / ("chosen" + suffix)).read_bytes()

    for args in [
        ("--replay", "walk.replay", "--nav-choice", "last"),
        ("--replay", "walk.replay", "--nav-script", "none"),
        ("--replay", "walk.replay", "--record", "another"),
        ("--replay", "walk.replay", "--snapshot-in", "middle.snap"),
        ("--snapshot-in", "middle.snap", "--seed", 71),
        ("--snapshot-in", "middle.snap", "--map-size", "64x40"),
        ("--snapshot-in", "middle.snap", "--nav-enemies", 16),
        ("--snapshot-in", "middle.snap", "--metrics", "./middle.snap"),
    ]:
        invoke("--frames", 1, *args, valid=False)
    intact = (out / "middle.snap").read_bytes()
    invoke("--frames", 1, "--snapshot-out", "middle.snap", valid=False)
    assert (out / "middle.snap").read_bytes() == intact
    (out / "truncated").write_bytes((out / "walk.replay").read_bytes()[:-1])
    invoke("--frames", 1, "--replay", "truncated", valid=False)
    print("Restored at tick 29; compared 42 event hashes and exact final snapshot/PNG across three processes.")
    print("Tick 600 Vitality choice, terminal transport latch, M9.2 CSV prefix, CLI conflicts and no-clobber passed.")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as tmp:
        exercise(Path(sys.argv[1]).resolve(), Path(tmp))
