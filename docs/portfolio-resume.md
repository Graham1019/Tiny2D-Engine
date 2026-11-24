# Tiny2D · 作品集与简历 / Portfolio and résumé

**一句话。** 用 C++17 与 macOS OpenGL 制作一个可玩的 2D 引擎作品集，让读者能从同一个展厅观察 HDR 渲染、重现一局确定性模拟，并从一次升级选择分出另一条时间线。

**One-line pitch.** A C++17 and OpenGL engine for macOS with eight native exhibits, including a playable arena survivor and a deterministic replay lab where the viewer can rewind an upgrade choice and play out a different outcome.

[实际展厅与操作](milestone-11-1.md) · [100 秒运行画面与演示顺序](media/m112/storyboard.md) · [架构与源码入口](project-architecture-overview.md)

## 中文简历项目经历

**Tiny2D Engine / C++17 · OpenGL · GLSL · CMake · macOS arm64**

- 实现多纹理精灵批渲染、粒子实例化和 HDR 光照管线。在相同 500 精灵夹具中，批处理将提交从 500 Draw 降至 1 Draw；8,192 个活动粒子也可一次实例化绘制。[精灵基准](performance-m41.md) · [原生展厅记录](data/m111/frames.csv)
- 实现从任意合法事件边界保存与事务式恢复导航世界，记录实际消费的输入和控制操作并逐事件验证。三个独立进程实验共核对 1,929 个事件后的完整状态，覆盖同 Tick 选卡与跨地图 Epoch。[格式、事务与实验](milestone-10-1.md)
- 构建有界 Checkpoint、随机定位与可导出的独立分支，使同一份 Seed 71 记录可演示 Vitality 和 Fleet 两种未来；为 Classic/Enhanced Survivor 接入共享战斗模拟，以完整 CombatEvent 轨迹对照呈现改造前后的规则。[分支边界](data/m102/boundaries.csv) · [战斗轨迹](data/m111/combat-parity.csv)

## English project bullets

**Tiny2D Engine / C++17 · OpenGL · GLSL · CMake · macOS arm64**

- Built multi-texture sprite batching, GPU-instanced particles and an HDR lighting pipeline. Reduced 500 immediate sprite draws to one batch in a controlled fixture and rendered 8,192 live particle instances in a single draw. [Sprite fixture](performance-m41.md) · [Native frame evidence](data/m111/frames.csv)
- Implemented versioned binary snapshots, transactional restore and deterministic replay of consumed gameplay commands. Compared the complete canonical state after each of 1,929 events across three independent record/replay experiments. [Cross-process evidence](data/m101/cross-process/cross-process.log)
- Added a bounded checkpoint index and independently playable branches. Forked the same recorded offer into two outcomes at Tick 600 while preserving the parent, and validated Classic and Enhanced Survivor against identical combat traces at two difficulties. [Branch boundaries](data/m102/boundaries.csv) · [Combat parity](data/m111/combat-parity.csv)

## 图形渲染岗位 / Rendering focus

**中文。** 将 2D 渲染器从立即式精灵绘制扩展到批处理、实例化粒子和浮点 HDR 帧图，提供可检查的 Albedo、Emission、法线、阴影和 Bloom 中间结果。通过真实 OpenGL 像素回读、状态恢复测试和同条件 CPU 提交分位数验证取舍。[渲染与性能](performance-m41.md) · [光照](milestone-07-3.md) · [硬阴影](milestone-07-4b.md) · [粒子](milestone-08-1.md)

**English.** Extended an immediate-mode sprite path with batching and instancing, then built inspectable HDR, normal-mapped lighting, hard shadows and Bloom. Verified pass behavior with native GL pixel readback and measured CPU submission under controlled workloads. [Renderer](../engine/src/Renderer/BatchRenderer.cpp) · [HDR pipeline](../lab/src/Pipeline.cpp) · [Rendering evidence](milestone-11-1.md)

## 引擎与工具岗位 / Engine and tools focus

**中文。** 为固定步长模拟定义状态所有权与文件格式，按事件序列验证录制、按有界索引回到任意合法边界，并以独立子记录试验新的输入。将规则状态、渲染缓存和运行实例身份分开，使回放结果可以在无窗口进程中检查。[快照恢复](../game/src/NavigationSnapshot.cpp) · [验证与检查点](milestone-10-2.md)

**English.** Designed the ownership and persistence boundaries of a fixed-step simulation. Implemented verified event replay, memory-bounded checkpoints and standalone branches, with headless tools to locate the first mismatching event and reproduce a saved state in a fresh process. [Dispatcher and player](../game/src/Replay.cpp) · [Timeline](../game/src/Timeline.cpp) · [Branches](../game/src/ReplayBranch.cpp)

## 五项可复核结果 / Five reproducible results

| 实验 / Fixture | 结果、条件与证据 / Result and evidence |
|---|---|
| 500 sprites / 8,192 particles | M4.1 的 500 精灵单纹理提交从 500 Draw 变成 1 Draw，CPU 提交 p50 从 489.92 µs 变成 90.67 µs。M11.1 粒子展品实际记录 8,192 个实例、1 个实例化 Draw。两种夹具分别计量。[M4.1](performance-m41.md) · [Showcase 帧记录](data/m111/frames.csv) |
| Movement / pathfinding | 历史构建对照匹配 32,400 条地图运动记录、11,319 个导航边界。独立正向 BFS 在每个测试地图尺寸匹配 4,096/4,096 个反向场首步。[Movement](data/m111/map-regression.log) · [Navigation](data/m111/navigation-regression.log) · [BFS](data/m111/bfs.csv) |
| Snapshot / Replay | 三组跨进程实验逐事件比较字段与规范字节，共 1,929 个事件。真实 OpenGL 固定显示条件下，对照截图与最终 Snapshot 文件分别逐字节一致。[CPU 实验](data/m101/cross-process/cross-process.log) · [原生 GL](data/m101/gl.log) |
| Checkpoint / Seek | 单机 Release、64×40、16 敌人、2,400 事件、每策略 128 次随机 Seek。仅保留 Base 时 p50 11.48 ms；间隔 120 个事件时为 0.91 ms，共保留 21 项、69,005 B。[M10.2 原始样本](data/m102/seek.csv) · [各地图与分位数](data/m102/quantiles.csv) |
| Showcase / Survivor | 八个真实展品及两次重开共拉起、回收 10 个子进程。普通/极端难度分别逐 Tick 核对 383/338 行记录，Classic 与 Enhanced 的 21/23 个有序战斗事件及完整状态轨迹逐字节一致。[端到端验收](milestone-11-1.md) · [每档轨迹摘要](data/m111/combat-parity.csv) |

所有时间数字均附夹具与测量设备。当前可分发形态为需目标 Mac 验收的 arm64 本地候选包。演示范围、CPU/GPU 计时可用性、签名方式和手工检查见候选说明。[最终验证与数据](milestone-11-2.md) · [候选包与验收](releases/v0.6.0-rc1.md)
