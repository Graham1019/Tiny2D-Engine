#!/usr/bin/env python3
"""Build a commit-isolated arm64 RC, sign both bundles, archive and verify a separate extraction."""
import argparse
import datetime
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile

# Importing the verifier must not create an untracked scripts/__pycache__.
sys.dont_write_bytecode = True
from verify_showcase_macos import (APPS, CANDIDATE, MANIFEST, PRODUCT, SHOWCASE, SURVIVOR,
                                  check_bundle, extract, inventory, output, require, sha, source_assets, verify)

README = """Tiny2D Showcase 0.6.0-rc1

Keep both applications together in this folder. Open Tiny2D Showcase.app for
the eight exhibits. Select an exhibit, launch it, then close its window to
return to the selector. Tiny2D Survivor.app opens the original difficulty menu.

Survivor controls
    1–5 select difficulty, Enter starts, WASD moves, Q/E zoom, arrows pan,
    R restarts after a terminal state, Esc closes the window. Enhanced mode
    adds B to toggle Bloom. The Showcase offers both presentation modes.

Snapshots, replays and branches
    Showcase resources contain assets/examples/mid.snap, parent.replay and
    fleet.branch. Use the Replay Inspector or the packaged tiny2d_lab CLI.
    Choose a writable location outside the apps for saves and shader-edit copies.

This local release candidate requires Apple Silicon and macOS 26.0 or later.
Both apps use ad-hoc signatures and have no notarization ticket. Gatekeeper
installation and native interaction require acceptance on the destination Mac.

manifest.json lists the source revision, tools, deployment settings, hashes,
architecture and dynamic-library closure. Third-party and texture notices
are included in each app's Contents/Resources folder.
"""


def run(*args, **kwargs):
    return subprocess.run(list(map(str,args)),check=True,**kwargs)


def source_revision(repo):
    """Unrelated user notes outside executable/build inputs never enter the archive."""
    inputs = ["CMakeLists.txt", "cmake", "engine", "game", "sandbox", "lab", "third_party",
              "scripts/package_showcase_macos.py", "scripts/verify_showcase_macos.py", "docs/third-party"]
    dirty = output("git","-C",str(repo),"status","--porcelain","--untracked-files=all","--",*inputs).strip()
    require(not dirty, "commit executable/packaging inputs before packaging:\n" + dirty)
    return output("git","-C",str(repo),"rev-parse","--verify","HEAD^{commit}").strip()


def prepare_build(repo, revision, work):
    source = work/"source"
    source.mkdir()
    archive = work/"source.tar"
    run("git","-C",repo,"archive","--format=tar","--output",archive,revision)
    run("tar","-xf",archive,"-C",source)
    archive.unlink()
    build = work/"build"
    # Start with a new cache and the exported commit. Package only this build's
    # executable bytes; the products and assets of any old build are ignored.
    flags = ["-DCMAKE_BUILD_TYPE=Release","-DCMAKE_OSX_ARCHITECTURES=arm64",
             "-DCMAKE_OSX_DEPLOYMENT_TARGET=26.0","-DBUILD_TESTING=OFF"]
    run("cmake","-S",source,"-B",build,*flags)
    run("cmake","--build",build,"--config","Release","--parallel","6","--target",
        "tiny2d_showcase_entry","tiny2d_lab","Tiny2DSurvivor")
    return source,build,flags


def install_app(root, name, source, build):
    app = root/name
    contents = app/"Contents"
    executables = contents/"MacOS"
    executables.mkdir(parents=True)
    exe, suffix = APPS[name]
    info = {"CFBundleName":name[:-4],"CFBundleDisplayName":name[:-4],"CFBundleExecutable":exe,
            "CFBundleIdentifier":"io.github.graham1019.tiny2d."+suffix,"CFBundlePackageType":"APPL",
            "CFBundleShortVersionString":"0.6.0","CFBundleVersion":"6001",
            "LSMinimumSystemVersion":"26.0","NSHighResolutionCapable":True}
    with (contents/"Info.plist").open("xb") as stream:
        plistlib.dump(info, stream)
    if name == SHOWCASE:
        shutil.copy2(build/"Tiny2DShowcase",executables/exe)
        shutil.copy2(build/"lab/tiny2d_lab",executables/"tiny2d_lab")
    else:
        shutil.copy2(build/"Tiny2DSurvivor.app/Contents/MacOS/Tiny2DSurvivor",executables/exe)
    with tempfile.TemporaryDirectory() as tmp:
        for relative, src in source_assets(source,name,Path(tmp)).items():
            dest = contents/"Resources"/relative
            dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(src,dest)
    # The existing macOS relocation tool discovers the adjacent Lab helper as
    # well as the primary executable and rewrites their transitive dependencies.
    run("cmake","-DAPP="+str(app),"-P",source/"cmake/FixBundle.cmake")
    for binary in sorted(contents.rglob("*.dylib")):
        if not binary.is_symlink():
            run("codesign","--force","--sign","-","--timestamp=none",binary)
    for helper in sorted(executables.iterdir()):
        if helper.name != exe:
            run("codesign","--force","--sign","-","--timestamp=none",helper)
    run("codesign","--force","--sign","-","--timestamp=none",app)
    return check_bundle(root,name)


def assemble(source, build, revision, flags, stage):
    product = stage/PRODUCT
    product.mkdir()
    metadata = {}
    for name in APPS:
        metadata.update(install_app(product,name,source,build))
    (product/"README.txt").write_text(README,encoding="utf-8")
    manifest = {
        "format":1,"candidate":CANDIDATE,"source_commit":revision,
        "built_utc":datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "target":{"architecture":"arm64","minimum_macos":"26.0","configuration":"Release",
                  "cmake_options":flags,"source":"git archive of source_commit into a new build directory"},
        "environment":{"macos":output("sw_vers","-productVersion").strip(),"build":output("sw_vers","-buildVersion").strip(),
                       "sdk":output("xcrun","--sdk","macosx","--show-sdk-version").strip(),
                       "compiler":output("xcrun","clang++","--version").splitlines()[0],
                       "cmake":output("cmake","--version").splitlines()[0]},
        "distribution":{"signature":"ad-hoc","notarization":"none","publication":"local candidate"},
        "mach_o":metadata,"files":inventory(product),
    }
    (product/MANIFEST).write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    verify(product,source)
    archive = stage/(PRODUCT+".zip")
    run("ditto","-c","-k","--keepParent","--norsrc","--noextattr",product,archive)
    digest = sha(archive)
    (stage/"SHA256SUMS").write_text(digest+"  "+archive.name+"\n")
    extracted = extract(archive,stage/"independent extraction")
    verify(extracted,source)
    require(sha(archive)==digest,"archive changed during validation")
    print(f"SOURCE_COMMIT={revision}\nDISTRIBUTION={product}\nARCHIVE={archive}\n"
          f"ARCHIVE_SHA256={digest}\nEXTRACTION={extracted}",flush=True)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_parent",type=Path,help="parent for a new, isolated temporary source and Release build")
    parser.add_argument("dist_parent",type=Path,help="parent for a new candidate directory")
    args = parser.parse_args()
    require(sys.platform=="darwin" and os.uname().machine=="arm64","requires an Apple Silicon build host")
    repo = Path(__file__).resolve().parents[1]
    revision = source_revision(repo)
    print("Building committed source " + revision,flush=True)
    build_parent,dist_parent = args.build_parent.resolve(),args.dist_parent.resolve()
    for path in (build_parent,dist_parent):
        path.mkdir(parents=True,exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix="candidate-",dir=dist_parent))
    with tempfile.TemporaryDirectory(prefix="showcase-build-",dir=build_parent) as scratch:
        source,build,flags = prepare_build(repo,revision,Path(scratch))
        assemble(source,build,revision,flags,stage)


if __name__ == "__main__":
    main()
