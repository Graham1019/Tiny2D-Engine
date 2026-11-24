"""Build the same public-getter trace driver twice and compare entire boundary records."""
import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
BASE = "9bcf0c285fc4d58bf45fee60537af74b77836f14"
DRIVER = Path(__file__).with_name("navigation_trace.cpp").resolve()

def compile_driver(root, exe):
    sources = list((root / "engine/src/World").glob("*.cpp"))
    sources += [root / "engine/src/Math/Vector2.cpp"]
    for name in ["NavigationSession", "UpgradeSystem", "NavigationSnapshot"]:
        path = root / f"game/src/{name}.cpp"
        if path.exists():
            sources.append(path)
    subprocess.run(["c++", "-O2", "-std=c++17", "-I" + str(root / "engine/include"),
                    "-I" + str(root / "game/include"), *sources, DRIVER, "-o", exe], check=True)

with tempfile.TemporaryDirectory() as work:
    d = Path(work)
    tree = d / "accepted"
    tree.mkdir()
    paths = ["game/include", "engine/include", "game/src", "engine/src/World", "engine/src/Math/Vector2.cpp"]
    archive = subprocess.check_output(["git", "archive", BASE, *paths], cwd=ROOT)
    subprocess.run(["tar", "-x", "-C", tree], input=archive, check=True)
    compile_driver(tree, d/"old")
    compile_driver(ROOT, d/"new")
    digest = hashlib.sha256()
    with subprocess.Popen([d/"old"], stdout=subprocess.PIPE) as a, subprocess.Popen([d/"new"], stdout=subprocess.PIPE) as b:
        count = 0
        for count, record in enumerate(a.stdout, 1):
            if record != b.stdout.readline():
                raise ValueError(f"first differing authoritative record {count}")
            digest.update(record)
        if b.stdout.read(1) or a.wait() or b.wait():
            raise ValueError("incomplete trace")
    print("accepted", BASE, "identical complete authority boundaries", count)
    print("controls: advance, pause, choose last, resume, reset, regenerate; populations 0/16/256")
    print("ordered full-record SHA256", digest.hexdigest())
