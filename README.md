# Tiny2D Engine

**A C++17 engine for macOS OpenGL. Play an arena survivor, inspect an HDR render pass, or rewind a recorded simulation and try a different upgrade.**

Tiny2D Showcase opens eight playable exhibits. Each starts a real Lab scene or Survivor process. The launcher stays open while you play. Close the exhibit to return and choose another.

| Tiny2D Showcase | Survivor Enhanced |
|---|---|
| ![The native Showcase launcher from the extracted macOS candidate](docs/media/m112/native-launcher.png) | ![Survivor Enhanced rendering the first enemy kill at simulation tick 87](docs/media/m111/survivor-enhanced.png) |

[Candidate and Mac acceptance guide](docs/releases/v0.6.0-rc1.md) · [Recorded demo and storyboard](docs/media/m112/storyboard.md) · [Architecture](docs/project-architecture-overview.md) · [Engineering interview guide](docs/interview-walkthrough.md)

## Run the local candidate

Build or obtain the candidate ZIP from its owner, verify its checksum, and extract it. Keep **Tiny2D Showcase.app** and **Tiny2D Survivor.app** beside one another. Open Showcase and select **Open exhibit**. Launching Survivor directly opens the Classic difficulty menu. Choose **Survivor Enhanced** in Showcase to see the same game with HDR feedback and Bloom.

From the directory containing the ZIP and its SHA256SUMS file, the following commands verify the archive and open its app pair from a fresh location.

```sh
shasum -a 256 -c SHA256SUMS
OUT=$(mktemp -d)
ditto -x -k Tiny2D-Showcase-0.6.0-rc1-macos-arm64.zip "$OUT"
open "$OUT/Tiny2D-Showcase-0.6.0-rc1-macos-arm64/Tiny2D Showcase.app"
```

The candidate targets **arm64, macOS 26.0 or later**. The validation host ran macOS 26.7.3 on Apple M5 Pro. The ad-hoc signed candidate passed a native test from a separate extraction. It launched and reaped nine children covering all eight exhibits and one reopen. Notarization, third-party machines, other macOS versions and the primary Mac's Finder/quarantine behavior have separate acceptance entries. The owner retains the decision on publishing a Release and selecting a repository-wide source license. [Extracted candidate smoke results](docs/data/m112/smoke/smoke.json) · [Candidate notes, hashes and support boundaries](docs/releases/v0.6.0-rc1.md)

### A short tour

| Exhibit | Try this |
|---|---|
| Neon Lighting | Move with WASD and fire with Space. Toggle Bloom (B), lighting (L), world-space normals (N) and shadows (O). Inspect the corresponding buffers. |
| Particle Fields | Drag a force center through 8,192 particles. Pause with P and press period to follow a single simulation step. |
| Shader Gallery | Open an effect in the Inspector, edit the file at the displayed path and press F5. A broken edit retains the last working program. The packaged Gallery keeps edits under `~/Library/Application Support/Tiny2D Showcase/Shader Gallery/`. |
| Seeded Tilemap | Change the Seed in the World panel and walk the regenerated maze. Check corners and face contacts against the collision overlay. |
| Enemy Navigation | Follow a committed enemy waypoint using the path overlay. At the upgrade prompt, choose an eligible card. |
| Replay & Timeline | The bundled recording opens at the first offer, event 600. Step to 601 for Vitality or fork at 600 and choose Fleet. Both choices occur at the same simulation Tick. |
| Survivor Classic / Enhanced | Select difficulty with 1–5, start with Enter, move with WASD and let the player auto-fire. Try B in Enhanced to compare Bloom. R restarts the applied level after an outcome. |

Survivor retains its arrow-key camera offset and Q/E zoom controls. The timeline's viewing pause and the simulation's manual pause are separate. Existing standalone scene shortcuts, controls and CLI examples are documented with each exhibit. [Showcase and Survivor](docs/milestone-11-1.md) · [Timeline](docs/milestone-10-2.md) · [Lab roadmap](docs/lab-roadmap.md)

## Build from source

Use a graphical macOS session with Xcode Command Line Tools, CMake 3.20+, GLFW and libpng available locally. Dear ImGui is vendored. The project uses the system OpenGL framework.

```sh
cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/local --parallel
ctest --test-dir build/local --output-on-failure

build/local/lab/tiny2d_lab --scene showcase
build/local/Tiny2DSurvivor.app/Contents/MacOS/Tiny2DSurvivor --presentation enhanced
```

The plain Lab executable opens Neon. The developer Sandbox retains the original immediate sprite demo and its existing command line. To run a CPU-only test suite, configure a separate build with `-DTINY2D_OPENGL_TESTS=OFF`. The macOS build still needs the platform dependencies, and the CPU replay verifier runs without a window or GL context.

```sh
build/local/tiny2d_sandbox --scene sprites --renderer batch --sprite-count 500 --frames 4
build/local/tiny2d_sandbox --scene survivor-demo --difficulty 5 --frames 600
build/local/tiny2d_m101_evidence verify docs/data/m101/cross-process/A/events.replay
build/local/tiny2d_timeline_verify branch docs/data/m102/fleet.branch 1
```

To prepare and verify a new candidate on macOS, use the packaging script. It builds a clean candidate in a new directory. The candidate notes describe archive verification, extraction, native launch and the editable shader workspace. [Packaging design](docs/release-architecture.md) · [Native verification and measurements](docs/milestone-11-2.md)

```sh
python3 scripts/package_showcase_macos.py build/m112-dist-builds build/m112-dist
```

## Engineering results

| Fixture | Observed result |
|---|---|
| 500 sprites using one texture | 500 immediate draws became 1 batched draw. In the M4.1 CPU-oriented fixture, submission p50 was 489.92 µs vs 90.67 µs. [Measurement conditions and raw data](docs/performance-m41.md) |
| 8,192 active particles | The Showcase's particle exhibit submitted 8,192 instances in one instanced draw. [Native frame records](docs/data/m111/frames.csv) |
| Seeded movement and navigation | Independent baseline builds matched 32,400 movement records and 11,319 complete navigation boundaries. An independent forward BFS matched all 4,096 first hops for the 256-enemy moving-target fixture on each tested size. [Movement](docs/data/m111/map-regression.log) · [Navigation](docs/data/m111/navigation-regression.log) · [BFS](docs/data/m111/bfs.csv) |
| Snapshot, Replay and Time Travel | Three cross-process runs matched full snapshots after 1,929 events. A 2,400-event default-map/16-enemy fixture had a 0.91 ms median seek with 120-event checkpoints and 69,005 B retained. [Replay experiment](docs/milestone-10-1.md) · [Seek distribution and memory](docs/milestone-10-2.md) |
| Classic / Enhanced at Normal and Extreme | Separate game processes produced byte-identical combat traces across 383 and 338 simulation ticks respectively, including all 44 combat events. [Combat parity](docs/data/m111/combat-parity.csv) |

The final candidate passed 89/89 native tests in both Debug and Release, 48/48 with GL tests disabled, and 18/18 CPU sanitizer tests. On the validation Mac the Lab reported zero valid GPU timestamp bits. The separate sprite benchmark recorded elapsed GPU queries. Replay determinism covers the same simulation rules and supported floating-point environment. [Final validation, performance methodology and raw data](docs/milestone-11-2.md)

## Source guide

| Directory | What to inspect |
|---|---|
| [engine](engine/) | GPU object lifetimes, batching, Scene, collisions, immutable maps and navigation |
| [game](game/) | Survivor combat, NavigationSession, snapshots, codecs, verified replay, checkpoints and branches |
| [lab](lab/) | Native showcase, HDR pipeline, lighting, particles, shader reload, map rendering and timeline tools |
| [sandbox](sandbox/) | Developer demos, independent Survivor entry, cameras, HUD and combat VFX |
| [tests](tests/) | CPU fixtures, real GL readbacks, failure recovery, process tests and performance probes |
| [scripts](scripts/) | Distribution, archive verification, offline launch and reproducible measurements |

[Development history](docs/development-history.md) · [Architecture and ownership](docs/project-architecture-overview.md) · [Bilingual portfolio / résumé](docs/portfolio-resume.md) · [Interview walkthrough](docs/interview-walkthrough.md)

## Credits

GLFW and libpng notices accompany the apps. Dear ImGui is distributed under the MIT license. Original procedural texture notices remain with the assets. The repository-wide source license awaits an owner decision. [Third-party credits](docs/third-party/README.md) · [Dear ImGui](third_party/imgui/LICENSE.txt) · [Texture notices](sandbox/assets/textures/LICENSE.txt) · [M4 assets](sandbox/assets/textures/M4-LICENSE.txt)
