#!/usr/bin/env python3
"""Record and verify the M10.1 CPU fixtures in separate processes."""
import argparse
import gzip
from pathlib import Path
import struct
import subprocess
import tempfile


def run(tool, *args):
    result = subprocess.run([tool, *map(str, args)], text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"{args[0]} failed ({result.returncode})\n{result.stdout}{result.stderr}")
    return result.stdout


def checksum(data):
    h = 14695981039346656037
    for value in data:
        h = ((h ^ value) * 1099511628211) & 0xffffffffffffffff
    return h


def damaged(tool, out, temp):
    original = (out / "A/base.snap").read_bytes()
    reports = []

    def inject(name, modify, repair=False, replay=False):
        source = (out / "A/events.replay").read_bytes() if replay else original
        broken = bytearray(source)
        modify(broken)
        if repair:
            struct.pack_into("<Q", broken, 16, checksum(broken[24:]))
        path = temp / name
        path.write_bytes(broken)
        reports.append(run(tool, "reject", "replay" if replay else "snapshot", path))

    inject("truncated.snap", lambda b: b.pop())
    inject("version.snap", lambda b: struct.pack_into("<I", b, 4, 77))
    inject("length.snap", lambda b: struct.pack_into("<I", b, 12, 0xffffffff))
    inject("checksum.snap", lambda b: b.__setitem__(-1, b[-1] ^ 1))

    def bad_tick(data):
        tiles = struct.unpack_from("<I", data, 24 + 24)[0]
        # Seed + parameters + count + tiles + spawn/exit + fingerprint + epoch.
        struct.pack_into("<Q", data, 24 + 28 + tiles + 16 + 8 + 8, 3601)

    def bad_event_hash(data):
        base = struct.unpack_from("<I", data, 24)[0]
        # A starts with three 51-byte AdvanceTicks. Hash is the last u64.
        pos = 24 + 4 + base + 4 + 2 * 51 + 43
        data[pos] ^= 1

    inject("semantic-tick.snap", bad_tick, repair=True)
    inject("expected-hash.replay", bad_event_hash, repair=True, replay=True)
    return "".join(reports)


def prove(tool, out, temp):
    summaries = []
    for scenario in ("A", "B", "C"):
        directory = out / scenario
        directory.mkdir()
        trace = temp / (scenario + ".canonical")
        summaries.append(f"[{scenario}]\n")
        summaries.append(run(tool, "record", scenario, directory, trace))
        replay = directory / "events.replay"
        log = run(tool, "verify-trace", replay, trace, directory / "base.snap", directory / "final.snap")
        if not all(row.endswith(",1") for row in log.splitlines()[1:]):
            raise AssertionError("canonical mismatch")
        (directory / "verification.csv.gz").write_bytes(gzip.compress(log.encode(), mtime=0))
        summaries.append(run(tool, "verify", replay))
        summaries.append(f"full_snapshot_comparisons={len(log.splitlines())-1} final_bytes_match=1\n")

    directory = out / "B"
    for file, fields in (
        ("base.snap", ["tick=600", "phase=AwaitingUpgrade", "hp=5/5", "offer=1"]),
        ("chosen.snap", ["tick=600", "phase=Playing", "manual_paused=1", "hp=6/6", "offer=0",
                         "last_choice=Vitality"]),
    ):
        restored = run(tool, "snapshot", directory / file)
        if any(field not in restored.split() for field in fields):
            raise AssertionError(restored)
        summaries.append(f"restored {file} {restored}")
    diagnostics = damaged(tool, out, temp)
    (out / "rejections.log").write_text(diagnostics)
    (out / "cross-process.log").write_text("".join(summaries))
    print("".join(summaries) + diagnostics, end="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", required=True, type=Path)
    parser.add_argument("--out", type=Path, help="new directory for reproducible evidence")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="m101-replay-") as scratch:
        temp = Path(scratch)
        out = args.out or (temp / "evidence")
        out.mkdir(parents=True, exist_ok=False)
        prove(args.tool.resolve(), out.resolve(), temp)


if __name__ == "__main__":
    main()
