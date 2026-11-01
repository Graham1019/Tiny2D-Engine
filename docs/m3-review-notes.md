# M3 Review Notes

## 基线

工作区初始干净；GitHub API 核实 PR #1 OPEN、未合并；M2 分支为 97dbf79aa92fc1dff21433c26c457b17e9d2fc1d，main 为 52587ef22ea2170d614f69cae8520893aa65d95a。从 origin/feat/shader-triangle 创建 feat/texture-sprite-camera；PR base 应为 feat/shader-triangle。

按指定 http.proxy=、HTTP/1.1 执行 fetch 与 ls-remote，网络超时；通过 GitHub API 交叉核实本地 remote-tracking SHA 与服务器一致。未改全局代理、SSL 验证或 main。

## 实际验收

- Debug、Release CMake 构建成功。
- Debug、Release CTest 均 8/8：Vector2、M2/M3 故障注入、M3 CPU、M3 GPU、M2 GPU、triangle 回归、根目录/build 目录 smoke。
- 原沙箱内 GUI 测试无法连接 macOS 服务并超时；通过允许访问真实图形会话的执行环境重新运行通过，无静默 skip。
- GPU 像素验证覆盖上传读回、上下 UV、透明纹素、中心顺时针旋转、straight-alpha、提交顺序、camera zoom/center、resize/framebuffer、纹理移动/析构、错误线程/无 Context 拒绝。
- Fake GL 覆盖创建返回 0、返回句柄同时报错、纹理上传失败、非法像素大小；检查资源回收和 unpack 状态恢复。原 M2 compile/link/VBO/VAO 注入保留。
- CPU 验证矩阵数值、乘法顺序、正交映射、相机移动/缩放/resize、无效参数、PNG 成功/缺失/非法/截断。
- 执行了 `./build/tiny2d_sandbox --scene sprites`（持续运行后由测试进程清理关闭）与 `--scene triangle --frames 4`；500 Sprite smoke 正常退出。

## 待人工确认

当前 UI 工具无法识别独立 GLFW 可执行文件，未完成自动键盘按住/拖拽窗口的目视操作。方向键、Q/E、WASD、Escape 和最小化恢复的完整人工交互仍待确认。resize、相机数学及正常退出已有自动测试；不将它们描述为人工目视验收通过。

## 技术债

每次 draw 查询 uniform 与检查 GL 错误，优先可诊断性；M4 再依据测量缓存 uniform/合并提交。外部 libpng 安装是新增构建依赖。Sprite shader 内嵌引擎，triangle shader 仍是外部资产。无资源管理器、上下文恢复、sRGB 管线、mipmap、纹理 atlas 或批渲染。GL 状态所有权见 milestone-03.md。
