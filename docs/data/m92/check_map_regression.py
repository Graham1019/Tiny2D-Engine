"""Compare M9.1 and current MapSession movement records, including float bits."""
import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
BASE = "6ccc811d8255e383fc80942b374fd5ce0f9d2279"
SOURCES = ["engine/src/Math/Vector2.cpp", *(
    f"engine/src/World/{name}.cpp" for name in ("TileMap", "MapGenerator", "TileCollision", "MapSession")
)]
HEADERS = [f"engine/include/tiny2d/{name}.h" for name in (
    "Vector2", "TileMap", "MapGenerator", "TileCollision", "MapSession", "FixedTimestep"
)]
DRIVER = r"""
#include "tiny2d/MapSession.h"
#include <cstdint>
#include <cstring>
#include <iostream>
using namespace tiny2d;
std::uint32_t bits(float x) {
    std::uint32_t b;
    static_assert(sizeof(b) == sizeof(x));
    std::memcpy(&b,&x,sizeof(b));
    return b;
}
int main() {
    MapSession world;
    for (auto shape : {TileCoord{17,25}, {64,40}, {128,96}}) {
        for (auto seed : {0u,71u,UINT32_MAX}) {
            world.regenerate(seed,{shape.x,shape.y});
            for (unsigned i=0; i<3600; ++i) {
                world.fixedUpdate(mapWalkInput(world.tick()));
                const auto& move = world.collision();
                std::cout << shape.x << ' ' << shape.y << ' ' << world.seed() << ' ' << world.epoch()
                          << ' ' << world.tick() << ' ' << bits(move.position.x) << ' ' << bits(move.position.y)
                          << ' ' << move.hitX << ' ' << move.hitY << ' ' << move.tilesQueried << '\n';
            }
        }
    }
}
"""


def trace(source, driver, output):
    subprocess.run(["c++", "-std=c++17", "-O2", "-Iengine/include", *SOURCES, driver, "-o", output],
                   cwd=source, check=True)
    return subprocess.check_output([output])


def main():
    with tempfile.TemporaryDirectory() as name:
        work = Path(name)
        base = work / "base"
        for relative in SOURCES + HEADERS:
            f = base / relative
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_bytes(subprocess.check_output(["git", "show", f"{BASE}:{relative}"], cwd=ROOT))
        driver = work / "driver.cpp"
        driver.write_text(DRIVER)
        accepted = trace(base, driver, work / "accepted")
        current = trace(ROOT, driver, work / "current")
        if accepted != current:
            raise ValueError("MapSession traces differ")
        count = len(current.splitlines())
        assert count == 32400
        print("base", BASE)
        print("compiler", subprocess.check_output(["c++", "--version"], text=True).splitlines()[0])
        print("sizes 17x25 64x40 128x96; seeds 0 71 4294967295; ticks per world 3600")
        print("compared dimensions, seed, epoch, tick, exact x/y float bits, hitX/hitY and tilesQueried")
        print("identical records", count)
        print("trace SHA256", hashlib.sha256(current).hexdigest())


if __name__ == "__main__":
    main()
