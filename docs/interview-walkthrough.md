# Interview walkthrough

建议先用两分钟讲 CPU/GPU 边界，再跟随一次输入、固定更新、事件消费和渲染提交。
源码链接均相对仓库；性能结论需带 fixture 与指标。

## 1. 整体架构

**可能提问：** 如何把引擎和游戏组织在一起？

**核心回答：** Application/Window 管理平台循环；SurvivorDemo 协调 Flow、GameSession 和 presentation。GameSession 不链接渲染实现，HUD/VFX 通过数据边界参与显示。

**源码：** [sandbox/src/SurvivorDemo.cpp](../sandbox/src/SurvivorDemo.cpp)

**追问：** 为何不让 GameSession 直接调用 renderer？

**避免：** 不要说 FlowController 拥有 GPU。

## 2. CPU/GPU 分工

**可能提问：** 哪些工作在 CPU，哪些在 GPU？

**核心回答：** CPU 处理输入、固定 Tick、碰撞、顶点生成和提交；GPU 执行顶点变换、采样与混合。驱动提交耗时可能包含等待，不能只按函数名称分类性能。

**源码：** [sandbox/src/SurvivorPresentation.h](../sandbox/src/SurvivorPresentation.h)

**追问：** glFinish 测量的是什么？

**避免：** 不要把等待时间称为纯 GPU 执行时间。

## 3. 渲染管线

**可能提问：** 一个四边形怎样到屏幕？

**核心回答：** CPU 计算四个顶点及 UV、颜色和纹理槽；VAO 描述布局，Shader 应用相机矩阵，索引绘制三角形，片元采样并混合。Framebuffer 像素大小用于 viewport。

**源码：** [engine/src/Renderer/BatchRenderer.cpp](../engine/src/Renderer/BatchRenderer.cpp)

**追问：** 逻辑窗口尺寸与 Retina framebuffer 区别？

**避免：** 不要声称实现 Metal 后端；系统驱动名称不是引擎后端。

## 4. 减少 Draw Calls

**可能提问：** 批处理具体省在哪里？

**核心回答：** 在容量和纹理槽允许范围内积累顶点，一次提交多个 Quad；500 Sprite fixture 从 500 draws 到 1。达到容量或纹理槽上限会 flush。

**源码：** [engine/src/Renderer/BatchRenderer.cpp](../engine/src/Renderer/BatchRenderer.cpp)

**追问：** 为何 5000 Sprite 是 5 draws？

**避免：** 不要将 99.8% draw reduction 等同 FPS 提升。

## 5. VBO/EBO

**可能提问：** 为什么一个动态一个静态？

**核心回答：** 位置、UV、颜色会随每帧变化，上传 VBO；Quad 的两三角形拓扑固定，因此预生成 EBO 并重复使用，减少重复索引上传。

**源码：** [engine/src/Renderer/BatchRenderer.cpp](../engine/src/Renderer/BatchRenderer.cpp)

**追问：** 如何处理容量溢出和上传字节统计？

**避免：** 不要说静态 EBO 使所有 GPU 数据都不再上传。

## 6. 纹理槽

**可能提问：** 为什么不支持无限纹理？

**核心回答：** 硬件纹理单元和 shader sampler 数量有限。实现允许 1..8，并查询设备限制；不同纹理超过可用槽会先 flush，再重建槽映射。

**源码：** [engine/src/Renderer/BatchRenderer.cpp](../engine/src/Renderer/BatchRenderer.cpp)

**追问：** 九种纹理如何测试跨槽正确性？

**避免：** 不要说任意数量纹理都能一个 draw 完成。

## 7. ObjectId

**可能提问：** 销毁对象后旧 ID 为什么失效？

**核心回答：** 句柄包含 Scene 身份、槽位与 generation；重用槽位时 generation 改变，find 拒绝旧句柄和其他 Scene 的句柄，避免悬空指针误引用新对象。

**源码：** [engine/src/Scene/Scene.cpp](../engine/src/Scene/Scene.cpp)

**追问：** generation 耗尽或 Scene 迁移有什么约束？

**避免：** 不要称其为线程安全或垃圾回收。

## 8. 固定 Tick

**可能提问：** 帧率变化时如何保持规则稳定？

**核心回答：** 输入被消费到 60 Hz fixedUpdate，accumulator 计算步骤数；每次最多补六步并丢弃超额时间，焦点丢失/重启重置计时。同样 tick 输入可重现规则。

**源码：** [engine/include/tiny2d/FixedTimestep.h](../engine/include/tiny2d/FixedTimestep.h)

**追问：** 为什么不无限追赶？浮点跨机器是否 bit-exact？

**避免：** 不要承诺不同平台完全相同的二进制浮点结果。

## 9. 碰撞

**可能提问：** 碰撞算法与顺序是什么？

**核心回答：** Collision2D 支持 AABB/Circle 和混合形状，边界接触算相交；Scene 生成有序碰撞事件，GameSession 消费并按既定规则结算命中和销毁。

**源码：** [engine/src/Physics/Collision2D.cpp](../engine/src/Physics/Collision2D.cpp)

**追问：** 密集对象下如何分析 pair checks？

**避免：** 不要宣称已有空间树、连续碰撞或完整刚体求解。

## 10. 事件解耦

**可能提问：** VFX 开关为什么不改变游戏？

**核心回答：** GameSession 发出带序号、位置等值的 CombatEvent，VFX 用独立 cursor 消费并更新固定容量效果。它没有反向修改伤害、生成或射击的接口；测试对比规则状态。

**源码：** [sandbox/src/vfx/CombatVFX.cpp](../sandbox/src/vfx/CombatVFX.cpp)

**追问：** ring overflow 和 Death/Hit/Trail 饱和怎么办？

**避免：** 不要称视觉事件队列为无限可靠消息系统。

## 11. Context/资源

**可能提问：** RAII 是否足够保证 GL 安全？

**核心回答：** 不够。GPU 对象还需要正确线程与当前 owning context。派生 Survivor 成员先销毁，基类 Application 的 Window 后销毁；SceneRenderer 持有排队纹理直到结束或取消。

**源码：** [engine/src/Renderer/SceneRenderer.cpp](../engine/src/Renderer/SceneRenderer.cpp)

**追问：** 异常中错误 context 能否安全取消？

**避免：** 不要说析构总能自动修复错误 context。

## 12. 测试策略

**可能提问：** 无图形环境如何测游戏？

**核心回答：** tiny2d_game 独立包含 CPU translation units；Headless 关闭图形测试但仍构建平台目标。Debug/Release 跑真实 context 像素、状态、寿命及异常恢复回归。

**源码：** [CMakeLists.txt](../CMakeLists.txt)

**追问：** Headless 是否表示配置完全不需要 GLFW？

**避免：** 不要把没执行的 GPU 测试记成通过。

## 13. 性能测量

**可能提问：** 怎样确定瓶颈，而非只看 FPS？

**核心回答：** 分离 fixedUpdate、VFX/HUD 准备、world/UI CPU submission 与 glFinish 等待，保留 p50/p95/p99、draw/flush/bind/upload 原始记录。固定 fixture、warmup 和重复次序帮助比较。

**源码：** [tests/benchmark_m63b1.cpp](../tests/benchmark_m63b1.cpp)

**追问：** 为何满负载 CPU 成本增加？如何排除 driver noise？

**避免：** 不要将 offscreen reciprocal throughput 当作交互 FPS。

## 14. 限制与下一步

**可能提问：** 这个引擎离产品还有什么距离？

**核心回答：** macOS 单平台、单线程 GL、有限碰撞与固定容量效果；未证明跨系统兼容、Intel 支持、完整泄漏安全或公开发行签名。当前先完成 RC 人工验收和许可证选择。

**源码：** [docs/milestone-06-3c.md](../docs/milestone-06-3c.md)

**追问：** 如果有时间会先测哪个瓶颈或兼容性？

**避免：** 不要称为完整商业引擎，也不要擅自承诺 M7。
