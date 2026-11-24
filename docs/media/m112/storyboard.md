# Tiny2D Showcase · 100 秒作品集分镜

本机已用 AVFoundation 将 13 张 Tiny2D 的实际运行截图编成 1280×720 H.264。视频采用静态画面与英文字幕，每秒写入一个视频样本，逐帧解码验证共 100 个样本、100 秒。MP4 保存在工作区 `build/m112-portfolio/Tiny2D-Showcase-100s.mp4`。这里的 1 Hz 是剪辑时基，场景表现与 CPU/GPU 性能见各自独立的运行记录。[媒体校验结果](../../data/m112/movie.log) · [各原图 SHA-256](../../data/m112/movie-sources.csv)

| 时间 | 画面与现场演示提示 | 原始素材 |
|---|---|---|
| 0–8 s | 从展厅选择 Neon。介绍每个展品拥有独立原生窗口。 | [Showcase](../m111/showcase.png) |
| 8–16 s | 查看受遮挡的点光与材质，切换阴影和 Bloom。 | [Neon](../m111/neon.png) |
| 16–24 s | 在 Forces 拖动力场，Timing 页核对 8,192 个实例和一个实例化 Draw。 | [粒子](../m111/particles.png) |
| 24–36 s | 在可写副本引入错误，F5 后看编译诊断。修复，再次 F5 观察新画面。 | [错误](../m111/reload-failed.png) · [恢复](../m111/reload-recovered.png) |
| 36–44 s | 移动玩家，展示 Seed、连接地板和碰撞。 | [地图](../m111/map-71.png) |
| 44–52 s | 在转角观察敌人完成已承诺的路段，切换路径叠加。 | [导航](../m111/navigation.png) |
| 52–60 s | 从有 16 条未结束路段的 Tick 37 快照恢复，回放后对照同设备最终像素。 | [已验证的回放画面](../m101/verified.png) |
| 60–68 s | 把时间轴退到 Cursor 600，松开滑块，读出 Checkpoint 和当前 Offer。 | [Seek](../m102/seek-before-choice.png) |
| 68–76 s | 从 Offer 分叉，选 Fleet，再切回保留了 Vitality 的父记录。 | [分支](../m102/child-fleet.png) |
| 76–83 s | 普通难度 Classic 模式的首个击杀，Tick 87。 | [Classic](../m111/survivor-classic.png) |
| 83–92 s | 同 Tick 和难度切换到 Enhanced，对照原 HUD、HDR 世界与战斗 VFX。 | [Enhanced](../m111/survivor-enhanced.png) |
| 92–100 s | 关闭展品返回同一展厅；以独立候选包启动同样的演示。 | [返回展厅](../m111/showcase-return.png) |

## 复现

在 macOS 原生媒体编码环境、仓库根目录执行。目标文件应为新路径。

```sh
OUT=$(mktemp -d)
swiftc -O docs/data/m112/render_portfolio.swift -o "$OUT/render-portfolio"
"$OUT/render-portfolio" . docs/media/m112/scenes.tsv "$OUT/Tiny2D-Showcase.mp4"
```

字幕、顺序和每幅画面的停留时间保存在 TSV。编码器按原图比例放入画面，并在底部独立区域绘制字幕。现场讲解可以按表内步骤实际操作。[字幕与时长](scenes.tsv) · [编码与解码验证](../../data/m112/render_portfolio.swift)
