# Tiny2D 工程面试讲解

两分钟开场可以从一帧 Survivor 讲起。输入进入 60 Hz 的 GameSession，弹丸命中生成带序号和位置的 CombatEvent，CombatVFX 消费事件并准备世界图元，Classic 或 Enhanced 渲染同一世界。接着打开 Timeline，在同一个 Tick 600 展示两次不同选择；这能解释渲染帧、模拟 Tick、历史位置和运行实例之间的区别。[整体架构](project-architecture-overview.md) · [实测战斗对照](milestone-11-1.md) · [时间轴对照](milestone-10-2.md)

## 渲染

### 1. OpenGL 渲染管线 · 一个 Sprite 怎样变成屏幕像素？

**原理与实现。** CPU 将四个角、UV、颜色和纹理槽写入顶点流。顶点着色器应用相机矩阵，索引定义两个三角形，光栅化产生片元，片元着色器采样纹理，混合写入当前 framebuffer。HUD 使用独立的屏幕空间相机。[批渲染器](../engine/src/Renderer/BatchRenderer.cpp) · [Survivor 绘制入口](../sandbox/src/SurvivorPresentation.h)

**选择与边界。** 世界相机按逻辑尺寸工作，viewport 和附件使用真实像素。两者独立支持 Retina 和 UI 固定大小；零尺寸窗口要挂起绘制。**验证。** 放大窗口后检查实际 framebuffer 读回、GL 状态恢复和角色位置。[Sprite 像素测试](../tests/test_m3_gpu.cpp) · [恢复与 Resize](../tests/m101_replay_gl.cpp)

**面试简答。** “我会先查本帧使用的 framebuffer、viewport 和相机，再沿 VAO、program、纹理和混合检查失真的像素。” **追问。** 同一难度下，为什么移动世界相机时 HUD 仍在原位？

### 2. Batch Rendering · 500 个透明 Sprite 如何合批？

**原理与实现。** 相同管线状态的 Quad 按提交顺序组成一个顶点流。静态索引复用 Quad 拓扑，动态顶点含纹理槽索引。当前纹理已入槽就复用；容量或槽用尽时先 flush。[提交与 flush](../engine/src/Renderer/BatchRenderer.cpp)

**选择与边界。** 保留透明叠加顺序，代价是跨容量、纹理和通道边界会增加 Draw。**验证。** 单纹理 500 Sprite 使用 Immediate 时有 500 Draw，同条件 Batch 为 1。M4.1 的 489.92 与 90.67 µs 是指定工作负载的 CPU 提交 p50。测试还比较跨槽透明合成的整帧像素。[基准](performance-m41.md) · [跨槽校验](../tests/test_m41_gpu.cpp)

**面试简答。** “合批主要减少重复绑定和提交；我同时数 Draw、Flush、上传字节，并用相同相机比较像素。” **追问。** 使用第九种纹理时，如何在保持透明顺序的同时继续绘制？

### 3. GPU Instancing · 8,192 个粒子为什么只需一次绘制？

**原理与实现。** 四角几何固定，两个 `vec4` 实例属性携带位置、大小、年龄比例、颜色和发光量。属性 divisor 为 1，GPU 每实例读取一条记录；CPU 以活动槽序准备数组，上传后调用 `glDrawArraysInstanced`。[粒子上传与 Draw](../lab/src/ParticleRenderer.cpp)

**选择与边界。** CPU 模拟可直接验证力场与确定性。满载每帧的实例记录为 8,192 × 32 = 262,144 B，力场更新及透明片元覆盖仍有成本。**验证。** 真实展厅的 8,192 个活动粒子对应 8,192 个实例和 1 个粒子 Draw；GL 测试另外检查增益、空载和状态恢复。[展厅原始行](data/m111/frames.csv) · [GL 测试](../tests/test_m81_gl.cpp)

**面试简答。** “实例化复用角点和着色器，只流式上传每粒子不同的数据。” **追问。** 如果 Draw 已经为 1，下一步应分别测哪些 CPU、上传或片元开销？

### 4. VAO / VBO / EBO · 三种对象各自保存什么？

**原理与实现。** VAO 记录属性描述及关联的索引缓冲，VBO 存顶点，EBO 存索引。Batch 的静态 EBO 每 Quad 写六个 u32 索引，顶点 VBO 预留容量并在批次上传；粒子使用静态四角 triangle strip 和独立实例 VBO。[Quad 布局](../engine/src/Renderer/BatchRenderer.cpp) · [实例属性](../lab/src/ParticleRenderer.cpp)

**选择与边界。** 重复索引常驻 GPU 可减少每帧传输，动态属性仍需更新。初始化和提交均要恢复借用的 VAO/绑定，异常时也要遵守所属 Context。**验证。** 记录调用前后的非默认绑定，再读回绘制结果；构造容量异常时核对现存对象。[Batch GL](../tests/test_m4_gpu.cpp) · [粒子 GL](../tests/test_m81_gl.cpp)

**面试简答。** “我把拓扑和属性分开：固定索引只建一次，颜色、坐标和槽号按批更新，VAO 固定解释方式。” **追问。** 为什么在错误的 VAO 上绑定 EBO 会改变后续绘制？

### 5. Shader 编译链接 · F5 输入了坏源码会怎样？

**原理与实现。** 主线程在所属 Context 编译临时 stages，链接候选 program，读取驱动日志并反射活动 uniforms、输入和输出接口。Gallery 需要符合契约的 `uTime` 与 `uResolution`。完成全部验证后再安装并递增版本。[ShaderProgram](../engine/src/Renderer/ShaderProgram.cpp) · [ReloadManager](../lab/src/ShaderReloadManager.cpp)

**选择与边界。** 手动事务式重载支持在暂停的同一 Tick 对照修改，程序编译仍由驱动执行。旧程序若当前已绑定，成功替换时要迁移绑定。候选包把可编辑源码放在用户可写目录。**验证。** 编译、链接、类型、数组、额外 sampler 或文件错误均留下旧 HDR 像素；修复后版本和画面一起变化。[测试](../tests/test_m82_gl.cpp) · [实际失败与恢复](milestone-11-1.md)

**面试简答。** “能链接只是第一关。我还校验应用需要的接口，候选完全可用后才替换，用户始终有上一版画面。” **追问。** 修改已被编译器优化掉的 uniform，会出现怎样的接口验证结果？

### 6. RAII 与 Context 生命周期 · move-only 为什么仍需要 Context 检查？

**原理与实现。** GLObject 记录名称、类型和所属 GLFW Context；move 转移所有权并清空源对象，析构按类型删除资源。Window 的生命期覆盖 GPU 成员，主线程使用资源前检查当前 Context。[GLObject](../engine/src/Renderer/GLObject.cpp) · [窗口适配器](../sandbox/src/SurvivorDemo.cpp)

**选择与边界。** 析构时资源必须仍有可用的所属 Context；实现对违反生命周期契约的析构显式终止。渲染前可预检并抛错，已提交到 GPU 的绘制无法回滚。**验证。** 建第二个 Context 调用旧 Pipeline，验证拒绝且原纹理仍有效；回原 Context 销毁后检查纹理已释放。[负向与资源测试](../tests/m101_replay_gl.cpp) · [异常恢复](../tests/test_m63b_render_recovery.cpp)

**面试简答。** “RAII 决定谁释放，Context 决定在哪里释放，这两种所有权都要能验证。” **追问。** 如何处理仍被当前 program 绑定引用的对象？

### 7. HDR / Bloom / Tone Mapping · 为什么把 HUD 放在 Bloom 后？

**原理与实现。** RGBA16F 保留大于 1 的线性亮度。Bright 根据 luminance 超过 1 的能量提取，半分辨率下先横向后纵向高斯模糊，输出叠加原图和模糊图后应用 `x/(1+x)` 与分段 sRGB 编码。[提取](../lab/assets/shaders/bright.frag) · [模糊](../lab/assets/shaders/blur.frag) · [映射](../lab/assets/shaders/display.frag)

**选择与边界。** 半分辨率和可分离卷积减少采样量；目标数量随分辨率增加，Resize 需遵守内存预算。HUD 在显示映射后绘制，保留逻辑尺寸和文字可读性。**验证。** 读回中间 HDR 附件和最终像素；Survivor 的同一击杀 Tick 对比 Classic、Enhanced 和 Bloom 关闭画面。[HDR 测试](../tests/test_m71_gpu.cpp) · [Survivor 对照](milestone-11-1.md)

**面试简答。** “先在线性域保存高亮和扩散能量，最后一次映射到显示值，避免文字参与亮部扩散。” **追问。** 用已经裁到 0–1 的颜色做阈值为 1 的亮部提取，会看到什么？

### 8. Normal Mapping · 二维平面怎样表现倒角？

**原理与实现。** 程序材质提供局部表面法线，Normal Pass 写入有符号世界空间法线。光照用像素到点光源的方向与法线的点积决定漫反射，并结合半径衰减。[材质生成](../lab/src/NormalMaterial.cpp) · [光照着色器](../lab/assets/shaders/light.frag)

**选择与边界。** 表面仍是二维平面；增加一个全分辨率缓冲和逐像素计算可提供方向变化。仅在需要时启用，不能用法线图改变轮廓或碰撞。**验证。** 在同一倒角、同一距离从两侧照明，比较法线分量、HDR 亮度方向和参考计算。[GL 像素对照](../tests/test_m74a_gl.cpp) · [材质与附件](milestone-07-4a.md)

**面试简答。** “我测同一点、等距离的左右灯光，这样观测到的差异可以归因于法线方向。” **追问。** 为什么需要明确法线坐标系，直接把贴图 RGB 当作颜色会有什么问题？

### 9. 阴影与遮挡 · 一面墙为什么只挡住某些光源？

**原理与实现。** CPU 以每盏选中的光为原点，根据遮挡边构造、裁剪投影区域；GPU 把区域写入可复用可见度遮罩，再只累加这盏灯未被遮挡的贡献。环境光与其他灯可继续照亮该像素。[几何](../lab/src/Occlusion.cpp) · [遮挡光照](../lab/assets/shaders/shadow_light.frag)

**选择与边界。** 重用遮罩限制附件个数，代价是每个投影灯都有额外几何和提交。当前实现受投影灯/遮挡边预算约束，支持硬阴影。**验证。** 独立 CPU 射线相交结果对照 GL 可见度，再比较红/蓝光源及合成中的能量。[CPU](../tests/test_m74b_cpu.cpp) · [GL](../tests/test_m74b_gl.cpp)

**面试简答。** “遮挡属于光源到该点的路径；我把它乘在单灯贡献上，因此遮罩内仍能看到其他灯和环境光。” **追问。** 移动一盏灯时，哪些遮挡数据值得缓存？

## 模拟与导航

### 10. 固定步长 · 一个渲染帧可能有几个 Tick？

**原理与实现。** 60 Hz 的累积器从真实帧间隔计算到期步数，保留分数余量，每次最多补六步；暂停、失焦、加载及时间轴跳跃会清空累积时间。记录器只捕捉实际消费的步输入。[FixedTimestep](../engine/include/tiny2d/FixedTimestep.h) · [Navigation 输入](../lab/src/NavigationApp.cpp)

**选择与边界。** 有界追赶避免主线程卡顿后无限补算；被丢弃的墙钟时间不会变成模拟 Tick。稳定复现要求同规则与支持的浮点环境。**验证。** 不同 30/60/120 Hz 调度下按相同逻辑 Tick 比较完整游戏状态；暂停和终局的空帧不得产生 Advance。[会话测试](../tests/m92_navigation_session.cpp) · [Dispatcher](../tests/m101_replay.cpp)

**面试简答。** “录一个输入轴被系统消费了几次，比录按键到达的渲染帧更能重现模拟。” **追问。** 上一次渲染后主线程停了三秒，再恢复时应补算多少？

### 11. 连续碰撞 · 如何让角色沿格子墙滑动？

**原理与实现。** Map 玩家采用 AABB，沿 X 扫过可能跨越的格线，将残余位移裁到首个阻挡面；再用已解算的 X 处理 Y。允许盒面恰好接触，越界视为实心。若精确接触点转成 float 后落入墙内，取合法侧的相邻可表示值。[TileCollision](../engine/src/World/TileCollision.cpp)

**选择与边界。** 轴分离适合正交静态 Tile 墙，轴顺序会影响角点结果。Survivor 的 Scene 对移动形状仍进行离散成对碰撞，高速掠过存在穿透限制。**验证。** 测试窄路、高速穿越多个格、贴墙反向和大/小浮点尺度，并与枚举墙面的参考对照。[扫掠测试](../tests/m91_tile_collision.cpp) · [Survivor 规则](combat-rules.md)

**面试简答。** “我检查从原位置到目标位置跨过的墙面，停在最近的合法面上，再解第二个轴。” **追问。** 格子外只检查目标位置为何会漏掉一整堵墙？

### 12. Seeded Map · 如何证明生成地图可达？

**原理与实现。** 整数格上的迭代迷宫生成使用独立 PRNG，在已连通格上开房间，再按预算打开额外连接。出生点和出口取合法地板。生成完成后独立 BFS 核对可达地板数量、出口距离和封闭边界。[MapGenerator](../engine/src/World/MapGenerator.cpp)

**选择与边界。** 固定行优先顺序、邻居顺序和参数上限使生成过程可重现。M10 快照还保存完整 Tiles，并与相同规则重新生成的内容比较；规则升级需要显式版本。**验证。** 默认 Seed 71、64×40 有 1,290 个地板，出口距离 80，行优先指纹 `de25ba54c55fec8b`；另有多尺寸多 Seed 扫描。[生成回归](../tests/m91_map_generation.cpp) · [M9.1](milestone-09-1.md)

**面试简答。** “构造时保持局部连通性，发布时用独立 BFS 检查整图，黄金值用来发现版本漂移。” **追问。** 为什么选一个远处的合法出口，还需要验证它到出生点的实际路径？

### 13. 反向 BFS · 256 个敌人为什么能共用一次搜索？

**原理与实现。** 大小相同的敌人都追同一目标格。在单位权图中从目标出发检查合法入边，将前驱首次赋值为当前距离加一。敌人最多查四个出邻居，走向距离减一的节点；同距按固定顺序打破平局。[图与边](../engine/src/World/NavigationGrid.cpp) · [反向场](../engine/src/World/DistanceField.cpp)

**选择与边界。** 每次重建 O(V+E)，多敌人首步查询 O(敌人数)。目标未换格时复用距离场。不同体积、目标或动态障碍会影响共享条件；敌人已在半程时先走完已承诺路段。**验证。** 独立正向 BFS 对 256 个起点、16 个目标样本得到 4,096/4,096 相同首步。[独立基准](../tests/benchmark_m92_search.cpp) · [数据](data/m111/bfs.csv)

**面试简答。** “把相同终点的多次搜索变成一个到目标的距离场，更新目标时建场，平时每个敌人查四个邻居。” **追问。** 在有向边上，从目标向外简单遍历出边有什么问题？

### 14. 确定性 RNG · 增加一个敌人为什么不该改变首个升级候选？

**原理与实现。** 地图、敌人出生和升级抽取分属独立随机流。出生与升级从 Seed 及独立域常量初始化，使用 SplitMix64 的无符号运算，并以高位乘法把随机字映射到有限范围；稳定候选顺序和抽样次数同样属于规则。[随机流](../game/src/NavigationRandom.h) · [升级抽取](../game/src/UpgradeSystem.cpp)

**选择与边界。** 两条流隔离消费顺序。任意位置恢复需保存当前流状态、已生成的候选及已选历史；只保存初始 Seed 无法直接恢复。浮点运动另有环境约束。**验证。** 比较 0/16 敌人下的首个 Offer 和升级流，检查 14 次黄金选择及封顶时的流消费。[升级测试](../tests/m92_upgrade.cpp) · [会话测试](../tests/m92_navigation_session.cpp)

**面试简答。** “RNG 的状态、候选顺序和消费次数都是隐形输入；我分开持有它们，并为流的结果和推进量做回归。” **追问。** 为什么抽到仅剩一个候选时仍需明确是否消耗一个随机字？

### 15. Snapshot · 在敌人走到路段一半时，怎样直接从这里继续？

**原理与实现。** 捕获一个事件边界的值对象，保存完整地图、玩家上次碰撞、敌人稳定槽位与已承诺的 Waypoint、HP/冷却、升级等级、待选 Offer 和两条 RNG。恢复入口验证地图与规则、碰撞位置和阶段等约束，再用保存的位置、目标格和尺寸重建导航图与距离场。全部成功才安装候选会话。[快照字段](../game/include/tiny2d_game/NavigationSnapshot.h) · [语义验证与安装](../game/src/NavigationSnapshot.cpp)

**选择与边界。** 保存实际 Tile 会增加体积，但能核对 Seed、参数、指纹与生成规则。检查可达性时允许敌人停在相邻格中心之间。完整支持的对象是 NavigationSession。**验证。** 从 Tick 37 的 16 个未结束路段恢复，对照原会话后续每一事件；另测 Tick 600 待选、同 Tick 选择、冷却、终局与损坏状态。[CPU 测试](../tests/m101_snapshot.cpp) · [独立进程实验](milestone-10-1.md)

**面试简答。** “下一次 step 会读到的历史决定都要保存，包括上次选好的路径。可由规则唯一重建的导航场可以丢掉。” **追问。** 如果恢复时用新距离场为所有敌人立即重选下一格，哪类 Snapshot 会先出现差异？

### 16. Serialization · 怎样将 C++ 状态变成可校验的长期文件？

**原理与实现。** 版本 1 的 24 字节文件头携带 Magic、结构版本、规则版本、Payload 长度和 FNV-1a 64 校验值。载荷用固定宽度小端整数，浮点用 memcpy 搬运 binary32 位模式。解码先检查文件预算和剩余字节，再分配变长数组。规范字节也用于状态指纹。[二进制基础](../game/src/BinaryCodec.h) · [Snapshot Codec](../game/src/SnapshotCodec.cpp)

**选择与边界。** Snapshot 上限 1 MiB，Replay 上限 32 MiB、100,000 个事件。格式版本控制读法，规则版本约束模拟含义。校验和覆盖文件损坏，可信来源需要另行认证。保存验证同目录临时文件后，以禁止覆盖的方式发布。**验证。** 用正确校验和包裹非法状态，同时注入非法枚举、非有限浮点、截断、尾部数据，核对失败后的旧状态。[文件发布](../game/src/SnapshotFile.cpp) · [编码测试](../tests/m101_snapshot_codec.cpp) · [逐字段差异](../game/src/SnapshotCompare.cpp)

**面试简答。** “我要先定义字节顺序、范围、版本和完整性，再独立验证这些值能否构成合法的模拟。” **追问。** 为什么对一块 C++ struct 内存直接做 Hash，连同一程序下一次启动都可能得到不同结果？

### 17. Replay · Tick 600 发生三次控制操作，如何指出第一次分歧？

**原理与实现。** Recorder 按成功消费的命令记录 Sequence、前后 Epoch/Tick、载荷及执行后的预期 Hash。Advance、暂停时单步、选择、重置与换图通过同一 Dispatcher 调用生产会话。ReplayPlayer 从 Base Snapshot 开始，逐事件检查前置条件、结果时标、Hash 及文件尾状态。[事件与回放接口](../game/include/tiny2d_game/Replay.h) · [Dispatcher 与验证](../game/src/Replay.cpp)

**选择与边界。** 同一帧若消费三个 Tick 就有三个事件，暂停或等待时没有虚构的推进。Reset 可使 Tick 归零，Sequence 仍递增。每事件完整捕获及 Hash 有实际 CPU 成本。位级保证限定在支持的构建、浮点环境和规则版本。**验证。** 三组独立录制/回放共逐事件对照 1,929 个完整 Snapshot；插入、删减、重排和错误预期 Hash 都有定位测试。[回放 CPU 测试](../tests/m101_replay.cpp) · [磁盘往返](data/m101/cross-process/cross-process.log)

**面试简答。** “我按实际被模拟消费的事件排序。遇到分歧就报告事件类型、Sequence 和前后时标/Hash，停在最后一个验证成功的状态。” **追问。** 用户暂停观看时为何不能调用历史会话的 setPaused？

### 18. Checkpoint · 如何在随机定位时权衡内存和重演成本？

**原理与实现。** Timeline 在验证记录时采样完整规范 Snapshot，索引包含游标、记录 ID 和该位置的预期 Hash。Seek 二分查找目标之前最近的 Checkpoint，恢复候选，再逐事件验证剩余后缀。预算计算实际保留容量，Base 固定保留。[索引结构](../game/include/tiny2d_game/Timeline.h) · [定位与稀疏化](../game/src/Timeline.cpp)

**选择与边界。** 默认间隔 120 个事件，索引预算 64 MiB、4,096 项；接近上限会翻倍采样间隔。预算指保留的索引，完整录制和临时候选另计。**验证。** 在 64×40、16 敌人、2,400 事件的夹具中，对相同 128 个随机目标测得仅 Base 的 Seek p50 为 11.48 ms，间隔 120 时为 0.91 ms，保留 21 项、69,005 B。[基准代码](../tests/benchmark_m102.cpp) · [完整分布](data/m102/quantiles.csv)

**面试简答。** “快照越密，读取后要补跑的事件越短。保留 Base 和完整事件序列以后，即使稀疏化索引也能正确到达每个位置。” **追问。** 为什么刚好 Seek 到一个已经索引的位置仍有固定耗时？

### 19. Time Travel · 时间轴回到升级之前，如何保留两种未来？

**原理与实现。** 游标 k 是完成 k 个事件的位置，0 对应 Base。向后查看通过恢复和向前验证实现。Fork 取当前位置完整快照，新 Recorder 从本地 Sequence 1 录制，子文件嵌入该 Base 与后续事件，并保存父记录、父分支身份和来源游标。[时间轴](../game/src/Timeline.cpp) · [分支封装](../game/src/ReplayBranch.cpp)

**选择与边界。** 父记录保留原来的未来；子文件能在独立进程播放，核实它与声明的父位置一致则需要父文件。Lab 同时展示一条父线和一个子分支。**验证。** Seed 71、父游标 600 和子 Base 字节相同。父游标 601 选 Vitality，HP 6/6；子游标 1 选 Fleet，速度 132，两者 Tick 都是 600。逐条验证父 626、子 26 个事件，并比较导出前后的父文件。[分支测试](../tests/m102_branch.cpp) · [边界值](data/m102/boundaries.csv) · [独立验证](data/m102/standalone.log)

**面试简答。** “我把查看过去和写入未来分成两个操作。查看只换当前显示的位置，分叉才创建一份拥有独立起点和事件编号的记录。” **追问。** 同一个历史 Tick 上的暂停、选择和恢复，应该用哪个字段区分？

### 20. Runtime Identity · 恢复同一张地图后，为什么上一次点击可能已经失效？

**原理与实现。** 历史身份包括地图 Epoch、敌人 slot 和 Offer Serial。记录位置还需要 recording/branch ID 和 cursor。UI 另持本次运行的 revision；加载、定位、分叉后，旧选中对象、待处理控制和晚到的 GPU 结果必须与新实例核对。[帧生命周期](../lab/src/MapApp.cpp) · [时间轴视图](../lab/src/TimelineView.cpp) · [异步历史过滤](../lab/src/Observation.h)

**选择与边界。** 同一份历史恢复后必须能识别当年的合法 Choice，故保留其 Epoch/Serial。运行时 revision 在进程内失效输入和查询，排除在 Snapshot/Hash 之外。Scene 内部另用 owner/slot/generation 防止实体槽回收后的旧句柄命中。[Scene 句柄](../engine/src/Scene/Scene.cpp) **验证。** 覆盖旧选择、同 Epoch 延迟 GPU 样本、Seek 后鼠标或键盘控制及错误恢复。[Replay GL](../tests/m101_replay_gl.cpp) · [Timeline GL](../tests/m102_timeline_gl.cpp)

**面试简答。** “能在过去重放的身份与当前窗口可操作的实例有不同生命周期。历史身份随文件走，显示实例在每次替换时换号。” **追问。** 仅比较地图 Epoch，旧 GPU 查询在什么情况下会错误写入当前 History？

### 21. macOS 打包与依赖闭包 · 怎样证明归档能从仓库外启动？

**原理与实现。** 从指定提交的源归档进行干净的 arm64 Release 构建，将 Showcase、真实 Lab helper 和独立 Survivor 放到两个相邻 app 中。复制所需的动态库与资源，改写加载路径，先签内部 Mach-O 再签 app。验证器枚举每个二进制的架构、最低系统和加载命令，解析各级非系统依赖并核对资源清单。[打包入口](../scripts/package_showcase_macos.py) · [闭包与文件校验](../scripts/verify_showcase_macos.py)

**选择与边界。** 部署目标为 macOS 26.0，签名为 ad-hoc。用户可编辑的着色器放到外部可写工作区，应用自身保持签名时的内容。Finder/Gatekeeper、下载隔离和目标硬件需要分别验收。**验证。** 在仓库外独立解包，从另一个工作目录启动真实展厅及八个展品，读取包内历史并测试可写 shader 副本的失败和恢复。[独立 smoke](../scripts/smoke_showcase_candidate.py) · [此次归档的原生记录](data/m112/smoke/native.log) · [候选验收和限制](releases/v0.6.0-rc1.md)

**面试简答。** “我从 ZIP 解包后的程序开始验证，逐条检查它实际会加载什么，再从仓库外启动它。签名校验、可启动性和用户机器上的安装体验各有自己的证据。” **追问。** 若一个内嵌 dylib 仍引用开发机上 Homebrew 的绝对路径，只看主可执行的依赖为什么会漏掉它？
