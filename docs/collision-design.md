# Collision2D — M5

纯 CPU AABB、Circle 和 variant Shape。所有输入 finite，AABB min ≤ max，radius ≥ 0；非法输入抛带诊断的 invalid_argument。闭集接触计为碰撞，包括点圆、线段/点 AABB。

- AABB/AABB：两个轴的闭区间均相交。
- Circle/Circle：中心差平方和 ≤ 半径和平方。
- AABB/Circle：中心 clamp 到矩形后，与最近点距离平方 ≤ 半径平方。
- Circle/AABB：对称包装。

距离运算先转 double 再相减/求和/平方，避免有限 float 极值的中间溢出。worldShape 将本地形状坐标（包括 Offset）加对象 position；平移结果超出 float 范围即拒绝。视觉 scale/rotation 不改变碰撞尺寸，非均匀缩放也不会将圆隐式变成椭圆。不实现 OBB、SAT、连续碰撞、刚体求解或弹开。

Scene 每固定步：对象快照 → 用户运动逻辑 → velocity 积分 → 世界形状 → O(N²) 两两检测 → 当前/上一集合比较 → 分发 → 清理延迟删除。Pair 用完整 ID 规范排序，集合消除重复。先按 pair 排序发送 Enter/Stay，再按 pair 排序发送 Exit；没有 unordered 容器造成的事件顺序变化。对象删除立即清除其 contact，取消后续事件，无 tombstone Exit。事件仅保存 ID，不保存对象地址；回调访问仍应 find。

形状列表只包含 enabled 且有 collider 的对象。回调创建对象从下一次 update 参与；回调移动或禁用对象在下一次检测生效。删除则立即失效。失败不提供整步事务回滚。

空间复杂度 O(N + C)，C 最坏 O(N²)；朴素检测适合当前几十个对象，不能视作大规模碰撞性能方案。后续应先测量，再评估 spatial hash，而非在 M5 引入复杂索引。
