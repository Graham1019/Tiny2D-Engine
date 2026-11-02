# M5 Development / v0.5.0 candidate

实现前决策：复用 Application/Window/Input 主循环、Camera2D、BatchRenderer、Texture2D；不修改渲染热点。新增 CPU Scene、Transform2D、Collision2D、FixedTimestep，以及持有纹理的 SceneRenderer 适配层。Scene Sprite 仅保存资源键，GPU 所有权集中于适配层。Arena 是独立 Application 子类，旧 Sandbox 保持原行为。

## 实现与使用

main 基线 120aae07be22b456ec9aaeec1e839462bd8b16ca；分支 feat/scene-collision；项目正式版本仍为 0.4.0。

```sh
cmake -S . -B build/m5-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m5-debug --parallel
ctest --test-dir build/m5-debug --output-on-failure
cmake -S . -B build/m5-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m5-release --parallel
ctest --test-dir build/m5-release --output-on-failure
./build/m5-release/tiny2d_sandbox --scene arena-demo --frames 120
./build/m5-release/tiny2d_sandbox --scene arena-demo
```

Arena 使用 BatchRenderer；--renderer 仅影响旧 sprites/multitexture 路径，--sprite-count 仅控制旧压力场景。WASD 玩家移动、箭头调整跟随相机偏移、Q/E 缩放（0.25–4）、Escape 退出。玩家中心 = position + (12,12)，正常 Camera 中心跟随玩家中心。网格提供空间参照，12 个初始敌人追逐玩家，接触变红、离开恢复蓝色。每秒固定步 10 创建敌人、步 40 删除最早敌人，最多 24 敌人。标题显示实际累计平均 FPS、Draw Calls、对象数、接触数。

## 实测（2026-11-01）

- Debug 和 Release configure/build 成功；各 15/15 CTest 通过，包含真实 OpenGL 与 M41 Context/Pixel Regression。
- 新增 M5_scene、M5_collision、M5_fixed_step（无需 Context），M5GPU，M5Arena。
- Scene 生命周期压力：20 轮 × 1000 对象，过期句柄、槽位复用、回调新增/删除、异常恢复。
- GPU：共享纹理、clear/removeAsset 后 Flush 前存活、Flush 后销毁、透明顺序、缩放投影、错误 Context 及恢复。
- Arena：120 frames / 120 ticks，55 objects，7 contacts，170 events，created=2 / deleted=2，draws=1；正常退出。
- 旧场景各 30 帧：Triangle=1，Immediate 500=500，Batch 500=1，Batch Multitexture 5000=5 Draw Calls。
- GUI 自动验证是真实 Context/像素读取；未声称完成人工 WASD、缩放、拖窗及最小化验收。

实现与边界见 [Scene](scene-architecture.md)、[Collision](collision-design.md)、[Fixed timestep](fixed-timestep.md)、[Review notes](m5-review-notes.md)。未实现插值、连续碰撞、刚体、完整游戏，也没有开始 M6。

## M6 roadmap（仅建议）

在下一轮源码审核后，再规划血量/伤害冷却、投射物与击杀反馈、确定性生成配置、失败/重开流程和简单 HUD。先明确玩法规模与性能预算，再考虑空间哈希和资源键管理；独立验收长时间交互、输入录制回放及资源卸载。

## 基础性能采样

Release，10 samples × 5 repeats，warmup=30；旧 M4.1 为 120 samples × 5。以下为各 trial CPU p50 的中位数（µs），小样本仅用于发现明显回归，不是新的正式 Benchmark。渲染热点源码未变。

|Workload|Sprites|Renderer|M4.1|M5|Change|
|---|---:|---|---:|---:|---:|
|cpu|500|immediate|489.9|490.0|+0.0%|
|cpu|500|batch|90.7|87.9|-3.0%|
|cpu|5000|immediate|4241.0|4359.0|+2.8%|
|cpu|5000|batch|280.9|274.0|-2.4%|
|pixel|500|immediate|511.8|480.6|-6.1%|
|pixel|500|batch|94.5|85.4|-9.7%|
|pixel|5000|immediate|4518.6|4281.5|-5.2%|
|pixel|5000|batch|279.4|410.6|+46.9%|

该测试比较旧渲染器路径，不测 Scene 碰撞开销。GPU/CPU 小样本容易受调度和图形会话影响；历史原始数据未改动。Headless 配置另行构建并通过 6/6，仍使用系统链接依赖，但不创建 OpenGL Context。

性能异常跟进：同环境保留的 integration-release 二进制在 pixel/5000/batch 的 CPU p50 中位数为 461.6 µs；M5 复测为 416.0 µs（首次 410.6），GPU 中位数均约 3.76 ms。历史对应 CPU 为 279.4 µs，但旧 trial 本身也出现 526.4 µs。当前对照不支持将波动归因于 M5 修改；更可能与采样长度、驱动排队及会话负载有关，此解释为推断。保留 baseline/repeat trials，不宣称获得性能提升或完成正式性能认证。

## M5.1 review 修复

M5 原始结果及数据保持不变。后续修复新增跨 Scene 身份检查、SceneRenderer 全量 Preflight、显式取消和纹理保活去重，详见 [M5.1 修复与新验收](m51-review-fixes.md)。没有新增 M6 功能。
