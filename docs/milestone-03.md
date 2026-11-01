# M3 — Texture2D、SpriteRenderer、Camera2D

## 架构与接口

- `ImageLoader::loadPNG(path)` 使用本机 libpng simplified API，返回 CPU `ImageData`（宽高与连续 RGBA8）。文件缺失、非法/截断数据、零尺寸、超限尺寸均抛出包含路径的异常。尺寸上限 16384、像素总量上限 64M；使用 uint64_t 计算并检查 size_t 溢出。解码器在异常路径也释放资源。
- `Texture2D(ImageData, TextureFilter)` 使用 glTexImage2D 上传 GL_RGBA8；支持 Linear/Nearest，S/T 均 CLAMP_TO_EDGE，无 mipmap。`bind(unit)` 检查纹理单元范围，`width/height/nativeHandle` 提供查询。
- `Mat4` 提供 identity/translation/scale/rotationZ/orthographic/乘法。
- `Camera2D` 提供 setPosition/move/setZoom/setViewportSize/viewProjectionMatrix，完全不依赖 GPU。
- `SpriteRenderer::beginFrame(camera)` 快照 VP 并清零计数；`draw(texture,position,size,radians,tint)` 每次一次 draw；`drawCalls()` 返回本帧计数。每个 renderer 仅创建一套单位四边形 VBO/VAO。
- ShaderProgram 新增 setInt/setMat4/setVec4，沿用绑定状态、当前线程/Context 与无效 uniform 检查。

## 数学与像素约定

列主序存储、列向量，上传矩阵时 transpose=GL_FALSE。
X 右、Y 下；position 为未旋转矩形左上角，角度为弧度，正角在屏幕上顺时针。

```
Model = T(position + size/2) * Rz(radians) * S(size) * T(-0.5,-0.5)
VP = Ortho(-w/2,w/2,-h/2,h/2) * S(zoom) * T(-cameraCenter)
clip = VP * Model * localVertex
```

相机中心始终映射到窗口中心，zoom>1 放大。viewport 使用逻辑窗口尺寸；Window 的 glViewport 使用 framebuffer 像素尺寸。最小化时跳过绘制/相机尺寸更新，保留上次合法投影。zoom 支持 [0.0001,10000]，viewport 支持 [1,1000000]，拒绝 NaN/Inf。

PNG 按文件顶部行先存储。上传不翻转；单位四边形的屏幕顶部使用 v=0，因此 top-left UV 对应 PNG 顶部。测试 PNG 有透明边框、半透明区域和不同上下颜色。

## 调用链与状态

初始化：可执行文件路径 → assets/textures/sprite.png → ImageLoader → ImageData → Texture2D → GPU。
每帧：输入/deltaTime → 相机及旋转动画 → beginFrame → Model/VP/tint/sampler uniforms → texture.bind(0) → 共享 VAO → glDrawArrays(GL_TRIANGLES,0,6)。

Straight Alpha：采样颜色乘 tint，renderer 在每次 draw 设置 BLEND enabled、FUNC_ADD、SRC_ALPHA/ONE_MINUS_SRC_ALPHA。状态保留给后续绘制，不自动恢复；调用方管理 depth/scissor/cull。透明物体遵循提交顺序。该 blendFunc 的目标 alpha 也是同一因子，离屏透明合成若需要独立 alpha 方程应在后续明确扩展。

Texture 构造恢复原 texture binding、UNPACK_ALIGNMENT、ROW_LENGTH、SKIP_ROWS、SKIP_PIXELS 与 unpack PBO；bind 显式改变 active unit 与该单元绑定。纹理只在初始化上传。

## 生命周期

Texture 扩展已有 GLObject::Texture，禁止复制、支持 noexcept 移动；移动后源 handle=0。构造失败（包括返回句柄同时报错）自动释放。每次使用与析构都保留 M2 的主线程/创建 Context 检查。违反析构契约仍 terminate，不隐藏错误。

Sandbox 派生类 GPU 成员先于 Application 的 Window 析构；renderer 内 VAO 先于 VBO，最后 program。初始化或 render 抛异常时同样遵循逆序展开。CPU ImageData 可在上传后释放。Renderer 禁止复制且不提供移动，避免 VAO/VBO 借用关系被错误重排。

## 依赖与版权

使用本地安装的 libpng（本次 1.6.59），无 stb_image、无配置阶段下载。来源：https://www.libpng.org/pub/png/libpng.html ，源代码：https://github.com/pnggroup/libpng 。许可证为 PNG Reference Library License version 2（本地 png.h 的完整版权声明随依赖提供）。安装：`brew install glfw libpng`。

测试图是本项目生成的原创程序图案，CC0 1.0，见 sandbox/assets/textures/LICENSE.txt。运行路径相对可执行文件，与 cwd 无关；CMake 每次构建复制整个 assets 目录。

## 验证及限制

见 m3-review-notes.md 和 performance-baseline.md。未实现 ECS、资源缓存、批处理、索引渲染、sRGB/色彩管理、异步加载与多线程 OpenGL。保留 M2 三角形资源初始化便于双场景回归，后续可拆分 scene 对象以减少未使用资源。

面试要点：CPU 解码与 GPU 所有权解耦；Context 生命周期比单纯 RAII 更重要；逻辑坐标与 Retina framebuffer 分离；非交换矩阵乘法；透明顺序限制批次重排；Draw Call 数量不能替代 GPU 时间测量。
