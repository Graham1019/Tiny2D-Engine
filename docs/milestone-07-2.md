# M7.2 · 在运行中的 Neon 上检查对象与渲染成本

![点击场景选中 Player 后查看属性](data/m72/selection.png)

橙色轮廓标出当前选中的 Player。右侧读取选中对象的位置、身份和快照 Tick。对象面板在本帧固定更新前构建，运行时它的快照 Tick 可以比底部 HUD 小一。暂停后可在同一 Tick 对照属性和图像。

## 操作与复现

| 操作 | 效果 |
|---|---|
| F2 / F1 | 显隐 Inspector / HUD |
| WASD / Space | 移动 / 加快发射 |
| B / [ / ] | Bloom 开关 / 强度以 0.1 调整，内部整数 0–30，默认 12 |
| 1 / 2 / 3 / 4 | Scene / Bright / Blur / Final |
| P / . / R | 暂停或恢复 / 暂停时恰好一 Tick / 按原 seed 重置 |
| 左键 / Objects | 在 Scene 或 Final 拾取，或在任意视图用列表选择；文本框可按对象名称或槽号筛选 |
| Controls | 调整显示、暂停、单步、重置、PickBounds、选中轮廓、缓冲预览、GPU 计时 |
| Timing | 最近一次 CPU 提交与异步 GPU 记录、Pass Draw、payload 和最近秩分位数；悬停阶段行看最近值 |
| Escape | 文本或弹出层处理自己的交互，场景获得键盘时退出 |

```sh
cd /Users/grahm/Desktop/Tiny2D-Engine
cmake -S . -B build/m72-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m72-release --parallel
./build/m72-release/lab/tiny2d_lab --scene neon
./build/m72-release/lab/tiny2d_lab --scene neon --seed 71 --frames 4 --view final --bloom on --inspector off --gpu-timing off --capture /tmp/m72-final.png
./build/m72-release/lab/tiny2d_lab --scene neon --seed 71 --frames 4 --view final --bloom on --inspector on --previews all --gpu-timing on --capture docs/data/m72/buffers.png
python3 docs/data/m72/measure.py run
python3 docs/data/m72/measure.py summarize
```

交互默认为面板和计时开启、预览关闭。有限帧默认为三者全关，显式开启预览需要 `--inspector on`。保留 `--size`、`--scene`、`--seed`、`--frames`、`--bloom`、`--view`、`--capture` 以及基线的错误返回。新增 `--inspector on|off`、`--gpu-timing on|off`、`--previews off|all` 和 `--metrics PATH`。有限帧每个成功展示帧推进一个 Tick，ImGui 关闭实时设备输入、使用固定初始布局和 1/60 GUI delta time，关闭 ini 读写。性能文字反映当次运行。四种无工具画面另行对照 M7.1。

## 输入到展示

`Window` 持有唯一 GLFW 窗口与原 OpenGL Core Context。仅 Lab 工具目标链接 Dear ImGui v1.93.0，官方标签解析到 `c1aaac65e896a45f81f47a4d68f9959e0f86c859`。CMake 从仓库源码构建，GL 使用项目现有的 Apple 系统头。ImGui 使用动态字体纹理和实际 framebuffer scale，面板位置、控件、鼠标均以逻辑点计算。许可证、使用文件和后端补丁记录在[依赖说明](../third_party/imgui/README.md)与[完整许可证](../third_party/imgui/LICENSE.txt)。

1. GLFW poll 原始事件。交互 Lab 在焦点回调中取消上一周期的输入，再转交官方 GLFW backend。官方后端串联 Window 的失焦回调，接收键盘、字符、滚轮与鼠标。Window 继续处理 framebuffer resize。
2. 查询逻辑与像素尺寸，在帧边界提交完整目标组替换，回收可读的旧 query。真实失焦和不可绘制使固定时钟丢弃累积时间。有限帧沿用原有 drawable 等待上限。
3. GUI NewFrame 构建控件。F2 使用原始键的按下边沿，即使输入框持有键盘也可切换。键盘捕获和活动控件屏蔽场景动作，已按住的键进入释放门控。Return 属于输入框，场景没有 Return 松开动作。鼠标按下时判定归属，面板切换、弹出层和恢复帧额外屏蔽穿透；拖出再松开不会拾取。
4. UI 和键盘的动作边沿取并集，显示参数在固定更新前应用。调度优先级依次为 Reset、Pause 切换、暂停单步或运行时累积时钟。捕获键盘期间运行中的时钟继续推进。列表选中走工具路径。重置保留暂停、显示、工具开关和布局，增加 epoch，清除选择、已展示几何与场景性能窗口。
5. 更新模拟，准备世界、HUD 和调试几何。补齐预览窗口并记录可见图像需求和本帧 Tick，生成 draw data。Pipeline 生产纹理、转换、合成、绘制轮廓与 HUD，最后提交 GUI。成功交换才发布供下一次点击使用的几何快照。

关闭时，未提交的 GUI 帧先丢弃，再释放 OpenGL 后端、恢复 GLFW 回调、销毁 ImGui Context。query 和图像均在所属 Context 有效时销毁。初始化失败按已完成的步骤清理。CPU 的 Neon、控制和选择逻辑可在无窗口进程中测试。

## 身份、显示坐标与调试几何

`LabHandle` 由 session epoch、类别、slot 和 spawn generation 组成。Player 和四个 Relay 的 generation 固定。12 个 Bolt 槽、8 个 Burst 槽在实际生成点递增各自 generation。列表和拾取都复制值，访问时验证 epoch、kind、槽界限、存活和 generation；重置的 epoch 位于 Neon 外层。选择操作只读模拟，不推进 RNG。

已展示快照带 camera、逻辑内容矩形、视图、目标 generation、renderFrameId、Tick 及对象几何。当前帧切视图、尺寸失配、尚未交换、零尺寸和过期句柄都拒绝旧画面命中。无旋转正交相机先减内容原点，再由相机中心加上相对视口中心的位移除以 zoom，世界 +Y 向下。鼠标已经是逻辑点，framebuffer 像素只用于目标和读回。Bolt 使用与显示相同的插值函数，Burst 使用同一射线半径。语义圆在屏幕空间增加 3 个逻辑点容差；重叠先按显示层，再按距离和槽号决定。

Neon 的物体没有 Collider。PickBounds 显示对应点击范围，选中轮廓与其共用几何。引擎的 `outlineCollider` 通过 Scene 和 ObjectId 查找真实启用对象，复用碰撞模块的 `worldShape`，将局部 AABB 或圆按位置平移，生成世界线段。现有碰撞规则中旋转与缩放只影响视觉。真实 Collider fixture 验证了 AABB 像素、位置变更后的圆以及删除后的失效。

## 缓冲预览与资源

![四张当前帧的显示色预览](data/m72/buffers.png)

SceneColor 与 Bright/Ping/Pong 保持线性 RGBA16F。GUI 借用的四张图是长边最多 256 像素、等比缩小的 RGBA8。复用主画面的 display shader 做 Reinhard 和一次 sRGB 编码，Final 使用相同强度的场景合成。HUD、轮廓和 Inspector 随后画到默认 framebuffer。ImGui 图像顶端用 V=1，底端用 V=0。像素测试通过实际 draw command 找到屏幕上的缩略图，读取非对称 HDR 标记，覆盖外部 sRGB 开和关。

| 需求 | 生产与提交 |
|---|---|
| Bright 可见 | 本帧提取亮部 |
| Blur 可见，或 Final 有 Bloom 贡献 | 本帧提取亮部，水平、垂直模糊 |
| 可见缩略图 | 每张一次输出 shader 转换。缩略图折叠、屏外、关闭时按实际可见掩码跳过 |
| 主画面 | 输出所选视图，Scene/Final 才提交启用的世界轮廓，再画 HUD 和 GUI |

纹理视图含借用名称、源尺寸、缩略图尺寸、目标 generation。渲染器唯一拥有资源。首次需要预览时整组分配，resize 对已有预览连同 HDR 组先构造候选组，全部成功才交换。生成 Image 命令后到 RenderDrawData 提交或显式 discard 之间，主循环不替换目标。恢复和异常测试包含两个真实新目标成功后，下一次生产 GLObject 创建检测到注入错误。检查临时名称释放、旧组身份与字节、viewport/FBO 恢复、失败期间拒借及下一次绘制。

FrameState 保护 draw/read FBO、viewport、程序、VAO、采样器、纹理、混合、写掩码、scissor 与 sRGB 等状态。GUI backend 保护自己的 VAO/VBO、scissor box、混合因子等，Lab 另包围字体更新所需的 unpack PBO 和 skip offsets。外部 sRGB 在显示绘制时关闭，离开时恢复。四个 HDR 附件理论字节为 `8 × (W×H + 3×ceil(W/2)×ceil(H/2))`；四张 256×144 预览合计 589824 B。字体按实际存在的 RGBA32 atlas 尺寸计。以上为已知附件和纹理数据，驱动额外存储未知。

## CPU 提交与 GPU 命令边界

![实际 CPU 提交和本机 timestamp 能力](data/m72/timing.png)

CPU 稳态窗口最近 120 个完整帧。总 CPU 时间从 poll 返回后开始，覆盖尺寸、query 回收、GUI 构建、输入、固定更新、几何准备、各 Pass 提交与状态恢复，在 swap、读回、文件写入前停止。GUI build 分两段，预览行聚合本帧实际执行的转换。阶段时间嵌套于 total。全部有效帧的最近秩分位数可从 CSV 重算。批次上报已提交的顶点 payload，GUI 在后端实际上传与越过剪裁/回调筛选后的 Draw 位置计数；纹理上传单列。静态 EBO、VBO 预留容量和驱动显存不计入每帧 payload。

GPU 路径探测实际版本/扩展、入口和 `GL_TIMESTAMP / GL_QUERY_COUNTER_BITS`。8 个槽，每槽 20 个 query 名称，覆盖 9 个阶段及总渲染的一对起止 timestamp，支持时上限 160。总区间含本帧实际执行的预览与 GUI，结束在交换前。每槽携带发起帧、Tick、epoch、目标组及像素尺寸、显示/预览/GUI 配置、实际 Pass mask、预热或测量归属。后续边界有限检查 AVAILABLE，已发出的全部标记可用才读 64 位结果。池满直接 skipped。计时关闭仍继续回收旧槽。

按有效位屏蔽高位，拒绝逆序、跨度超过半个计数范围或超出外层区间的区间，纳秒差除以 1000 得到 µs。不完整帧计 invalid，槽位仍占用直到先前标记都可回收。晚到的旧 epoch 不进入当前场景窗口。CPU 和 GPU 分别显示帧号，GPU 同时显示回收延迟。timestamp 之间的间隔可包含调度影响。全程和采样窗口分别记录 requested = completed + pending + skipped + invalid，未发起或未就绪的区间没有 GPU 时长。[OpenGL glQueryCounter](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl4/glQueryCounter.xml) · [ARB_timer_query](https://github.com/KhronosGroup/OpenGL-Registry/blob/main/extensions/ARB/ARB_timer_query.txt)

## 测试与回归

基线 `927c8f89aef9abcf410019ce008e432b641fef4d` 已合并 PR #11，包含 M7.1 交付 `4c268f10bd1d9b73ed0d5a273484daec8ab096d2`。继承全部历史 CTest，新增 M72CPU 和 M72InspectorGL。

| 构建 | 基线清单 | 本次结果与用例 |
|---|---|---|
| Debug，OpenGL ON | [43 项](data/m72/baseline-debug.txt) | [45/45](data/m72/ctest-debug.log) |
| Release，OpenGL ON | [43 项](data/m72/baseline-release.txt) | [45/45](data/m72/ctest-release.log) |
| Release，OpenGL OFF | [24 项](data/m72/baseline-headless.txt) | [25/25](data/m72/ctest-headless.log) |

```sh
cmake -S . -B build/m72-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m72-debug --parallel
ctest --test-dir build/m72-debug --output-on-failure
ctest --test-dir build/m72-release --output-on-failure
cmake -S . -B build/m72-headless -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=OFF
cmake --build build/m72-headless --parallel
ctest --test-dir build/m72-headless --output-on-failure
sh docs/data/m72/verify-demo.sh
```

- M72CPU 核对 CLI 组合、Release 门控和调度优先级，比较脚本输入下的全部原模拟字段、轮转索引与 RNG。覆盖句柄过期、复用、epoch、非法类别和槽位、非零原点/奇数尺寸/zoom 与不同像素密度下的坐标转换。fake query 后端在读取未就绪结果时使测试失败，断言池满跳过、partial 回收、旧 epoch/预热身份、有效位与回绕、微秒换算和分类守恒。CSV fixture 验证在没有后续渲染帧时也能写出迟到记录。
- M72InspectorGL 使用图形登录会话、生产 renderer 和官方后端。注入鼠标、键盘和字符，操作 Bloom、Pause/Resume、Step、对象列表和可编辑筛选框。测试拖离释放、文本中的 R/W/Space/P/Return、F2 持有与恢复、有限帧隔离。另行读回真实 Collider 轮廓、GUI 缩略图的非对称标记、Bloom off 时变化的中间缓冲及 HUD 外的 Final。测试创建故障、外部 unpack/sRGB 状态、错误 Context、弃帧和下一帧恢复。硬件有可用位时相同测试要求真实 query 完成。
- M71GPU 的原四视图、浮点和状态恢复断言，M71RelocatedCLI 的资源定位及故障返回均继续通过。正式 CLI 的四张 Tick 4 无工具截图用 `cmp` 与仓库 M7.1 PNG 逐字节相等。旧 Sandbox 的三角形、500 immediate、500 batch、5000 multitexture 分别为 1 / 500 / 1 / 5 Draw。经典 Survivor 的难度、流程、事件、终局及容量测试保留。[命令与日志](data/m72/demo.log)
- 交互 Lab 和 Tiny2DSurvivor.app 各自启动后，按进程 ID 观察到可见窗口，随后结束本次 smoke 启动的进程。对象和 Timing 截图来自固定窗口尺寸的自动后端事件测试，Buffers 截图来自正式有限帧入口。这些是自动化运行证据。[启动记录](data/m72/interactive.log)

源码复核期间修正了异常退出的计数、跨帧 CSV 回收和面板隐藏时字体存储的记录。首次编译新 GL fixture 时 `Scene` 类型名与 Lab 按键同名，改用限定名后重新构建。沙箱内的 GitHub 请求及硬件 sysctl 查询受限，通过受管授权重试取得结果。

## Release 工具成本

实测代码 `6df39b8efa14fb718cad3327a165dc4afae5b26c`。Apple M5 Pro、macOS 26.7.3、Apple clang 21.0.0、OpenGL 4.1 Metal 90.5。每格单独启动同一个 Release 程序，1320 个成功展示帧、1320 Tick。预热 120 帧，随后采样 1200 帧。两个尺寸都用本次配置 1 作为各自基线。全窗场景、HUD 与 Bloom 开启，seed 71、Final、无选择、无调试几何。含预览的每个测量帧均验证 mask=15 及四次转换 Draw。[协议](data/m72/protocol.md) · [环境](data/m72/environment.txt) · [全部阶段分位数与样本数](data/m72/quantiles.md)

| 实际像素 | 配置 | CPU 帧 p50 / p95 / p99（µs） | Pass / Draw | 批次顶点 payload p50（B） | GUI 顶点 / 索引 payload p50（B） | 原始记录 |
|---|---|---:|---:|---:|---:|---|
| 1600×900 | 1 · 工具全关 | 109.208 / 130.875 / 158.542 | 6 / 6 | 142128 | 0 / 0 | [CSV](data/m72/800x450-1.csv) · [日志](data/m72/800x450-1.log) |
| 1600×900 | 2 · Inspector | 125.958 / 166.709 / 202.708 | 7 / 8 | 142128 | 20680 / 4260 | [CSV](data/m72/800x450-2.csv) · [日志](data/m72/800x450-2.log) |
| 1600×900 | 3 · 再开四预览 | 158.292 / 192.583 / 218.125 | 11 / 22 | 144144 | 35080 / 6582 | [CSV](data/m72/800x450-3.csv) · [日志](data/m72/800x450-3.log) |
| 1600×900 | 4 · 再请求 GPU 计时 | 164.291 / 203.917 / 221.250 | 11 / 22 | 144144 | 36880 / 6870 | [CSV](data/m72/800x450-4.csv) · [日志](data/m72/800x450-4.log) |
| 2240×1260 | 1 · 工具全关 | 113.791 / 152.833 / 188.500 | 6 / 6 | 141552 | 0 / 0 | [CSV](data/m72/1120x630-1.csv) · [日志](data/m72/1120x630-1.log) |
| 2240×1260 | 2 · Inspector | 126.875 / 178.500 / 201.334 | 7 / 8 | 141552 | 20680 / 4260 | [CSV](data/m72/1120x630-2.csv) · [日志](data/m72/1120x630-2.log) |
| 2240×1260 | 3 · 再开四预览 | 170.708 / 239.708 / 273.708 | 11 / 22 | 143568 | 35400 / 6630 | [CSV](data/m72/1120x630-3.csv) · [日志](data/m72/1120x630-3.log) |
| 2240×1260 | 4 · 再请求 GPU 计时 | 175.542 / 256.625 / 302.583 | 11 / 22 | 143568 | 37200 / 6918 | [CSV](data/m72/1120x630-4.csv) · [日志](data/m72/1120x630-4.log) |

两档尺寸中，单开面板增加 2 个实际 GUI Draw。配置 2 的 GUI build 和 render 中位数分别为 7.334 / 6.667 µs、8.125 / 6.708 µs。配置 3 在同一 Bloom 生产链上增加 4 次转换，转换段中位数分别为 26.875 和 28.875 µs。带缩略图的两个窗口共提交 12 个 GUI Draw。世界批次始终为 1 Draw，World Sprite 中位数均为 238。HUD 的统计数字和面板文字影响各自的几何 payload。

1600×900 的四个 HDR 附件合计 20,160,000 B，2240×1260 为 39,513,600 B。四张预览增加 589,824 B，已创建的字体 atlas 在测量窗口为 262,144 B。默认 framebuffer 和驱动额外存储未知。

本机查询得到 0 个有效 timestamp 位。配置 4 全程及采样窗口的 requested、completed、pending、skipped、invalid 均为 0，各阶段 GPU 有效样本均为 0。配置 4 的界面绘制能力诊断，CSV 区分 unavailable 与 off。配置 3→4 的 CPU 差值包含界面变化和运行波动。实际发起 GPU query 的设备成本仍待测量。另用无效截图路径验证了非零退出、部分帧信息与计数报告。[错误退出记录](data/m72/failed-capture.log)

## 待人工试玩与下一阶段接口

M7.1 用户反馈交互和视觉无异常，PR #11 已合并。本次 M7.2 的自动注入覆盖不替代人的键鼠手感与视觉验收。请在常用窗口尺寸下拖动/折叠面板，暂停后单步跟随 Bolt，观察槽位复用，再以不同 backing scale 的显示器验证跨屏字体、点击与最小化恢复。本次机器仅覆盖 Apple M5 Pro、arm64、macOS 26.7.3、同一屏幕的两个 framebuffer 尺寸。支持非零 timestamp bits 的另一台真实设备仍需复核 GPU 时间与帧延迟。

M7.3 可在 Controls 命令中增加经过验证的光照参数，在 Pipeline 的需求判断中加入 Light Buffer，复用带 generation 的纹理借用、显示转换与 timestamp 区间。新增 Pass 时一起扩展名称、mask、区间数、CSV 和像素 fixture。本 PR 停在 Inspector 的源码 Review 和人工试玩。

## Review Fix

本次修复从 PR #12 的 `1c73ed8ff0d4485499491c50651b8afe31034015` 开始。以下补测独立保存，上一节的八组性能数据仍对应原测量提交。

### 焦点恢复与输入队列

GLFW 可在一次 poll 中派发多组按下、释放和焦点事件。官方后端把事件交给 ImGui，默认的事件队列可能要经过多次 NewFrame 才能消费完。旧实现只封住恢复第一帧观察到的键，第二帧就可能把上个焦点周期的 R 当作一次新命令。真实后端定向测试将 W、R、P、B、F2 连同失焦和恢复放在同一批次。旧代码在随后 16 帧执行了 1 次 Reset，另有 14 帧处于暂停、13 帧关闭 Bloom、12 帧隐藏 Inspector。[修复前记录](data/m72/review-fix/focus-before.log)

`Inspector::focusChanged` 在 GLFW 派发失焦和得焦时清除 ImGui 的待消费事件与已采样按键、鼠标状态，再转发一次官方回调。失焦前已按下的控件先取消按压，恢复时重新取得静止光标的逻辑坐标。`Controls::poll` 仍丢弃失焦期间累积的时间。后端能确认按键来自恢复后的新周期时，同一轮 poll 内的真实新按下也可立即生效，文本框继续按 `WantCaptureKeyboard` 和活动控件状态捕获输入。

macOS 的 GLFW 3.5.1 在通知失焦后为已按下的键合成释放。若某键实际仍按住，Cocoa 的下一次自动重复可能被 GLFW 标为首次 PRESS。Lab 只为失焦时仍按下的键保留标记。遇到对应的第一次 PRESS 时，用 AppKit 的窗口、scancode 和 `NSEvent.isARepeat` 核对，恢复重复事件的含义，再交给官方后端。失焦期间释放、恢复后重新按下的键直接生效。该检查发生在按键回调中，AppKit 是 macOS 系统框架。[原生重复的定向复现](data/m72/review-fix/focus-native-before.log)

逐步修复时分别保留了两次中间失败记录。仅清 ImGui 队列时，Cocoa 自动重复仍触发一次 Reset。恢复光标坐标后，已按下的 Bloom 控件会把清空 MouseDown 当作释放。最终代码先取消活动控件，再清输入状态。[控件误释放复现](data/m72/review-fix/focus-widget-before.log)

修复后，同一批 W/R/P/B/F2 在 16 帧内造成的误操作均为 0。恢复同轮新按 R 恰好 Reset 一次，跨两轮焦点切换的原生自动重复产生 0 次 Reset。按住 Bloom 控件切窗保留原值，恢复后不移动鼠标即可正常重新点击。测试通过本进程的 AppKit 队列和实际安装的 GLFW/ImGui 回调注入事件，保留了文本捕获、持键关闭面板、拖出释放与暂停单步断言。[修复后记录](data/m72/review-fix/focus-after.log) · [连续七次运行](data/m72/review-fix/focus-repeat.log)

### GPU 最新帧与历史窗口

查询槽按物理编号遍历。较低编号的槽可能已经复用给新帧，较高编号的槽还在等待旧帧。基线测试实际得到回收顺序 1、3、2，直接追加使 Timing 显示的最新帧从 3 退回 2。[修复前记录](data/m72/review-fix/history-before.log)

`History::recordGpu` 在进入显示窗口时校验发起 epoch 和完成状态。通常的顺序结果直接插到末尾，迟到的结果按发起帧 ID 用二分查找定位插入。超过 120 条时从队首淘汰。面板原有的末项读取因此总能得到当前 epoch 已完成的最大帧 ID。CSV 按实际回收事件写出，保留发起帧、Tick、epoch 和回收帧。查询池仍按每个请求的实际结果计数。

用 fake timestamp 后端驱动生产查询池和历史更新路径。单次回收 1、3、2 后 Latest 为 3，分两次回收 5、4 后 Latest 保持 5。Reset 后旧 epoch 的帧 6 仍被回收，当前 Latest 保持新 epoch 的帧 7。最后才返回帧 10 时，窗口仍完整保留 14–133。133 个请求对应 131 completed、0 pending、1 skipped、1 invalid，CSV 有 132 条唯一回收记录，各行均核对了原 Tick、epoch 与状态。[修复后记录](data/m72/review-fix/history-after.log)

### 复跑本次回归

在仓库根目录执行。两项回归在已有的 M72 测试目标中运行，`M72InspectorGL` 需要可用的 macOS 图形登录会话。

```sh
cmake -S . -B build/m72-review-debug -DCMAKE_BUILD_TYPE=Debug -DTINY2D_OPENGL_TESTS=ON
cmake -S . -B build/m72-review-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake -S . -B build/m72-review-off -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=OFF
for cfg in debug release off; do cmake --build "build/m72-review-$cfg" --parallel 4; done
ctest --test-dir build/m72-review-debug -R '^(M72CPU|M72InspectorGL)$' --repeat until-fail:7 --output-on-failure
for cfg in debug release off; do ctest --test-dir "build/m72-review-$cfg" --output-on-failure; done
```

| CTest 配置 | 通过 / 总数 | 失败 |
|---|---:|---:|
| [Debug，OpenGL ON](data/m72/review-fix/ctest-debug.log) | 45 / 45 | 0 |
| [Release，OpenGL ON](data/m72/review-fix/ctest-release.log) | 45 / 45 | 0 |
| [Release，OpenGL OFF](data/m72/review-fix/ctest-off.log) | 25 / 25 | 0 |

最终 Debug 构建的两项 M72 定向测试各连续通过 7 次。Release 在 seed 71、Tick 4、工具全关时分别生成 Scene、Bright、Blur、Final，四张 PNG 均与 M7.1 文件逐字节相同。Neon 带四预览的有限帧完成 13 帧 / 13 Tick。按各自的进程 ID 确认交互 Neon 与独立经典 Survivor 有可见窗口。原 Sandbox 的三角形、500 immediate、500 batch、5000 multitexture 最终帧分别为 1 / 500 / 1 / 5 Draw。[定向重复](data/m72/review-fix/targeted-debug.log) · [入口、PNG 校验和与窗口记录](data/m72/review-fix/entries.log)

### 受影响配置的 Release 对照

同一台 Apple M5 Pro，macOS 26.7.3、Apple clang 21.0.0、OpenGL 4.1 Metal 90.5。修复前取自 `1c73ed8` 的源码归档，修复后取自本次提交的生产代码，日志记录两份可执行文件的 SHA-256。两边均取 seed 71、Final、Bloom、HUD、Inspector、四项预览并请求 GPU 计时。实际 framebuffer 为 1600×900，1–120 帧预热，121–1320 帧计入最近秩分位数。[运行命令和完整输出](data/m72/review-fix/frame-cost.log)

| 模式 | 修复前 CPU 帧 p50 / p95 / p99（µs） | 修复后 CPU 帧 p50 / p95 / p99（µs） |
|---|---:|---:|
| 交互，无键鼠操作 | 265.208 / 543.041 / 604.500 | 206.625 / 258.667 / 282.792 |
| 有限帧，固定 1320 Tick | 159.333 / 204.500 / 244.459 | 158.666 / 195.833 / 226.167 |

四组各有 1200 个 CPU 样本，所有样本的实际 Draw 都为 22，其中 GUI 12、预览转换 4，Pass mask 为 447。有限帧两侧的采样 Tick 完全对齐。交互侧按真实时间推进，采样 Tick 分别为 9–465 和 63–662，单轮分位数差异不足以归因提速。原生重复判定只在焦点恢复后的候选按键回调执行。GPU 查询仍使用八槽非阻塞池，实机返回 0 个 timestamp 有效位，两边均报告 unavailable、0 个 GPU 样本。可离线重算全部帧分位数。[原始采样](data/m72/review-fix/frame-samples) · [重算脚本](data/m72/review-fix/compare.py)

另外单测 CPU 侧历史更新，每 32 条有一条迟到时，每条均摊 p50 / p95 / p99 从 12.04 / 13.59 / 14.77 变为 7.57 / 7.89 / 8.63 ns。每个样本汇总 1024 次插入，预热 64 组，测量 512 组。计时只包围历史容器更新，GPU 驱动回收和面板绘制在区间之外。[测试源文件](data/m72/review-fix/history-cost.cpp) · [命令和记录](data/m72/review-fix/history-cost.log)

人工验收仍需实际键鼠跨应用、macOS 最小化恢复，以及在不同 backing scale 的屏幕间移动窗口后检查拾取和字体。在 timestamp 有效位非零的设备上，还需观察真实 GPU 样本、帧延迟及 Reset 后的显示。
