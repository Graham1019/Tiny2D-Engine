# M4 batch rendering design

`BatchRenderer(capacity=1000, textureSlots=8)` owns one program, one VAO, one
stream VBO and one immutable EBO. `RenderStats` contains actual submitted sprites,
draw calls, nonempty flushes, texture bind calls and uploaded vertex bytes.
Immediate rendering remains available through `SpriteRenderer::stats()` and
`drawCalls()`; its static quad upload is construction work, not frame upload.

## Lifecycle and ownership

Call `beginFrame(camera)`, zero or more `draw(texture, position, size, radians,
tint)` / `flush()`, then `endFrame()`. Nested begin and inactive draw/flush/end
throw `logic_error`. Empty flush performs no upload, bind or draw. Begin resets
statistics, including empty frames. Invalid sizes, nonfinite inputs, moved-from
textures and overflowing transformed coordinates are rejected before altering
the queue. Positive sizes and any finite tint are accepted; RGBA8 output clamps.

Texture objects must stay alive until their queued draws are flushed. All GPU
operations, moves that release resources, and destruction require the owning
context current on the main thread. Moved-from BatchRenderer operations throw.
Moving transfers the complete frame/state snapshot. Destruction during an active
frame cancels queued work and restores state; it does not implicitly draw.
Construction failures unwind RAII owners. endFrame failures discard the queue,
restore state and propagate diagnostics, permitting a fresh frame.

Begin/end forms an exclusive render pass. Do not mutate GL state, framebuffer,
or texture contents inside it. Before changing incompatible state, call
endFrame (which flushes), change state, then begin a new pass. There is no hidden
per-sprite GL state polling. A manual flush alone does not relinquish ownership.

| State | Contract |
|---|---|
| Program, VAO, array buffer | Owned during pass, restored at end |
| EBO | Attached to owned VAO; caller VAO/EBO association untouched |
| Texture units 0..slot limit, active unit | Saved at begin, restored at end |
| Blend enable, RGB/alpha equations and factors | Saved and restored |
| Depth, scissor, cull, framebuffer, viewport, color mask, sRGB | Caller-owned, never modified |

Immediate retains its M3 state contract: it leaves program, VAO, texture unit 0
and blending configured. Call beginFrame before drawing; repeated begin resets
its frame counters. Sandbox uses a separate frame draw count to report zero even
when the framebuffer is minimized. Interaction still uses the existing Window
VSync setting; only the benchmark requests interval zero.

## Geometry and texture submission

Each quad contributes 4 vertices (36 bytes each) and 6 uint32 indices. CPU model
transformation matches M3's center rotation, position and scale; camera projection
is one uniform per pass. Capacity is checked against GLsizei index count, GLuint
vertex indices and GLsizeiptr upload bytes before allocation. Reserving CPU arrays
before submission avoids allocation while appending a quad.

Capacity flush occurs before appending the next sprite, so exactly 1000 sprites
produce one draw at end. Vertex and index capacity are coupled by construction.
An unseen texture flushes when all slots are occupied; otherwise repeated handles
reuse their slot. No sort occurs: indices and texture assignment preserve original
submission order, including across flushes.

Fragment limit is `min(requested, GL_MAX_TEXTURE_IMAGE_UNITS)` with a configured
maximum of eight. Generated GLSL 330 uses a switch with **constant sampler array
subscripts**. UV derivatives are computed before the switch and passed through
textureGrad, avoiding implicit derivatives in divergent control flow. Unused
sampler uniforms alias unit zero so all active samplers reference complete textures.
This avoids a warning observed on Apple's Metal-backed OpenGL driver.

Each nonempty flush orphans VBO storage using glBufferData(null, GL_STREAM_DRAW),
then uploads only queued vertices with glBufferSubData and issues one
`glDrawElements`. OpenGL retains in-flight old storage; no unsynchronized mapped
regions or per-frame GPU object creation are used. EBO remains immutable.

Straight-alpha source RGB is multiplied by source alpha; destination RGB by
one minus source alpha. Alpha uses ONE / ONE_MINUS_SRC_ALPHA:
`Cout = Cs*As + Cd*(1-As)`, `Aout = As + Ad*(1-As)`.
This corrects M3's alpha channel while preserving its RGB appearance.

## Diagnostics and performance scope

ShaderProgram caches successful uniform locations per program owner, preserving
missing-name and wrong-bound-program errors. Immediate caches its private model
and tint locations and sets camera/sampler once in beginFrame. Texture2D caches
its context's combined texture-unit limit at creation. Public wrapper validation
remains; internal context prechecks and hot binding/uniform checks use debugGL.
Shader compile/link status, allocation/upload failures and draw errors remain
checked in Release. Performance results therefore include remaining validation,
state capture/restoration, CPU vertex transformation and driver submission costs.
