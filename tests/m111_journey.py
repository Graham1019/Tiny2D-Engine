"""Exercise real Showcase child processes, specialist controls and Survivor feedback.

Run with a native GL build. --evidence NEW_DIR retains screenshots and compact
verification records. Without it the entire run is confined to a temporary directory.
"""
import argparse
import csv
import gzip
import hashlib
import io
import math
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import tempfile
import zlib


EXHIBITS = ("neon", "particles", "shaders", "map", "navigation", "replay",
            "survivor-classic", "survivor-enhanced")
CPU_FIELDS = ("scene", "seed", "epoch", "tick", "width", "height", "view", "lighting", "shadows",
              "particle_capacity", "active_particles", "active_instances", "particle_draw_calls",
              "gallery_effect", "shader_version", "map_width", "map_height", "nav_phase", "nav_hp",
              "nav_last_choice", "nav_player_speed", "event_sequence", "recorded_events", "verified_events",
              "branch_id", "parent_branch_id", "fork_cursor", "state_hash", "checkpoint_count", "replay_mode")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def png_info(path):
    data = path.read_bytes()
    require(data.startswith(b"\x89PNG\r\n\x1a\n"), f"invalid PNG signature: {path}")
    pos, compressed = 8, bytearray()
    width = height = 0
    while pos < len(data):
        length, kind = struct.unpack_from(">I4s", data, pos)
        payload = data[pos + 8:pos + 8 + length]
        crc, = struct.unpack_from(">I", data, pos + 8 + length)
        require(crc == zlib.crc32(kind + payload), f"PNG checksum mismatch: {path}")
        if kind == b"IHDR":
            width, height = struct.unpack_from(">II", payload)
        if kind == b"IDAT":
            compressed.extend(payload)
        pos += 12 + length
        if kind == b"IEND":
            break
    require(pos == len(data) and width > 0 and height > 0 and len(zlib.decompress(compressed)) > height,
            f"incomplete framebuffer capture: {path}")
    return width, height, sha(data)


def read_rows(path):
    with path.open(newline="") as source:
        return list(csv.DictReader(source))


def cpu_frame(path):
    rows = [row for row in read_rows(path) if row["record"] == "cpu"]
    require(rows, f"no CPU/display rows in {path}")
    return rows[-1]


def write_table(path, columns, rows):
    with path.open("w", newline="") as fp:
        out = csv.DictWriter(fp, fieldnames=columns, extrasaction="ignore", lineterminator="\n")
        out.writeheader()
        out.writerows(rows)


def quantiles(directory):
    # Identical pre-terminal gameplay window. These CPU spans exclude swap and file I/O.
    reports = []
    for path in sorted(directory.glob("level-*-*.metrics.csv.gz")):
        with gzip.open(path, "rt", newline="") as fp:
            rows = [r for r in csv.DictReader(fp) if 121 <= int(r["frame"]) <= 300]
        require(len(rows) == 180 and all(r["frame"] == r["tick"] for r in rows),
                f"incomplete matched gameplay window: {path.name}")
        for key in ("fixed_us", "prepare_us", "world_us", "ui_us", "draw_us", "extract_us", "horizontal_us",
                    "vertical_us", "output_us", "world_draws", "ui_draws", "fullscreen_draws", "hdr_bytes"):
            values = sorted(float(r[key]) for r in rows)
            reports.append({"difficulty": path.name.split("-")[1], "mode": rows[0]["mode"], "metric": key,
                            "samples": len(rows), **{f"p{q}": values[math.ceil(len(rows)*q/100)-1]
                                                    for q in (50,95,99)}})
    require(len(reports) == 4 * 13, "expected two difficulties and two presentations")
    return reports


class Journey:
    def __init__(self, build, work, evidence):
        self.build = build
        self.lab = build / "lab/tiny2d_lab"
        self.survivor = build / "Tiny2DSurvivor.app/Contents/MacOS/Tiny2DSurvivor"
        self.work = work
        self.media = evidence / "media"
        self.data = evidence / "data"
        self.media.mkdir(parents=True)
        self.data.mkdir(parents=True)
        self.frames = []
        self.captures = []
        self.log = []

    def invoke(self, *argv):
        cmd = list(map(str, argv))
        result = subprocess.run(cmd, cwd=self.work, text=True, capture_output=True, timeout=150)
        self.log.append("$ " + shlex.join(cmd) + "\n" + result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"exit {result.returncode}: {shlex.join(cmd)}\n{result.stdout[-4000:]}\n{result.stderr}")
        return result.stdout

    def image(self, source, name):
        width, height, fingerprint = png_info(source)
        if source != self.media / (name + ".png"):
            shutil.copyfile(source, self.media / (name + ".png"))
        self.captures.append({"name": name, "width": width, "height": height, "bytes": source.stat().st_size,
                              "sha256": fingerprint})
        return fingerprint

    def frame(self, name, path):
        row = cpu_frame(path)
        self.frames.append({"run": name, **{key: row.get(key, "") for key in CPU_FIELDS}})
        return row

    def native_tour(self):
        visits = (*EXHIBITS, "neon", "survivor-enhanced")
        children = self.work / "children"
        gallery = self.work / "showcase.png"
        output = self.invoke(self.lab, "--scene", "showcase", "--showcase-tour", ",".join(visits),
                             "--child-frames", 24, "--showcase-output", children, "--capture", gallery)
        launches = re.findall(r"showcase_launch exhibit=([\w-]+) pid=(\d+)", output)
        exits = re.findall(r"showcase_exit exhibit=([\w-]+) code=(\d+)", output)
        require([e for e, _ in launches] == list(visits) and len({p for _, p in launches}) == len(visits),
                f"unexpected native child process starts: {launches}")
        require(exits == [(e, "0") for e in visits], f"native child exits: {exits}")
        require(f"launched={len(visits)} reaped={len(visits)}" in output, "Showcase did not reap every child")
        self.image(gallery, "showcase-return")
        launcher = self.work / "launcher.png"
        self.invoke(self.lab, "--scene", "showcase", "--showcase-exhibit", "neon", "--frames", 4,
                    "--capture", launcher)
        self.image(launcher, "showcase")
        records = []
        for i, ((exhibit, pid), _) in enumerate(zip(launches, exits), 1):
            base = children / f"{i}-{exhibit}"
            width, height, digest = png_info(base.with_suffix(".png"))
            if exhibit in EXHIBITS[:6]:
                if i <= len(EXHIBITS):
                    self.image(base.with_suffix(".png"), exhibit)
                row = self.frame(f"tour-{i}-{exhibit}", base.with_suffix(".csv"))
                require(row["scene"] == exhibit, f"wrong running scene for {exhibit}")
                if exhibit == "particles":
                    require(int(row["active_particles"]) == int(row["active_instances"]) == 8192 and
                            int(row["particle_draw_calls"]) == 1, "8192 live particles must be drawn in one instanced call")
                if exhibit == "replay":
                    require(int(row["checkpoint_count"]) > 1, "Replay exhibit lacks its verified timeline")
            else:
                require(base.with_suffix(".trace.csv").stat().st_size and base.with_suffix(".csv").stat().st_size,
                        f"missing standalone Survivor trace/metrics for {exhibit}")
            records.append({"visit": i, "exhibit": exhibit, "pid": pid, "exit_code": 0,
                            "width": width, "height": height, "png_sha256": digest})
        require((children / "7-survivor-classic.trace.csv").read_bytes() ==
                (children / "8-survivor-enhanced.trace.csv").read_bytes(), "Showcase Survivor launch traces diverged")
        require((children / "8-survivor-enhanced.trace.csv").read_bytes() ==
                (children / "10-survivor-enhanced.trace.csv").read_bytes(), "reopened Survivor retained old state")
        write_table(self.data / "visits.csv", records[0].keys(), records)

    def lab_frame(self, scene, name, frames, args, inspector=False):
        image, metrics = self.work / (name + ".png"), self.work / (name + ".csv")
        self.invoke(self.lab, "--scene", scene, "--size", "640x360", "--frames", frames,
                    "--gpu-timing", "off", "--inspector", "on" if inspector else "off",
                    "--capture", image, "--metrics", metrics, *args)
        return self.frame(name, metrics), self.image(image, name)

    def exhibits(self):
        # Shader fixture uses the production manager with a temporary on-disk shader,
        # compiles a broken edit, asserts intact HDR pixels, then re-installs and redraws.
        shaders = self.work / "shader-recovery"
        shader_log = self.invoke(self.build / "lab/tiny2d_m82_gl", shaders)
        require("rollback=compile,link" in shader_log and "exact_HDR_restored=yes" in shader_log,
                "shader rollback/recovery checks did not execute")
        for name in ("reload-before", "reload-failed", "reload-recovered"):
            self.image(shaders / (name + ".png"), name)
        maps = []
        for seed in (71, 72):
            row, digest = self.lab_frame("map", f"map-{seed}", 24,
                                         ["--seed", seed, "--map-size", "21x13", "--map-script", "none"])
            require(row["seed"] == str(seed) and row["total_tiles"] == str(21 * 13), "map seed/size mismatch")
            maps.append(digest)
        require(maps[0] != maps[1], "different map seeds rendered identical captures")

        offer, _ = self.lab_frame("navigation", "navigation-offer", 600,
                                  ["--seed", 71, "--nav-enemies", 0, "--nav-script", "walk"], inspector=True)
        require(offer["tick"] == "600" and offer["nav_phase"] == "AwaitingUpgrade" and
                offer["nav_offer_serial"] == "1", "did not reach the live Navigation offer")
        root = self.lab.parent / "assets/showcase/timeline.replay"
        parent_bytes = root.read_bytes()
        before, _ = self.lab_frame("replay", "timeline-offer", 2,
                                   ["--replay", root, "--timeline-view", "on", "--seek-event", 600], inspector=True)
        child = self.work / "journey.branch"
        opened, _ = self.lab_frame("replay", "timeline-forked", 3,
                                   ["--replay", root, "--timeline-view", "on", "--fork-at", 600,
                                    "--branch-id", 111, "--nav-choice", "last", "--branch-out", child])
        self.invoke(self.build / "tiny2d_timeline_verify", "branch", child)
        played, _ = self.lab_frame("replay", "timeline-child", 2,
                                    ["--branch-in", child, "--timeline-view", "on", "--seek-event", 1], inspector=True)
        require(before["tick"] == played["tick"] == "600" and before["nav_phase"] == "AwaitingUpgrade" and
                played["nav_last_choice"] == "Fleet" and played["nav_player_speed"] == "132" and
                opened["parent_branch_id"] == before["branch_id"] and played["fork_cursor"] == "600" and
                played["event_sequence"] == "1" and root.read_bytes() == parent_bytes,
                "standalone Showcase timeline fork did not preserve its source and diverge at choice")

    def survivor_pair(self, difficulty):
        traces, terminal = {}, {}
        for mode in ("classic", "enhanced"):
            tag = f"level-{difficulty}-{mode}"
            trace, metrics = self.work / (tag + ".trace.csv"), self.work / (tag + ".metrics.csv")
            terminal[mode] = self.invoke(self.survivor, "--presentation", mode, "--difficulty", difficulty,
                                         "--frames", 600, "--trace", trace, "--metrics", metrics)
            traces[mode] = trace.read_bytes()
            (self.data / (tag + ".metrics.csv.gz")).write_bytes(gzip.compress(metrics.read_bytes(), mtime=0))
        require(traces["classic"] == traces["enhanced"], f"difficulty {difficulty}: authoritative traces differ")
        trace = traces["classic"]
        rows = list(csv.DictReader(io.StringIO(trace.decode())))
        require(rows and all(int(b["tick"]) == int(a["tick"])+1 for a,b in zip(rows,rows[1:])),
                f"difficulty {difficulty}: missing fixed-tick states")
        events = [(row["tick"], item) for row in rows for item in row["events"].split(";") if item]
        kinds = {}
        for name in ("ProjectileHit", "EnemyKilled", "PlayerDamaged"):
            found = [int(tick) for tick, event in events if "/" + name + "/" in event]
            require(found, f"difficulty {difficulty}: no real {name} in recording")
            kinds[name] = found
        (self.data / f"level-{difficulty}-authority.csv.gz").write_bytes(gzip.compress(trace, mtime=0))
        result = {"difficulty": difficulty, "ticks": len(rows), "final_tick": rows[-1]["tick"],
                  "hp": rows[-1]["hp"], "kills": rows[-1]["kills"], "phase": rows[-1]["phase"],
                  "events": len(events), "bytes": len(trace), "sha256_classic": sha(traces["classic"]),
                  "sha256_enhanced": sha(traces["enhanced"]), **{k: len(v) for k, v in kinds.items()}}

        legacy = self.invoke(self.build / "tiny2d_sandbox", "--scene", "survivor-demo",
                             "--difficulty", difficulty, "--frames", 600)
        def counts(output):
            end = next(line for line in output.splitlines() if line.startswith("Survivor frames="))
            return dict(re.findall(r"(\w+)=([^ ]+)", end))
        sandbox = counts(legacy)
        classic_counts = counts(terminal["classic"])
        for key in ("difficulty", "ticks", "health", "kills", "enemies", "projectiles", "spawned", "fired", "hits", "phase"):
            require(sandbox[key] == classic_counts[key], f"legacy Survivor differs at {key}")
        result["legacy_draws"] = sandbox["draws_total"]

        if difficulty == 3:
            tick = kinds["EnemyKilled"][0]
            captures = {}
            for mode, extra, name in (("classic", [], "survivor-classic"), ("enhanced", [], "survivor-enhanced"),
                                      ("enhanced", ["--bloom", "off"], "enhanced-bloom-off")):
                path = self.work / (name + ".png")
                self.invoke(self.survivor, "--presentation", mode, "--difficulty", difficulty,
                            "--frames", tick, "--capture", path, *extra)
                captures[name] = self.image(path, name)
            require(len(set(captures.values())) == 3, "same-tick classic, enhanced and Bloom-off pixels must differ")
            result["capture_tick"] = tick
        return result

    def run(self):
        self.native_tour()
        self.exhibits()
        pairs = [self.survivor_pair(d) for d in (3,5)]
        write_table(self.data / "combat-parity.csv", pairs[0].keys(), pairs)
        reports = quantiles(self.data)
        write_table(self.data / "survivor-quantiles.csv", reports[0].keys(), reports)
        write_table(self.data / "frames.csv", ("run", *CPU_FIELDS), self.frames)
        write_table(self.data / "captures.csv", self.captures[0].keys(), self.captures)
        print("Native Showcase launched and reaped all eight exhibits plus two reopenings.")
        for row in pairs:
            print(f"level={row['difficulty']} equal_ticks={row['ticks']} equal_combat_events={row['events']} "
                  f"sha256={row['sha256_classic']} classic_sandbox_draws={row['legacy_draws']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, nargs="?")
    parser.add_argument("--summarize", type=Path, help="recompute Survivor quantiles from retained raw CSVs")
    parser.add_argument("--evidence", type=Path, help="new directory to retain data and media")
    args = parser.parse_args()
    if args.summarize:
        import sys
        rows = quantiles(args.summarize)
        out = csv.DictWriter(sys.stdout, fieldnames=list(rows[0]), lineterminator="\n")
        out.writeheader()
        out.writerows(rows)
        return
    if not args.build:
        parser.error("supply a native build directory or --summarize DATA")
    with tempfile.TemporaryDirectory(prefix="tiny2d-showcase-") as tmp:
        work = Path(tmp)
        evidence = args.evidence.resolve() if args.evidence else work / "evidence"
        evidence.mkdir(exist_ok=False)
        journey = Journey(args.build.resolve(), work, evidence)
        try:
            journey.run()
        finally:
            (journey.data / "journey.log").write_text("\n".join(journey.log))


if __name__ == "__main__":
    main()
