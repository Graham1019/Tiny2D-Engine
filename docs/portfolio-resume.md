# Portfolio / resume material

## 中文项目经历（3 条）

- 使用 C++17 与 macOS OpenGL 构建轻量 2D 引擎，以 RAII 和 Context 校验管理 GPU 资源，实现 Scene 代际句柄、相机、纹理与异常安全渲染通道。
- 实现动态 VBO / 静态 EBO 多纹理批渲染；500 Sprite 测试中 Draw Calls 从 500 降为 1（99.8%），M4.1 指定 CPU 提交场景 p50 为 489.92 对 90.67 µs，约 5.40× 比值，不代表整体 FPS。
- 将 CPU GameSession 与事件驱动 VFX/HUD 解耦，支持 60 Hz 固定 Tick、五档难度、90 秒生存与重开；完成 Debug/Release 各 40 项、Headless 23 项回归及 macOS arm64 RC 打包。

## English project bullets (3)

- Built a C++17 macOS OpenGL 2D engine with RAII GPU ownership, context validation, generation-based scene handles and exception-safe rendering passes.
- Implemented multi-texture batching with a dynamic VBO and static EBO, reducing 500-sprite draw calls from 500 to 1; measured a 5.40× CPU submission p50 ratio in the specified M4.1 fixture, not a whole-game FPS gain.
- Separated CPU gameplay from bounded event-driven VFX and HUD, delivered five difficulty levels with 60 Hz simulation, and validated 40 Debug, 40 Release and 23 headless tests before packaging an arm64 macOS release candidate.

## 中文简介（约 80 字）

Tiny2D 是基于 C++17 与 macOS OpenGL 的轻量二维引擎作品，实现批渲染、固定步长战斗、代际场景句柄和事件特效，以可玩生存 Demo、回归测试及独立 App 打包展示资源管理与工程交付能力。

## English overview (119 words)

Tiny2D is a lightweight C++17 engine and playable arena-survival demonstration for macOS.
Its renderer combines explicit GPU ownership with multi-texture batching, while its CPU
gameplay layer handles fixed-step simulation, collision and difficulty independently of
OpenGL. Generation-based scene handles reject stale references, and render-pass guards
recover pending work after exceptions. A bounded combat-event stream drives hit and death
feedback without changing gameplay rules. The project includes reproducible performance
fixtures, CPU and real-context GPU regression tests, and a standalone arm64 macOS app
with bundled third-party libraries and documented asset resolution. The release candidate
is ad-hoc signed for local validation, with public distribution, notarization and the
repository source-license decision still subject to owner review. It demonstrates focused
engine design rather than commercial engine completeness.

Evidence: [M4.1 performance](performance-m41.md), [M6.3b.1](m63b1-review-fixes.md),
[M6.3c tests and distribution](milestone-06-3c.md). Do not claim cross-platform,
Metal/Vulkan implementation, multithreaded rendering or certified leak freedom.
