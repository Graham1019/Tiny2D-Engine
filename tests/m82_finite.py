"""Exercise the installed layout, tick clock and live file isolation through the Lab CLI."""
import csv
import gzip
import os
from pathlib import Path
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def exercise(executable, stage):
    relocated = stage / "Lab Demo"
    relocated.mkdir()
    app = relocated / executable.name
    shutil.copy2(executable, app)
    shutil.copytree(executable.parent / "assets", relocated / "assets")
    cwd = stage / "other working directory"
    cwd.mkdir()
    args = [app, "--scene", "shaders", "--effect", "aurora", "--frames", "360",
            "--size", "192x120", "--inspector", "off", "--gpu-timing", "off"]

    def run(capture, metrics):
        subprocess.run([*args, "--capture", capture, "--metrics", metrics],
                       cwd=cwd, check=True, stdout=subprocess.DEVNULL, timeout=45)

    baseline = stage / "baseline.png"
    metrics = stage / "baseline.csv"
    run(baseline, metrics)
    with metrics.open() as stream:
        rows = csv.DictReader(stream)
        header = rows.fieldnames
        historical = Path(__file__).resolve().parents[1] / "docs/data/m81/800x450-0.csv.gz"
        with gzip.open(historical, "rt") as old:
            legacy = next(csv.reader(old))
        if header[:len(legacy)] != legacy:
            raise AssertionError("M8.1 CSV header/order changed")
        rows = list(rows)
    if len(rows) != 360:
        raise AssertionError(f"expected 360 finite frame records, found {len(rows)}")
    for frame, row in enumerate(rows, 1):
        if (None in row or None in row.values() or row["scene"] != "shaders"
                or int(row["tick"]) != frame or int(row["frame"]) != frame
                or row["gallery_effect"] != "aurora" or row["shader_version"] != "1"
                or not int(row["pass_mask"]) & (1 << 17) or not row["shader_gallery_us"]
                or row["particle_emission_us"] or row["active_particles"]):
            raise AssertionError(f"incorrect gallery metrics in frame {frame}: {row}")

    # The pipe provides a frame-completion witness and backpressure. Stop the
    # actual process before replacing its executable-adjacent shader, then resume.
    edited = stage / "edited-during-run.png"
    fragment = relocated / "assets/shaders/gallery/aurora.frag"
    fifo = stage / "in-flight.csv"
    os.mkfifo(fifo)
    reader = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)
    with os.fdopen(reader, "rb", buffering=0) as stream, subprocess.Popen(
        [*args, "--capture", edited, "--metrics", fifo], cwd=cwd, stdout=subprocess.DEVNULL
    ) as child:
        try:
            witness = b""
            replaced = False
            deadline = time.monotonic() + 45
            while child.poll() is None:
                if time.monotonic() >= deadline:
                    raise TimeoutError("waiting for finite gallery frames")
                if not select.select([stream], [], [], .25)[0]:
                    continue
                chunk = stream.read(16384)
                if replaced:
                    continue  # Drain the metrics FIFO through final submission.
                witness += chunk or b""
                if b"\ncpu," in witness:
                    os.kill(child.pid, signal.SIGSTOP)
                    _, status = os.waitpid(child.pid, os.WUNTRACED)
                    if not os.WIFSTOPPED(status) or edited.exists():
                        raise AssertionError("unable to observe an unfinished finite run")
                    replacement = fragment.with_suffix(".new")
                    shutil.copyfile(fragment.with_name("contours.frag"), replacement)
                    replacement.replace(fragment)
                    replaced = True
                    os.kill(child.pid, signal.SIGCONT)
            if child.returncode:
                raise RuntimeError(f"live-edit run exited {child.returncode}")
            if not replaced:
                raise AssertionError("finite child exited before the mid-run edit")
        finally:
            if child.poll() is None:
                child.kill()

    after_restart = stage / "restarted.png"
    run(after_restart, stage / "restarted.csv")
    if edited.read_bytes() != baseline.read_bytes() or after_restart.read_bytes() == baseline.read_bytes():
        raise AssertionError("active finite capture must stay identical; next startup must see the changed file")
    print("360 fixed ticks: mid-run asset replacement preserved PNG; next launch consumed replacement")


if __name__ == "__main__":
    executable = Path(sys.argv[1]).resolve()
    root = Path(sys.argv[2]).resolve()
    root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=root) as stage:
        exercise(executable, Path(stage))
