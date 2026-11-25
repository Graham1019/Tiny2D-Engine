#!/usr/bin/env python3
"""Run the two signed products from a fresh ZIP extraction. No checkout or build dependencies.

python3 smoke_showcase_candidate.py candidate.zip NEW_DIR
python3 smoke_showcase_candidate.py --shader-only /path/to/tiny2d_lab [NEW_DIR]
"""
import argparse
from contextlib import contextmanager
import csv
import hashlib
import json
import os
from pathlib import Path
import plistlib
import queue
import re
import shlex
import stat
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
    def __init__(self, out, log_name="native.log"):
        self.out = out
        self.cwd = out / "other cwd 无源码"
        self.cwd.mkdir()
        self.log = out / log_name
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

    @contextmanager
    def controlled_shader(self, lab, work, frames, *extra):
        args = [str(lab), "--scene", "shaders", "--shader-workspace", str(work),
                "--shader-control-stdin", "on", "--frames", str(frames), "--size", "384x216",
                "--inspector", "off", "--gpu-timing", "off", *map(str, extra)]
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
        def until(prefix, diagnostics=None):
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
                if diagnostics is not None:
                    diagnostics.append(line)
            raise RuntimeError("gallery did not acknowledge " + prefix)

        try:
            until("shader_control_ready ")
            yield child, until
        finally:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=8)
            reader.join(timeout=2)
            child.stdout.close()
            if not child.stdin.closed:
                child.stdin.close()

    def gallery(self, lab):
        work = self.out / "External Shader 工作副本"
        aurora = work / "aurora.frag"
        initial, failed, recovered, stable = [self.out / (name + ".png") for name in
                                             ("shader-initial", "shader-rejected", "shader-recovered", "shader-stable")]
        def session(steps):
            with self.controlled_shader(lab, work, len(steps)) as (child, until):
                for frame, (verb, dest, change, version, result) in enumerate(steps, 1):
                    if change == "invalid":
                        aurora.write_text("#version 330 core\nthis is not a fragment shader\n", encoding="utf-8")
                    elif change == "oversize":
                        aurora.write_bytes(b" " * (1024 * 1024 + 1))
                    elif change == "fifo":
                        aurora.unlink()
                        os.mkfifo(aurora)  # No writer. A blocking read cannot complete the frame.
                    elif change == "contours":
                        aurora.unlink()
                        aurora.write_bytes((work / "contours.frag").read_bytes())
                    self.note("> " + verb + " " + str(dest))
                    child.stdin.write(verb + " " + str(dest) + "\n")
                    child.stdin.flush()
                    diagnostics = []
                    response = until("shader_control ", diagnostics)
                    expected = f"shader_control frame={frame} tick=0 version={version} result={result}"
                    require(response == expected, response)
                    if change in ("fifo", "oversize"):
                        reason = "expected regular file" if change == "fifo" else "byte budget"
                        require(any(str(aurora) in line and reason in line for line in diagnostics),
                                f"{change} shader did not report a bounded-file diagnostic: {diagnostics}")
                    self.shader_results.append({"capture": dest.name, "tick": 0, "version": version,
                                                "result": result, "diagnostics": diagnostics, "png_sha256": self.image(dest)})
                child.stdin.close()
                until("status=ok scene=shaders ")
                require(child.wait(timeout=10) == 0, "gallery shutdown failed")

        oversize = self.out / "shader-oversize.png"
        fifo = self.out / "shader-fifo.png"
        session([("draw", initial, None, 1, "ok"), ("reload", failed, "invalid", 1, "failed"),
                 ("reload", oversize, "oversize", 1, "failed"), ("reload", fifo, "fifo", 1, "failed"),
                 ("reload", recovered, "contours", 2, "ok"), ("draw", stable, None, 2, "ok")])
        require(sha(initial) == sha(failed) == sha(oversize) == sha(fifo), "rejected reload changed back-buffer pixels")
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
        aurora.write_bytes(b" " * (1024 * 1024 + 1))
        oversized_edit = sha(aurora)
        fallback_size = self.out / "shader-startup-oversize.png"
        session([("draw", fallback_size, None, 1, "failed")])
        require(sha(aurora) == oversized_edit and sha(fallback_size) == sha(initial), "oversize startup fallback differs")
        aurora.unlink()
        os.mkfifo(aurora)
        fallback_fifo = self.out / "shader-startup-fifo.png"
        session([("draw", fallback_fifo, None, 1, "failed")])
        require(aurora.is_fifo() and sha(fallback_fifo) == sha(initial), "FIFO startup fallback replaced the edit or pixels")
        aurora.unlink()
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

    def signed_state(self, apps, manifest, phase):
        root = apps[0].parent
        self.note(f"Checking both strict signatures and the complete distribution inventory {phase}")
        for app in apps:
            self.run("/usr/bin/codesign", "--verify", "--deep", "--strict", "--verbose=2", app)
        files = {}
        modes = {}
        for path in sorted(root.rglob("*")):
            rel = path.relative_to(root).as_posix()
            modes[rel] = path.lstat().st_mode
            if path.is_symlink():
                files[rel] = {"link": os.readlink(path)}
            elif stat.S_ISREG(modes[rel]):
                files[rel] = {"bytes": path.stat().st_size, "sha256": sha(path)}
            else:
                require(stat.S_ISDIR(modes[rel]), "unexpected special file: " + rel)
        manifest_digest = files.pop("manifest.json")["sha256"]
        require(files == manifest["files"], "distribution contents differ from manifest " + phase)
        self.note(f"Verified {len(files)} manifest entries; manifest SHA-256 {manifest_digest}")
        # The manifest pins each file and link. Compare directory names and entry modes as well.
        return modes, manifest_digest

    def output_protection(self, lab, survivor_exe, showcase, survivor):
        cases = []
        for name, exe, own, other, flags in (
                ("Survivor", survivor_exe, survivor, showcase, ("capture", "trace", "metrics")),
                ("Lab", lab, showcase, survivor, ("capture", "metrics"))):
            own_resources = own / "Contents/Resources"
            other_resources = other / "Contents/Resources"
            alias = self.cwd / (name + " 目录链接")
            alias.symlink_to(own_resources, target_is_directory=True)
            original = other_resources / "assets/shaders/triangle.vert"
            hardlink = self.cwd / (name + " 硬链接")
            os.link(original, hardlink)
            original_hash = sha(original)
            partial = self.cwd / (name + " 未完成输出")
            cwd_contents = set(self.cwd.iterdir())
            for flag in flags:
                output_name = f"rejected-{name}-{flag}"
                for kind, target in (("own-app", own_resources / output_name),
                                     ("sibling-app", other_resources / output_name),
                                     ("linked-directory", alias / output_name),
                                     ("hardlink", hardlink)):
                    label = f"{name} --{flag} {kind}"
                    companion = "--metrics" if flag == "capture" else "--capture"
                    result = self.run(exe, "--frames", 2, companion, partial, "--" + flag, target, ok=False)
                    require(target.name in result, "missing target in diagnostic: " + label)
                    if kind == "hardlink":
                        require(re.search(r"new|exist|overwrite", result, re.I), "missing no-clobber diagnostic: " + label)
                        require(hardlink.samefile(original) and sha(hardlink) == original_hash,
                                "resource alias changed: " + label)
                    else:
                        require("outside the application" in result and not os.path.lexists(target),
                                "bundle output was not rejected: " + label)
                    require(set(self.cwd.iterdir()) == cwd_contents, "partial output left behind: " + label)
                    cases.append(label)
                    self.note("PASS " + label + " (no partial output)")
            hardlink.unlink()
            alias.unlink()
        # Pause at the native Gallery's stdin barrier after CLI preflight but before
        # the last frame can finish. Introduce an existing resource at publication time.
        original = survivor / "Contents/Resources/assets/shaders/triangle.vert"
        original_hash = sha(original)
        occupied = self.cwd / "late metrics.csv"
        drawn = self.out / "late-link frame.png"
        previous_entries = set(self.cwd.iterdir())
        with self.controlled_shader(lab, self.out / "late-link workspace", 1, "--metrics", occupied) as (child, until):
            require(not os.path.lexists(occupied), "packaged metrics published before the run succeeded")
            os.link(original, occupied)
            self.note(f"Added hard link to {original} at {occupied} after shader_control_ready")
            child.stdin.write("draw " + str(drawn) + "\n")
            child.stdin.close()
            diagnostic = until("Tiny2D Lab:")
            require(occupied.name in diagnostic and re.search(r"new|exist|overwrite", diagnostic, re.I),
                    "missing late collision diagnostic: " + diagnostic)
            require(child.wait(timeout=10) == 1, "late metrics collision must fail the process")
        png(drawn)  # This independent stdin draw completed before the CLI metrics collision.
        require(occupied.samefile(original) and sha(occupied) == original_hash, "late hardlink resource changed")
        occupied.unlink()
        require(set(self.cwd.iterdir()) == previous_entries, "late collision left partial metrics or staging files")
        cases.append("Lab --metrics hardlink after CLI preflight")
        self.note("PASS " + cases[-1] + " (resource unchanged, staging cleaned)")
        return cases

    def relative_outputs(self, lab, survivor_exe):
        for name, exe in (("Survivor", survivor_exe), ("Lab", lab)):
            directory = Path(name + " 外部输出")
            (self.cwd / directory).mkdir()
            capture, metrics = directory / "画面 capture.png", directory / "指标 metrics.csv"
            args = [exe, "--frames", 2, "--capture", capture, "--metrics", metrics]
            trace = directory / "轨迹 trace.csv"
            if name == "Survivor":
                args.extend(("--trace", trace))
            self.run(*args)
            png(self.cwd / capture)
            with (self.cwd / metrics).open() as stream:
                rows = [row for row in csv.DictReader(stream) if row.get("record", "cpu") == "cpu"]
            require([r["frame"] for r in rows] == ["1", "2"], name + " incomplete relative metrics")
            if name == "Survivor":
                with (self.cwd / trace).open() as stream:
                    require([row["tick"] for row in csv.DictReader(stream)] == ["1", "2"], "incomplete relative trace")
            self.note("PASS " + name + " relative external PNG and CSV output(s) in path with spaces and Chinese")

    def candidate(self, archive):
        digest = sha(archive)
        destination = self.out / "隔离 安装"
        self.run("/usr/bin/ditto", "-x", "-k", archive, destination)
        apps = list(destination.rglob("Tiny2D Showcase.app"))
        require(len(apps) == 1, "expected one extracted Showcase")
        showcase = apps[0]
        survivor = showcase.parent/"Tiny2D Survivor.app"
        require(survivor.is_dir(), "sibling Survivor app missing")
        manifest = json.loads((showcase.parent/"manifest.json").read_text(encoding="utf-8"))
        revision = manifest["source_commit"]
        require(re.fullmatch(r"[0-9a-f]{40}", revision) and int(revision, 16) != 0,
                "candidate must identify a real source revision")
        def entry(app):
            with (app/"Contents/Info.plist").open("rb") as f:
                return app/"Contents/MacOS"/plistlib.load(f)["CFBundleExecutable"]
        lab = showcase/"Contents/MacOS/tiny2d_lab"
        examples = showcase/"Contents/Resources/assets/examples"
        guard_out = self.out / "output protection"
        guard_out.mkdir()
        guard = Probe(guard_out, "signature-protection.log")
        guard.note(f"Archive {archive.name} SHA-256 {digest}; source {revision}; fresh extraction {destination}")
        apps = (showcase, survivor)
        before = guard.signed_state(apps, manifest, "before negative cases")
        rejected = guard.output_protection(lab, entry(survivor), showcase, survivor)
        require(before == guard.signed_state(apps, manifest, "after negative cases"),
                "negative cases changed the distribution")
        guard.relative_outputs(lab, entry(survivor))
        launcher = self.out/"launch-services.png"
        self.run("/usr/bin/open", "-n", "-W", showcase, "--args", "--frames", 4, "--capture", launcher)
        self.image(launcher)
        classic = self.out/"standalone-survivor.png"
        self.run(entry(survivor), "--frames", 8, "--presentation", "classic", "--capture", classic)
        self.image(classic)
        visits = ["neon","particles","shaders","map","navigation","replay","survivor-classic","survivor-enhanced","neon"]
        output = self.out/"Tour renders"
        tour_workspace = output/"Tour shader work"
        returned = self.out/"launcher-return.png"
        transcript = self.run(entry(showcase), "--showcase-tour", ",".join(visits), "--child-frames", 8,
                              "--showcase-output", output, "--shader-workspace", tour_workspace,
                              "--capture", returned, timeout=180)
        require(re.findall(r"^showcase_launch exhibit=([\w-]+)", transcript, re.M) == visits and
                re.findall(r"^showcase_exit exhibit=([\w-]+) code=0 signal=0", transcript, re.M) == visits and
                "launched=9 reaped=9" in transcript, "packaged exhibit launch/reap mismatch")
        self.image(returned)
        for i, exhibit in enumerate(visits, 1):
            self.image(output / f"{i}-{exhibit}.png")
        defaults = showcase/"Contents/Resources/assets/shaders/gallery"
        require((tour_workspace/".tiny2d-gallery-v1").is_file() and
                all(sha(tour_workspace/path.name) == sha(path) for path in defaults.iterdir() if path.is_file()),
                "tour did not publish its external shader workspace alongside exhibit captures")
        self.replay(lab, examples)
        self.gallery(lab)
        require(before == guard.signed_state(apps, manifest, "after positive cases and exhibit tour"),
                "native exercise changed the distribution")
        require(digest == sha(archive), "ZIP changed during acceptance")
        return {"archive": archive.name, "zip_sha256": digest, "source_commit": revision,
                "signed_entries_compared": sum(rel.startswith(tuple(app.name + "/" for app in apps))
                                               for rel in manifest["files"]),
                "manifest_entries_compared": len(manifest["files"]), "manifest_sha256": before[1],
                "rejected_outputs": rejected, "relative_external_outputs": ["Survivor PNG/trace/metrics", "Lab PNG/metrics"],
                "signature_log": str(guard.log.relative_to(self.out)),
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
