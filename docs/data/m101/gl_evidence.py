"""Run from the checkout. The destination must be a fresh directory."""
import argparse
import csv
import hashlib
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument("lab", type=Path)
p.add_argument("destination", type=Path)
a = p.parse_args()
out = a.destination.resolve()
out.mkdir(parents=True, exist_ok=False)
exe = a.lab.resolve()
base = [exe, "--scene", "replay", "--size", "800x450", "--view", "final", "--bloom", "on"]
def run(tag, frames, args, panel=False):
    cmd = [*base, "--frames", str(frames), "--gpu-timing", "on" if panel else "off",
           "--inspector", "on" if panel else "off", "--capture", out/f"{tag}.png",
           "--metrics", out/f"{tag}.csv", *args]
    print("$", " ".join(map(str, cmd)), flush=True)
    stdout = subprocess.check_output(cmd, text=True, timeout=120)
    print(stdout, end="")
    with (out/f"{tag}.csv").open() as f:
        return list(csv.DictReader(f))

cut, length = 417, 151
walk = ["--nav-script", "walk", "--nav-choice", "first"]
run("checkpoint", cut, [*walk, "--snapshot-out", out/"mid.snap"])
continuous = run("continuous", cut+length, [*walk, "--snapshot-out", out/"continuous.snap"])
recorded = run("recorded", length, ["--snapshot-in", out/"mid.snap", *walk, "--record", out/"suffix.replay",
                                    "--snapshot-out", out/"recorded.snap"])
replayed = run("replayed", length, ["--replay", out/"suffix.replay", "--snapshot-out", out/"replayed.snap"])
def authority(rows):
    return [(r["epoch"],r["tick"],r["state_hash"]) for r in rows]
assert len(recorded)==length and authority(continuous[cut:])==authority(recorded)==authority(replayed)
assert int(replayed[-1]["verified_events"])==length
for suffix in ("png","snap"):
    files=[out/f"{name}.{suffix}" for name in ("continuous","recorded","replayed")]
    data = files[0].read_bytes()
    assert all(f.read_bytes()==data for f in files)
    print(f"three independent final {suffix} files identical: bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}")
print(f"matched all {length} event boundaries starting at tick {cut}")
run("inspector", length+1, ["--replay", out/"suffix.replay", "--nav-debug","path"], panel=True)
