# M6.3c — macOS release and portfolio packaging

## Baseline and scope

2026-11-05: clean starting tree on feat/combat-polish-profiling at 0f63fe2.
Fetched origin through the requested HTTP proxy. `origin/main` is
`d4b7440d07645eff89c6d660dc6849cfb0ba43ad`, the merge of PR #9; ancestry check
confirms 0f63fe2. Fast-forwarded main, created `feat/macos-release-packaging`.
No reset, force push, tag, merge or release publication. No M7 or balance change.

## Architecture and build

[Bundle architecture and commands](release-architecture.md) document static
engine/game/UI linkage, independent SurvivorMain calling shared runSurvivor(0),
MACOSX_BUNDLE Info.plist, Resources/assets and Frameworks layout. Sandbox default
and its Triangle/Immediate/Batch/Multitexture/Arena/Survivor CLI remain unchanged.
Version 0.5.0, bundle build 5001, candidate rc1; GitHub-owned namespace identifier.
CMake target Tiny2DSurvivor builds the developer App; package_macos produces
`Tiny2D Survivor.app` with recursively relocated dependencies and fresh resources.

Canonical executable-relative asset resolution supports symlinks and spaces,
uses Resources in bundles and adjacent assets in development, and has no cwd,
source-tree or Homebrew fallback. Missing assets report the expected full path.
Negative test removes a resource only from a temporary copy and executes through
a symlink from `/`; [result](data/m63c/missing-assets.txt) confirms failure.

## Dependencies / environment

[Environment](data/m63c/environment.txt), [full dependency/signature checks](data/m63c/package-final.txt),
and [live loaded files](data/m63c/runtime-files.txt). GLFW 3.5.1, libpng 1.6.59;
both installed dylibs require macOS 26.0, so target minimum is 26.0. Actual host
26.5.1, AppleClang 21, arm64. file/lipo verified executable; verifier checks all
included dylibs. System OpenGL, zlib and Apple frameworks remain system references.
No third-party load command references Homebrew after fixup. Full installed-package
licenses retained in docs/third-party and Resources/licenses. Source license undecided.

## Independent run

ZIP independently extracted by ditto outside source/build tree into a system
temporary directory, App path contains spaces. `open -n` launched with no arguments.
Native UI observation confirmed default Lv.3 menu, Enter → Playing, HP/countdown/kills,
combat/damage presentation, actual GameOver, R restart and Escape termination.
Screenshots menu.png, gameplay-lv3.png, game-over.png preserve actual observed frames.
Live lsof showed bundled GLFW/libpng, no project or Homebrew runtime file mapping.
This is not exhaustive file-access tracing or a clean-Mac test.
Final rebuilt package was independently extracted again under
`tiny2d-release-validation.dKWbzV`; menu, Enter, Playing/VFX/HUD and Escape were
observed again. Earlier GUI evidence used candidate-S0BApO, before adding the credits
README; final candidate-dOgOD9 uses the same gameplay sources.

## Signatures and artifacts

Nested libraries signed before outer App, ad-hoc only. Strict deep verification
passed before and after ZIP extraction. [spctl](data/m63c/gatekeeper.txt) reported
accepted with **security disabled override** on this host; do not infer ordinary
Gatekeeper acceptance. No host policy was changed. No Developer ID or notarization.

Final candidate: `build/m63c-dist/candidate-dOgOD9/`.
ZIP: `Tiny2D-Survivor-v0.5.0-rc1-macos-arm64.zip`.
SHA-256: `6c1fd130d99795b00a9224172dccff09a05a1640eeb07845a60875d8acfff3c1`.
`SHA256SUMS` accompanies it; checksum verified by script. Earlier candidate directories
remain intact. Build binaries are ignored, not committed. See [release draft](releases/v0.5.0-rc1.md).

## Regression evidence

| Suite | Result | Evidence |
|---|---|---|
| Debug | 40/40 | [normal graphical session rerun](data/m63c/debug-ctest-rerun.txt) |
| Release | 40/40 | [log](data/m63c/release-ctest.txt) |
| Headless | 23/23 | [log](data/m63c/headless-ctest.txt) |
| Packaging smoke | PASS | [final log](data/m63c/package-final.txt) |
| Missing asset / symlink / spaces / cwd | PASS | [log](data/m63c/missing-assets.txt) |
| Legacy draws | 1 / 500 / 1 / 5 | [log](data/m63c/legacy.txt) |

Initial Debug execution inside the restricted process sandbox could not reach
macOS graphical services and timed out; it was interrupted, original partial
[log retained](data/m63c/debug-ctest.txt). The complete rerun with graphical access
passed all 40. CPU headless tests did not create a context. All historical tests
remain registered, including M6.3a/b/b.1. No hosted CI run or new benchmark claimed.
Packaging smoke is a separate non-GUI script, not an automatically registered CTest.

## Portfolio

README rewritten for visitors with actual screenshots, build/run steps, scoped
performance and design links. Historical README moved to development-history.md
with relative links adjusted. New architecture diagrams, bilingual resume and
14-question source-linked interview walkthrough; no inflated platform/FPS claims.
[Media status and filming script](media/README.md); video has not been recorded.

## Remaining human acceptance / owner decisions

- [ ] Finder double-click first launch, including a real quarantined download.
- [ ] Resize, minimize/restore and repeated close/reopen.
- [ ] Retina visual review and movement across different displays.
- [ ] Full human Lv.1/Lv.3/Lv.5 sessions, Victory/GameOver and restart.
- [ ] Lv.5 and Victory screenshots; optional 30–60s video.
- [ ] Another clean Mac and additional macOS versions; Intel/Universal unsupported.
- [ ] PR review, final version, repository LICENSE, Developer ID/notarization policy.
- [ ] Explicit later authorization for GitHub Release and uploaded assets.

Stop after commit/push/PR; do not merge or begin M7.
