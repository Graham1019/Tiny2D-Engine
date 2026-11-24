#!/usr/bin/env python3
"""Run the two signed products from a fresh ZIP extraction. No checkout or build dependencies.

python3 smoke_showcase_candidate.py candidate.zip NEW_DIR
python3 smoke_showcase_candidate.py --shader-only /path/to/tiny2d_lab [NEW_DIR]
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import plistlib
import queue
import re
import shlex
import struct
import subprocess
import tempfile
import threading
import time
import zlib


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png(path):
    data = path.read_bytes()
    require(data[:8] == b"\x89PNG\r\n\x1a\n", f"missing PNG signature: {path}")
    start, compressed = 8, bytearray()
    width = height = 0
    while start < len(data):
        length = struct.unpack_from(">I", data, start)[0]
        kind = data[start + 4:start + 8]
        payload = data[start + 8:start + 8 + length]
        check = struct.unpack_from(">I", data, start + 8 + length)[0]
        require(zlib.crc32(kind + payload) == check, f"bad PNG checksum: {path}")
        if kind == b"IHDR":
            width, height = struct.unpack_from(">II", payload)
        if kind == b"IDAT":
            compressed.extend(payload)
        start += length + 12
        if kind == b"IEND":
            break
    require(width > 0 and height > 0 and start == len(data) and len(zlib.decompress(compressed)) > height,
            f"incomplete PNG: {path}")
    return {"bytes": len(data), "width": width, "height": height, "sha256": sha(path)}


class Probe:
    def __init__(self, out):
        self.out = out
        self.cwd = out / "other cwd 无源码"
        self.cwd.mkdir()
        self.log = out / "native.log"
        self.images = {}
        self.frames = []
        self.shader_results = []

    def note(self, message):
        with self.log.open("a", encoding="utf-8") as log:
            log.write(message + "\n")

    def run(self, *command, ok=True, timeout=120):
        args = list(map(str, command))
        self.note("$ " + shlex.join(args))
        result = subprocess.run(args, cwd=self.cwd, capture_output=True, text=True, timeout=timeout)
        self.note(result.stdout + result.stderr)
        require((result.returncode == 0) == ok, f"exit {result.returncode}: {shlex.join(args)}\n{result.stderr}")
        return result.stdout + result.stderr

    def image(self, path):
        self.images[path.name] = png(path)
        return self.images[path.name]["sha256"]

    def gallery(self, lab):
        work = self.out / "External Shader 工作副本"
        aurora = work / "aurora.frag"
        initial, failed, recovered, stable = [self.out / (name + ".png") for name in
                                             ("shader-initial", "shader-rejected", "shader-recovered", "shader-stable")]
        def session(steps):
            args = [str(lab), "--scene", "shaders", "--shader-workspace", str(work),
                    "--shader-control-stdin", "on", "--frames", str(len(steps)), "--size", "384x216",
                    "--inspector", "off", "--gpu-timing", "off"]
            self.note("$ " + shlex.join(args))
            child = subprocess.Popen(args, cwd=self.cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, bufsize=1)
            lines = queue.Queue()

            def pump():
                for line in child.stdout:
                    lines.put(line.rstrip("\n"))
                lines.put(None)

            reader = threading.Thread(target=pump, daemon=True)
            reader.start()
            def until(prefix):
                deadline = time.monotonic() + 30
                while time.monotonic() < deadline:
                    try:
                        line = lines.get(timeout=max(.001, deadline - time.monotonic()))
                    except queue.Empty:
                        break
                    require(line is not None, "gallery closed before " + prefix + "; see " + str(self.log))
                    self.note(line)
                    if line.startswith(prefix):
                        return line
                raise RuntimeError("gallery did not acknowledge " + prefix)

            try:
                until("shader_control_ready ")
                for frame, (verb, dest, change, version, result) in enumerate(steps, 1):
                    if change == "invalid":
                        aurora.write_text("#version 330 core\nthis is not a fragment shader\n", encoding="utf-8")
                    elif change == "contours":
                        aurora.write_bytes((work / "contours.frag").read_bytes())
                    self.note("> " + verb + " " + str(dest))
                    child.stdin.write(verb + " " + str(dest) + "\n")
                    child.stdin.flush()
                    response = until("shader_control ")
                    expected = f"shader_control frame={frame} tick=0 version={version} result={result}"
                    require(response == expected, response)
                    self.shader_results.append({"capture": dest.name, "tick": 0, "version": version,
                                                "result": result, "png_sha256": self.image(dest)})
                child.stdin.close()
                until("status=ok scene=shaders ")
                require(child.wait(timeout=10) == 0, "gallery shutdown failed")
            finally:
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=8)
                reader.join(timeout=2)
                child.stdout.close()
                if not child.stdin.closed:
                    child.stdin.close()

        session([("draw", initial, None, 1, "ok"), ("reload", failed, "invalid", 1, "failed"),
                 ("reload", recovered, "contours", 2, "ok"), ("draw", stable, None, 2, "ok")])
        require(sha(initial) == sha(failed), "failed reload changed actual back-buffer pixels")
        require(sha(initial) != sha(recovered) == sha(stable), "recovery did not replace the visible shader")
        edited = sha(aurora)
        restart = self.out / "shader-edited-restart.png"
        session([("draw", restart, None, 1, "ok")])
        require(sha(aurora) == edited and sha(restart) == sha(recovered), "restart lost the external user edit")
        aurora.write_text("invalid saved user program", encoding="utf-8")
        invalid = sha(aurora)
        fallback = self.out / "shader-startup-fallback.png"
        session([("draw", fallback, None, 1, "failed")])
        require(sha(aurora) == invalid and sha(fallback) == sha(initial), "invalid startup edit lost or fallback differs")
        aurora.write_bytes((work / "contours.frag").read_bytes())
        self.note("External edits preserved across runs. Failed reload retained same-tick pixels; repaired shader changed them.")

    def replay(self, lab, examples):
        def draw(name, count, *flags):
            base = self.out / name
            self.run(lab, "--scene", "replay", "--size", "384x216", "--frames", count, "--inspector", "off",
                     "--capture", base.with_suffix(".png"), "--snapshot-out", base.with_suffix(".snap"),
                     "--metrics", base.with_suffix(".csv"), *flags)
            with base.with_suffix(".csv").open() as stream:
                rows = [row for row in csv.DictReader(stream) if row["record"] == "cpu"]
            require(len(rows) == count, name + " missing CPU trace frames")
            self.frames.append({"run": name, **{key: rows[-1][key] for key in (
                "tick", "epoch", "event_sequence", "verified_events", "state_hash", "nav_last_choice", "nav_phase",
                "branch_id", "parent_branch_id", "fork_cursor")}, "png_sha256": self.image(base.with_suffix(".png")),
                "snapshot_sha256": sha(base.with_suffix(".snap"))})
            return rows
        suffix = self.out / "recorded suffix.replay"
        rec = draw("restored", 40, "--snapshot-in", examples/"mid.snap", "--nav-script", "walk", "--record", suffix)
        played = draw("replayed", 40, "--replay", suffix)
        require([r["state_hash"] for r in rec] == [r["state_hash"] for r in played], "suffix event hashes differ")
        for key in ("png_sha256", "snapshot_sha256"):
            require(self.frames[-2][key] == self.frames[-1][key], "independent continuation differs: " + key)
        root = examples / "parent.replay"
        offer = draw("offer", 2, "--replay", root, "--seek-event", 600)
        parent = draw("parent-vitality", 2, "--replay", root, "--seek-event", 601)
        child = draw("supplied-fleet", 2, "--branch-in", examples/"fleet.branch", "--seek-event", 1)
        require(offer[-1]["nav_phase"] == "AwaitingUpgrade" and parent[-1]["nav_last_choice"] == "Vitality" and
                child[-1]["nav_last_choice"] == "Fleet" and parent[-1]["tick"] == child[-1]["tick"] == "600",
                "bundled timeline boundaries differ")
        branch = self.out/"新保存.branch"
        draw("forked", 8, "--replay", root, "--fork-at", 600, "--nav-choice", "last",
             "--nav-script", "walk", "--branch-id", 112, "--branch-out", branch)
        draw("fork-replay", 8, "--branch-in", branch)
        require(self.frames[-1]["snapshot_sha256"] == self.frames[-2]["snapshot_sha256"] and
                self.frames[-1]["png_sha256"] == self.frames[-2]["png_sha256"], "new branch replay differs")
        blocked = examples/"forbidden.snap"
        result = self.run(lab, "--scene", "replay", "--frames", 1, "--snapshot-out", blocked, ok=False)
        require("outside the application" in result and not blocked.exists(), "package output protection failed")

    def candidate(self, archive):
        digest = sha(archive)
        destination = self.out / "隔离 安装"
        self.run("/usr/bin/ditto", "-x", "-k", archive, destination)
        apps = list(destination.rglob("Tiny2D Showcase.app"))
        require(len(apps) == 1, "expected one extracted Showcase")
        showcase = apps[0]
        survivor = showcase.parent/"Tiny2D Survivor.app"
        require(survivor.is_dir(), "sibling Survivor app missing")
        def entry(app):
            with (app/"Contents/Info.plist").open("rb") as f:
                return app/"Contents/MacOS"/plistlib.load(f)["CFBundleExecutable"]
        lab = showcase/"Contents/MacOS/tiny2d_lab"
        examples = showcase/"Contents/Resources/assets/examples"
        def sealed():
            state = {}
            for app in (showcase, survivor):
                self.run("/usr/bin/codesign", "--verify", "--deep", "--strict", "--verbose=2", app)
                for path in sorted(app.rglob("*")):
                    key = str(path.relative_to(showcase.parent))
                    if path.is_symlink():
                        state[key] = "link:" + os.readlink(path)
                    elif path.is_file():
                        state[key] = sha(path)
            return state
        before = sealed()
        launcher = self.out/"launch-services.png"
        self.run("/usr/bin/open", "-n", "-W", showcase, "--args", "--frames", 4, "--capture", launcher)
        self.image(launcher)
        classic = self.out/"standalone-survivor.png"
        self.run(entry(survivor), "--frames", 8, "--presentation", "classic", "--capture", classic)
        self.image(classic)
        visits = ["neon","particles","shaders","map","navigation","replay","survivor-classic","survivor-enhanced","neon"]
        output = self.out/"Tour renders"
        returned = self.out/"launcher-return.png"
        transcript = self.run(entry(showcase), "--showcase-tour", ",".join(visits), "--child-frames", 8,
                              "--showcase-output", output, "--shader-workspace", self.out/"Tour shader work",
                              "--capture", returned, timeout=180)
        require(re.findall(r"^showcase_launch exhibit=([\w-]+)", transcript, re.M) == visits and
                re.findall(r"^showcase_exit exhibit=([\w-]+) code=0 signal=0", transcript, re.M) == visits and
                "launched=9 reaped=9" in transcript, "packaged exhibit launch/reap mismatch")
        self.image(returned)
        for i, exhibit in enumerate(visits, 1):
            self.image(output / f"{i}-{exhibit}.png")
        self.replay(lab, examples)
        self.gallery(lab)
        require(before == sealed(), "signed app contents changed during native exercise")
        require(digest == sha(archive), "ZIP changed during acceptance")
        return {"archive": archive.name, "zip_sha256": digest, "signed_entries_compared": len(before),
                "launch_services_entry": entry(showcase).name, "launched": len(visits), "reaped": len(visits)}


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--shader-only", action="store_true")
    args.add_argument("input", type=Path)
    args.add_argument("destination", type=Path, nargs="?")
    opts = args.parse_args()
    binary = opts.input.resolve(strict=True)
    def execute(out):
        p = Probe(out)
        archive = {}
        if opts.shader_only:
            p.gallery(binary)
        else:
            archive = p.candidate(binary)
        report = {**archive, "shader_frames": p.shader_results, "boundaries": p.frames, "captures": p.images}
        (out/"smoke.json").write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n", encoding="utf-8")
        print(f"Native smoke passed: {len(p.images)} PNGs, {len(p.shader_results)} controlled shader frames, "
              f"{len(p.frames)} snapshot/replay runs")
    if opts.destination:
        directory = opts.destination.resolve()
        directory.mkdir(parents=True, exist_ok=False)
        execute(directory)
    else:
        require(opts.shader_only, "candidate acceptance requires a new evidence directory")
        with tempfile.TemporaryDirectory() as directory:
            execute(Path(directory))


if __name__ == "__main__":
    main()
