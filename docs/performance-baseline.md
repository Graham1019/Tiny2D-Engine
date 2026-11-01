# M3 性能基线

2026-10-30，macOS 26.5.1 arm64，AppleClang 21，Release；GLFW swap interval=1。
使用 Python time.perf_counter 包围 subprocess.run，执行：

```
./build/release/tiny2d_sandbox --scene sprites --sprite-count N --frames 120
```

| Sprite 数 | Draw Calls/正常绘制帧 | 120 帧进程墙钟耗时（秒） |
|---:|---:|---:|
| 1 | 1 | 1.2805 |
| 100 | 100 | 1.1985 |
| 500 | 500 | 1.1970 |

各测一次，包含进程、窗口、shader、纹理初始化和 VSync 等待；不是 GPU 时间，也不代表吞吐上限，不应用该表推断 500 Sprite 比 1 Sprite 更快。部分 Sprite 可处于相机外，本阶段不做剔除，仍提交 draw。最终帧计数可从 --frames 运行输出检查；默认场景为 12 draws/frame，最小化跳过绘制。

每个 Sprite 1 次 glDrawArrays，所有 Sprite 共用单位四边形和同一纹理。没有批渲染或 GPU 优化声明。

M4 建议：先定义透明稳定提交顺序，再引入动态顶点缓冲/索引四边形、纹理槽与批次容量；测试纹理切换、容量溢出、透明重叠的批次 flush。分别测量 CPU 提交时间、GPU timer query 和 draw/纹理切换计数，预热后多次采样并报告分位数，保留 M3 场景参数作为对照。
