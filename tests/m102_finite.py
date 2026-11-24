"""Replay Lab disk/GL evidence. Pass --evidence NEW_DIR to retain the files."""
import argparse
import csv
import hashlib
from pathlib import Path
import subprocess
import tempfile


def exercise(exe, out):
    rows = []

    def run(name, frames, args, panel=False):
        args = [str(exe), "--scene", "replay", "--size", "800x450", "--bloom", "on",
                "--view", "final", "--gpu-timing", "off", "--inspector", "on" if panel else "off",
                "--timeline-view", "on", "--frames", str(frames), "--capture", name + ".png",
                "--snapshot-out", name + ".snap", "--metrics", name + ".csv", *map(str, args)]
        result = subprocess.run(args, cwd=out, text=True, capture_output=True, timeout=100)
        print("$", " ".join(args))
        assert result.returncode == 0, result.stderr
        print(result.stdout.splitlines()[-1])
        with (out / (name + ".csv")).open() as stream:
            frame = list(csv.DictReader(stream))[-1]
        entry = {"run": name, **{key: frame[key] for key in (
            "tick", "epoch", "nav_phase", "nav_last_choice", "nav_hp", "nav_player_speed", "state_hash",
            "event_sequence", "recorded_events", "verified_events", "branch_id", "parent_branch_id",
            "fork_cursor", "event_type", "checkpoint_count", "checkpoint_bytes", "replay_mode")}}
        entry["snapshot_sha256"] = hashlib.sha256((out / (name + ".snap")).read_bytes()).hexdigest()
        entry["png_sha256"] = hashlib.sha256((out / (name + ".png")).read_bytes()).hexdigest()
        rows.append(entry)
        return entry

    parent = run("record-parent", 625, ["--seed", "71", "--nav-enemies", "0", "--nav-script", "walk",
                                        "--nav-choice", "first", "--record", "parent.replay"])
    source = ["--replay", "parent.replay"]
    original = (out / "parent.replay").read_bytes()
    chosen = run("parent-vitality", 4, [*source, "--seek-event", "601"], panel=True)
    offer = run("seek-before-choice", 4, [*source, "--seek-event", "600"], panel=True)
    assert chosen["tick"] == offer["tick"] == "600"
    assert chosen["event_sequence"] == "601" and chosen["event_type"] == "ChooseUpgrade"
    assert chosen["nav_last_choice"] == "Vitality" and chosen["nav_hp"] == "6"
    assert offer["event_sequence"] == "600" and offer["nav_phase"] == "AwaitingUpgrade"
    assert int(offer["checkpoint_count"]) > 1 and int(offer["checkpoint_bytes"]) <= 64 * 1024 * 1024

    # Child event one chooses Fleet at the same historical tick. Its first snapshot is the fork state.
    child = run("record-child", 26, [*source, "--fork-at", "600", "--branch-id", "102",
                                     "--nav-choice", "last", "--nav-script", "walk", "--branch-out", "fleet.branch"])
    assert child["nav_last_choice"] == "Fleet" and child["nav_player_speed"] == "132"
    assert child["parent_branch_id"] == offer["branch_id"] and child["fork_cursor"] == "600"
    branch = ["--branch-in", "fleet.branch"]
    fork = run("fork-base", 4, [*branch, "--seek-event", "0"], panel=True)
    alternate = run("child-fleet", 4, [*branch, "--seek-event", "1"], panel=True)
    assert fork["state_hash"] == offer["state_hash"] and fork["snapshot_sha256"] == offer["snapshot_sha256"]
    assert alternate["tick"] == chosen["tick"] and alternate["nav_last_choice"] == "Fleet"
    assert alternate["event_sequence"] == "1" and alternate["state_hash"] != chosen["state_hash"]
    replayed = run("independent-child", 27, branch)
    assert replayed["verified_events"] == "26" and replayed["replay_mode"] == "Playback finished"
    for key in ("snapshot_sha256", "png_sha256"):
        assert replayed[key] == child[key], key
    unchanged = run("independent-parent", 626, source)
    assert unchanged["verified_events"] == "626"
    assert unchanged["snapshot_sha256"] == parent["snapshot_sha256"] and unchanged["png_sha256"] == parent["png_sha256"]
    assert original == (out / "parent.replay").read_bytes()

    for bad in ([*source, "--seek-event", "627"], [*source, "--seek-event", "1", "--fork-at", "1"],
                [*branch, "--nav-script", "walk"], [*source, "--branch-out", "bad.branch"],
                [*branch, "--replay", "parent.replay"]):
        result = subprocess.run([exe, "--scene", "replay", "--frames", "1", *bad], cwd=out,
                                text=True, capture_output=True, timeout=30)
        assert result.returncode and "Tiny2D Lab:" in result.stderr, bad
    with (out / "boundaries.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    print("Parent and child independently reproduce final canonical snapshots and GL pixels;"
          " cursor 600 and child base are byte-identical; Vitality and Fleet diverge at tick 600.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("lab", type=Path)
    parser.add_argument("--evidence", type=Path)
    args = parser.parse_args()
    if args.evidence:
        args.evidence.mkdir(parents=True, exist_ok=False)
        exercise(args.lab.resolve(), args.evidence.resolve())
    else:
        with tempfile.TemporaryDirectory() as directory:
            exercise(args.lab.resolve(), Path(directory))
