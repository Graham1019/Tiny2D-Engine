"""Run every exhibit in a real child, then reopen the first from the same launcher."""
from pathlib import Path
import csv
import re
import shutil
import subprocess
import sys
import tempfile


def exercise(lab, root):
    ids = ["neon", "particles", "shaders", "map", "navigation", "replay", "survivor-classic", "survivor-enhanced", "neon"]
    output = root / "离线 展品"
    capture = root / "Showcase.png"
    result = subprocess.run([lab, "--scene", "showcase", "--showcase-tour", ",".join(ids), "--child-frames", "3",
                             "--showcase-output", output, "--capture", capture, "--metrics", root/"launcher.csv"],
                            cwd=root, capture_output=True, text=True, timeout=140)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert re.findall(r"^showcase_launch exhibit=([\w-]+)", result.stdout, re.M) == ids
    assert re.findall(r"^showcase_exit exhibit=([\w-]+) code=0 signal=0", result.stdout, re.M) == ids
    assert "status=ok scene=showcase launched=9 reaped=9" in result.stdout
    for n, exhibit in enumerate(ids, 1):
        assert (output / f"{n}-{exhibit}.png").read_bytes().startswith(b"\x89PNG")
        assert (output / f"{n}-{exhibit}.csv").stat().st_size > 50
        if exhibit.startswith("survivor-"):
            assert (output / f"{n}-{exhibit}.trace.csv").stat().st_size > 50
    assert capture.read_bytes().startswith(b"\x89PNG")
    with (root/"launcher.csv").open() as stream:
        rows = list(csv.DictReader(stream))
    assert rows and all(int(r["gui_vertices"]) and int(r["gui_indices"]) and float(r["gui_build_us"]) >= 0
                        and float(r["cpu_submit_us"]) >= 0 for r in rows)

    # A copied launcher still finds its own assets; a missing adjacent helper reports failure.
    relocated = root / "离线 package" / "lab"
    relocated.mkdir(parents=True)
    copied = relocated / lab.name
    shutil.copy2(lab, copied)
    (relocated / "assets").symlink_to(lab.parent/"assets", target_is_directory=True)
    run = subprocess.run([copied, "--scene", "showcase", "--showcase-tour", "survivor-classic", "--child-frames", "1"],
                         cwd=root, capture_output=True, text=True, timeout=20)
    assert run.returncode != 0 and "showcase_launch_error exhibit=survivor-classic" in run.stderr, run.stderr
    run = subprocess.run([copied, "--scene", "showcase", "--showcase-tour", "neon,neon", "--child-frames", "2"],
                         cwd=root, capture_output=True, text=True, timeout=30)
    assert run.returncode == 0 and "launched=2 reaped=2" in run.stdout, (run.stderr, run.stdout)
    print("Launched/reaped all eight exhibits; reopened Neon. Missing helper diagnosed from relocated Unicode path.")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as tmp:
        exercise(Path(sys.argv[1]).resolve(), Path(tmp))
