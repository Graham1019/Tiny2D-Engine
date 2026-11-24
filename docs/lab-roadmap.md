# Tiny2D Lab 路线

| 阶段 | 能力 | 状态 |
|---|---|---|
| M7.1 | 独立 Lab 入口、neon、RGBA16F 目标、Bloom 和四种观察模式 | 已合并 PR #11；用户反馈交互与视觉试玩无异常 |
| M7.2 | [对象检查、拾取、缓冲预览、输入捕获与性能观测](milestone-07-2.md) | 已合并 PR #12，用户完成七项 Mac 人工验收 |
| M7.3 | [二维动态点光、表面合成与 LightBuffer 检查](milestone-07-3.md) | PR #13 已合并，主设备七项人工验收通过 |
| M7.4a | [世界空间法线材质、方向光照与 NormalBuffer](milestone-07-4a.md) | PR #14 已合并，主设备七项人工验收通过 |
| M7.4b | [逐灯硬遮挡、可见度遮罩与 Shadow Inspector](milestone-07-4b.md) | 已合并 PR #15 |
| M8.1 | [确定性粒子力场、HDR 实例化与性能测量](milestone-08-1.md) | PR #16 已合并，源码 Review 与七项 Mac 人工验收通过 |
| M8.2 | [Shader 展厅、手动安全重载与像素验证](milestone-08-2.md) | PR #17 已合并，源码 Review 与六项 Mac 主设备人工验收通过 |
| M9.1 | [种子地图、连通性、Tilemap 与地图碰撞](milestone-09-1.md) | PR #18 已合并，ChatGPT 源码 Review 与六项 Mac 主设备人工验收通过 |
| M9.2 | [敌人导航与随机增益](milestone-09-2.md) | [PR #19](https://github.com/Graham1019/Tiny2D-Engine/pull/19) 已合并，ChatGPT 源码 Review 与七项 Mac 主设备人工验收通过 |
| M10.1 | [完整快照、输入记录与确定性回放](milestone-10-1.md) | [PR #20](https://github.com/Graham1019/Tiny2D-Engine/pull/20) Open，等待源码 Review 与 Mac 主设备人工验收 |
| M10.2 | [有界时间轴、事件定位与分支继续](milestone-10-2.md) | [PR #21](https://github.com/Graham1019/Tiny2D-Engine/pull/21) 依赖 #20，等待源码 Review 与 Mac 主设备人工验收 |
| M11.1 | [统一展厅及 Survivor 增强显示](milestone-11-1.md) | [PR #22](https://github.com/Graham1019/Tiny2D-Engine/pull/22) 依赖 #21，等待源码 Review 与 Mac 主设备人工验收 |
| M11.2 | [本地候选包、真实演示与作品集](milestone-11-2.md) | 依赖 PR #22，等待统一源码 Review 与 Mac 主设备人工验收 |

M7 至 M9.2 经逐阶段源码 Review 和主设备试玩后合并。M10.1 至 M11.2 采用依赖式 Open PR，前一阶段通过自动化验收后继续下一阶段，四阶段提交一起等待最终审查与主设备验收。M7.2 的观察面板通过当前帧需求读取亮部与模糊结果。M7.1 的用户反馈涵盖已试玩环境中的交互与视觉，跨显示器和其他系统的覆盖仍以各阶段的实测记录为准。

## 接口边界

- 引擎拥有 Context 限定的 GPU 资源。Lab 负责演示编排和显示参数，Survivor 的 CPU 规则仍由 game 拥有。
- Lab 的固定 Tick 状态包括位置、Tick、弹丸与爆发槽位、环形写入索引、下一个目标与视觉 RNG 状态。未来从中途回放要保存这些值以及后续逐 Tick 输入。从头重演可恢复初始 seed 并重新施加相同输入。
- Bloom、HUD、观察模式、逻辑与像素尺寸、计时以及 GL 名称属于显示侧。暂停/单步控制 Tick 的调度。未来若检查 Scene 实体，应保存含 scene token 与 generation 的 ObjectId，在使用时重新查找。
