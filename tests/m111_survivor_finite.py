"""Exercise the actual bundled Survivor executable in both presentation modes."""
import csv
from pathlib import Path
import subprocess
import sys
import tempfile


def exercise(exe, tmp):
    def call(*args, success=True):
        run = subprocess.run([exe, *map(str, args)], cwd=tmp, capture_output=True, text=True, timeout=90)
        assert (run.returncode == 0) == success, (args,run.stderr)
        return run.stdout

    for level in (3, 5):
        for mode in ("classic", "enhanced"):
            stem = f"{mode}{level}"
            out = call("--presentation",mode,"--difficulty",level,"--frames",180,
                       "--trace",stem+".csv","--metrics",stem+".metrics","--capture",stem+".png")
            assert "Survivor frames=180" in out and "ticks=180" in out
            with (tmp/(stem+".metrics")).open() as source:
                rows = list(csv.DictReader(source))
            assert len(rows) == 180 and rows[-1]["mode"] == mode
            assert int(rows[-1]["fullscreen_draws"]) == (4 if mode == "enhanced" else 0)
        classic = tmp/f"classic{level}.csv"
        assert classic.read_bytes() == (tmp/f"enhanced{level}.csv").read_bytes()
        assert (tmp/f"classic{level}.png").read_bytes() != (tmp/f"enhanced{level}.png").read_bytes()
        with classic.open() as source:
            rows = list(csv.DictReader(source))
        assert len(rows) == 180 and any("/ProjectileHit/" in row["events"] for row in rows)
    prev = (tmp/"classic3.png").read_bytes()
    call("--frames",1,"--capture","classic3.png",success=False)
    assert prev == (tmp/"classic3.png").read_bytes()
    call("--frames",1,"--capture","duplicate","--trace","./duplicate",success=False)
    call("--frames",1,"--bloom","on",success=False)
    call("--frames",0,success=False)
    print("360 per-tick entity, timer and CombatEvent rows match across standalone classic and HDR processes.")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as path:
        exercise(Path(sys.argv[1]).resolve(), Path(path))
