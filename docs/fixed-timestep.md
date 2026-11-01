# Fixed timestep — M5

FixedTimestep 是独立无 OpenGL、无时钟依赖的 CPU 累积器。advance(delta) 返回要执行的步数，调用者每次传入常量 step=1/60 秒。首次 accumulator=0，不凭空模拟第一步；不足一步返回零。double 累积和 1e-12 个步长的边界容差减少等价时间分割误差，余量归零钳制避免微小负值。

每渲染帧输入最多接受 6 步（0.1 秒），更长部分计入 dropped；旧小数余量保留，最多执行 6 步。负值、NaN、Inf 拒绝且不修改状态。Application 原有 0.1 秒上限不变：正常交互路径不会再次减少已接受时间，dropped 仅描述本类收到的时间，不包括 Application 先丢弃的时间。

Arena 复用 Application 主循环：poll → variable update 中采样输入并 advance → N 次 fixed（生命周期、运动、碰撞、反馈）→ camera → render → swap。旧 Sandbox 不接入累积器。固定逻辑速度：玩家 180 world units/s，对角输入归一化；敌人 65 units/s。窗口 framebuffer 为零时 reset 累积器并暂停模拟，恢复时不追赶；渲染跳过提交。投影采用窗口逻辑尺寸，viewport 继续由 Window framebuffer 回调管理。

--frames N 的 Arena 是确定性 smoke 模式：每显示帧输入恰好一步，包括第一帧，保证快速运行也能覆盖生成删除。正常无限帧模式使用真实时钟 dt。固定初始布局，无随机源。输入、初始状态和步序列相同，单一构建/平台可复现；不保证跨平台浮点逐位确定性。没有插值：渲染使用最近模拟状态，120Hz 显示可能重复相同状态。严重长帧会有意丢失模拟时间。
