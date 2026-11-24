#!/usr/bin/env python3
"""Verify an extracted Tiny2D Showcase release candidate or its local ZIP."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import stat
import subprocess
import tempfile
import zipfile

CANDIDATE = "0.6.0-rc1"
PRODUCT = "Tiny2D-Showcase-" + CANDIDATE + "-macos-arm64"
SHOWCASE = "Tiny2D Showcase.app"
SURVIVOR = "Tiny2D Survivor.app"
APPS = {SHOWCASE: ("Tiny2DShowcase", "showcase"), SURVIVOR: ("Tiny2DSurvivor", "survivor")}
HDR = ("fullscreen.vert", "display.frag", "bright.frag", "blur.frag", "light.frag", "compose.frag")
EXAMPLES = {"mid.snap": "docs/data/m101/cross-process/A/base.snap",
            "parent.replay": "docs/data/m102/parent.replay", "fleet.branch": "docs/data/m102/fleet.branch"}
PREVIEWS = ("neon", "particles", "shaders", "map", "navigation", "replay", "classic", "enhanced")
LICENSES = ("GLFW-LICENSE.md", "libpng-LICENSE.txt", "Dear-ImGui-LICENSE.txt", "SHOWCASE-NOTICES.txt")
MANIFEST = "manifest.json"


def require(test, message):
    if not test:
        raise ValueError(message)


def output(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inventory(root):
    files = {}
    for path in sorted(root.rglob("*")):
        rel = path.relative_to(root).as_posix()
        if rel == MANIFEST:
            continue
        if path.is_symlink():
            dest = os.readlink(path)
            app = rel.split("/", 1)[0]
            require(app in APPS and not os.path.isabs(dest) and
                    path.resolve(strict=True).is_relative_to((root / app).resolve()),
                    "link outside owning app: " + rel)
            files[rel] = {"link": dest}
        elif path.is_file():
            files[rel] = {"bytes": path.stat().st_size, "sha256": sha(path)}
        else:
            require(path.is_dir(), "unexpected special file: " + rel)
    return files


def source_assets(source, app, scratch):
    """Use the build's own exhibit-resource staging recipe, including its example replay."""
    result = {}

    def copy_tree(directory, prefix):
        for path in sorted(directory.rglob("*")):
            if path.is_file():
                result[prefix + "/" + path.relative_to(directory).as_posix()] = path

    copy_tree(source / "sandbox/assets", "assets")
    if app == SHOWCASE:
        copy_tree(source / "lab/assets", "assets")
        staged = scratch / "showcase"
        subprocess.run(["cmake", "-DSOURCE=" + str(source), "-DDEST=" + str(staged),
                        "-P", str(source / "cmake/StageShowcaseAssets.cmake")], check=True)
        copy_tree(staged, "assets/showcase")
        for name, src in EXAMPLES.items():
            result["assets/examples/" + name] = source / src
    else:
        for shader in HDR:
            result["assets/shaders/" + shader] = source / "lab/assets/shaders" / shader
    copy_tree(source / "docs/third-party", "licenses")
    result["licenses/Dear-ImGui-LICENSE.txt"] = source / "third_party/imgui/LICENSE.txt"
    return result


def inside(path, app):
    require(path.resolve(strict=True).is_relative_to(app.resolve()), "dependency escapes app: " + str(path))
    return path.resolve()


def expand(ref, binary, app):
    for token, directory in (("@loader_path", binary.parent), ("@executable_path", app / "Contents/MacOS")):
        if ref == token or ref.startswith(token + "/"):
            return directory / ref[len(token):].lstrip("/")
    raise ValueError(f"external or unanchored install name {ref} in {binary.name}")


def load_info(binary):
    load = output("otool", "-l", str(binary))
    versions = re.findall(r"^\s*minos (\d+(?:\.\d+)+)\s*$", load, re.M)
    if not versions:
        versions = re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version ([\d.]+)", load)
    require(len(versions) == 1, "missing or ambiguous minimum OS: " + binary.name)
    rpaths = re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset", load)
    arch = output("lipo", "-archs", str(binary)).strip()
    require(arch == "arm64", "expected arm64-only: " + binary.name)
    minimum = tuple(int(x) for x in versions[0].split("."))
    require((minimum + (0, 0))[:3] <= (26, 0, 0), "deployment target above 26.0: " + binary.name)
    lines = output("otool", "-L", str(binary)).splitlines()[1:]
    deps = [line.strip().split(" (compatibility version")[0] for line in lines if line.strip()]
    return {"architecture": arch, "min_macos": versions[0], "rpaths": rpaths, "load_names": deps}


def binaries(app, expected):
    contents = app / "Contents"
    real = sorted(p for p in contents.rglob("*") if p.is_file() and not p.is_symlink() and
                  (p.parent == contents/"MacOS" or p.suffix == ".dylib"))
    require({p.name for p in real if p.parent == contents/"MacOS"} == set(expected),
            "unexpected executable inventory in " + app.name)
    # Also reject executable code hidden elsewhere in the bundle.
    known = set(real)
    for p in contents.rglob("*"):
        if not p.is_file() or p.is_symlink() or p in known:
            continue
        with p.open("rb") as stream:
            header = stream.read(4)
        require(header not in (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xca\xfe\xba\xbe",
                               b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xce"),
                "unexpected Mach-O: " + str(p))
    return real


def check_bundle(root, app_name):
    app = root / app_name
    contents = app / "Contents"
    exe, suffix = APPS[app_name]
    with (contents / "Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    keys = {"CFBundleExecutable": exe, "CFBundleShortVersionString": "0.6.0", "CFBundleVersion": "6001",
            "CFBundleName": app_name[:-4], "CFBundleDisplayName": app_name[:-4],
            "CFBundleIdentifier": "io.github.graham1019.tiny2d." + suffix, "CFBundlePackageType": "APPL",
            "LSMinimumSystemVersion": "26.0", "NSHighResolutionCapable": True}
    for key, expected in keys.items():
        require(info.get(key) == expected, f"invalid {app_name} {key}")
    main = contents / "MacOS" / exe
    executables = [exe, "tiny2d_lab"] if app_name == SHOWCASE else [exe]
    resources = contents/"Resources"
    required = ["shaders/" + s for s in HDR] + ["shaders/triangle.vert", "shaders/triangle.frag",
                "textures/sprite.png", "textures/red.png", "textures/green.png", "textures/blue.png",
                "textures/LICENSE.txt", "textures/M4-LICENSE.txt"]
    if app_name == SHOWCASE:
        required += ["shaders/" + n for n in ("normal.frag", "normal.vert", "normal_display.frag",
                     "particle.frag", "particle.vert", "shadow_mask.vert", "shadow_mask.frag",
                     "shadow_light.frag", "shadow_display.frag", "gallery/screen.vert",
                     "gallery/aurora.frag", "gallery/contours.frag", "gallery/orbitals.frag")]
        required += ["showcase/" + n + ".png" for n in PREVIEWS] + ["showcase/timeline.replay"]
        required += ["examples/" + n for n in EXAMPLES]
    for name in required:
        require((resources/"assets"/name).is_file(), "missing asset: " + name)
    for name in LICENSES:
        require((resources/"licenses"/name).is_file(), "missing vendor notice: " + name)

    items = binaries(app, executables)
    data = {p: load_info(p) for p in items}
    for binary, metadata in data.items():
        if binary.parent == main.parent:
            require(os.access(binary,os.X_OK), "non-executable product: " + binary.name)
            require(metadata["min_macos"] in ("26.0", "26.0.0"), "incorrect product deployment target")
        rpaths = [inside(expand(rp, binary, app), app) for rp in metadata["rpaths"]]
        inherited = [inside(expand(rp, main, app), app) for rp in data[main]["rpaths"]]
        for ref in metadata["load_names"]:
            if ref.startswith(("/System/Library/", "/usr/lib/")) and ".." not in PurePosixPath(ref).parts:
                continue
            if ref.startswith("@rpath/"):
                paths = [p / ref[7:] for p in rpaths + inherited]
                matches = {inside(p,app) for p in paths if p.is_file()}
                require(len(matches) == 1 and matches.issubset(data), "unresolved/ambiguous " + ref)
            else:
                resolved = inside(expand(ref,binary,app),app)
                require(resolved in data, "non-Mach-O dependency: " + ref)
        output("codesign", "--verify", "--strict", str(binary))
        details = output("codesign", "-d", "--verbose=4", str(binary))
        require("Signature=adhoc" in details, "expected ad-hoc signature: " + binary.name)
    output("codesign","--verify","--deep","--strict",str(app))
    return {p.relative_to(root).as_posix(): metadata for p,metadata in data.items()}


def verify(root, source=None):
    root = root.resolve()
    with (root/MANIFEST).open() as stream:
        manifest = json.load(stream)
    require(manifest["format"] == 1 and manifest["candidate"] == CANDIDATE and
            re.fullmatch(r"[0-9a-f]{40}",manifest["source_commit"]), "invalid candidate provenance")
    require((root/"README.txt").is_file(), "missing distribution instructions")
    actual = inventory(root)
    recorded = manifest["files"]
    for name in sorted(set(actual) | set(recorded)):
        require(actual.get(name) == recorded.get(name), "manifest mismatch: " + name)
    report = {}
    for app_name in APPS:
        report.update(check_bundle(root, app_name))
        if source:
            with tempfile.TemporaryDirectory() as scratch:
                inputs = source_assets(source.resolve(), app_name, Path(scratch))
                resources = root / app_name / "Contents/Resources"
                actual_names = {p.relative_to(resources).as_posix() for p in resources.rglob("*") if p.is_file()}
                require(actual_names == set(inputs), "source resource inventory differs: " + app_name)
                for name, src in inputs.items():
                    require(sha(resources/name) == sha(src), "source bytes differ: " + name)
    require(report == manifest["mach_o"],"Mach-O metadata differs from manifest")
    print(f"verified {len(actual)} files/links, {len(report)} Mach-O binaries, two ad-hoc signatures; "
          f"source={manifest['source_commit']}")
    return manifest


def extract(archive, directory):
    # Examine links and all paths before asking ditto to restore permissions.
    links = set()
    entries = set()
    with zipfile.ZipFile(archive) as zipped:
        for member in zipped.infolist():
            name = PurePosixPath(member.filename)
            require(not name.is_absolute() and ".." not in name.parts and name.parts[0] == PRODUCT,
                    "invalid archive member: " + member.filename)
            require(name not in entries, "duplicate archive member: " + member.filename)
            entries.add(name)
            if stat.S_ISLNK(member.external_attr >> 16):
                target = zipped.read(member).decode("utf-8")
                require(len(name.parts) > 2 and name.parts[1] in APPS and not target.startswith("/") and
                        Path(os.path.normpath(str(name.parent / target))).is_relative_to(Path(*name.parts[:2])),
                        "archive symlink escapes app")
                links.add(name)
        require(not any(parent in links for name in entries for parent in name.parents), "archive uses a link as a directory")
    directory.mkdir(exist_ok=False)
    subprocess.run(["ditto","-x","-k",str(archive),str(directory)],check=True)
    return directory/PRODUCT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate",type=Path)
    parser.add_argument("--source",type=Path,help="also compare every resource with this source checkout")
    args = parser.parse_args()
    if args.candidate.is_file():
        archive = args.candidate.resolve()
        sums = archive.parent/"SHA256SUMS"
        digest = sha(archive)
        if sums.exists():
            require(sums.read_text().strip() == digest + "  " + archive.name, "archive SHA-256 mismatch")
        with tempfile.TemporaryDirectory() as tmp:
            verify(extract(archive,Path(tmp)/"extracted"),args.source)
        print("archive_sha256=" + digest)
    else:
        verify(args.candidate,args.source)


if __name__ == "__main__":
    main()
