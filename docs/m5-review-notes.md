# M5 review notes

新增六个公共类型头、Scene/Collision2D/SceneRenderer 实现、独立 ArenaDemo、三组 CPU 测试和一组 GPU 集成测试；CMake 新增五项注册（包含 Arena smoke），保留原十项。Application、Window、Input、BatchRenderer、SpriteRenderer、Texture2D、数学实现没有修改。GameObject 为数据类型，无需空 .cpp。

所有权检查：Scene unique_ptr 槽位 + generation；borrowed find 指针不能跨删除；Scene 不含 GPU 所有权；适配层资源表和帧 keep-alive 共享 const Texture2D；对象没有回指适配层，故无引用循环。GPU 测试 weak_ptr + glIsTexture 验证最后 owner 释放和实际 GL 删除；派生资源先于基类 Window 析构。无全局资源缓存。

异常检查：非法几何、非法时间、递归 update、错误 Context、缺少 asset、失效 ID、用户回调异常。Scene 在异常后仍可继续操作，但不提供事务回滚。GPU 析构仍继承原 main-thread/live-owner-context 硬契约；不能在另一个线程销毁 SceneRenderer。CPU-only Scene 可独立于图形线程使用，但不支持并发访问。

验证记录见 milestone-05 和 docs/data/m5。GPU 测试真实执行，不以 headless 替代。Debug 第一次受限沙箱尝试遇到 macOS hiservices 连接错误并超时；终止该次测试后，在获准图形会话完整重跑。

技术债：资源键由调用者分配，缺失键诊断来自 map::at；每次 Scene 遍历会分配快照，create 查找空槽 O(slots)，clear 多次扫描；碰撞 O(N²)，keep-alive 每提交一次增加一份引用，未去重。当前 55 个对象且仅 13 个移动 Collider 的原型可接受，不能宣称大型 ECS 性能。没有专门堆泄漏分析器报告；只有 RAII 与明确资源生命周期测试。人工键盘、拖动 resize、最小化/恢复交互尚待用户验收。未新增 CI、Tag 或 Release。

面试说明：重点是以 CPU Scene + 图形资源适配层划分职责，以代数句柄防止槽位复用别名，以结构快照保证回调修改安全，以帧所有权桥接延迟批提交。固定步保证运动和离散碰撞按统一时间推进，但不等价于网络锁步或连续物理。
