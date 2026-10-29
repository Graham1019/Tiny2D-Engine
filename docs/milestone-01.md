# Milestone 01 — Window + Input + Game Loop

## Modules and source files

- `engine/include/tiny2d/Window.h`, `engine/src/Platform/Window.cpp`: window/context ownership, event polling, close requests, swap, title, logical and framebuffer dimensions, viewport resize, minimal clear operation.
- `engine/include/tiny2d/Input.h`, `engine/src/Platform/Input.cpp`: polling bound to a specific Window; named WASD/Escape keys and integer GLFW keyboard tokens.
- `engine/include/tiny2d/Application.h`, `engine/src/Core/Application.cpp`: reusable loop with virtual `update(float)` and `render()` hooks.
- `sandbox/src/main.cpp`: 800×600 demo, color mapping, Escape handling and FPS title. These behaviors are intentionally Sandbox-specific.
- Existing Vector2 API and `Vector2Tests` remain intact; obsolete exercise TODOs were removed.

## Loop and time

`Application` constructs Window, then Input. `run()` performs:

1. Poll GLFW events; stop immediately if the window was closed.
2. Sample `std::chrono::steady_clock`, compute elapsed seconds since the previous sample.
3. Clamp simulation deltaTime to [0, 0.1] seconds and call `update(deltaTime)`.
4. Stop if update requested close (including Sandbox Escape).
5. Make the application's context current, call `render()`, then swap buffers.

The initial timestamp is captured immediately before entering the loop, excluding window creation and subclass setup time. A monotonic clock avoids wall-clock adjustments. The cap prevents a debugger pause or stall producing a huge simulation step; excess time is discarded, so this is not a fixed-step simulation. Sandbox FPS uses separate, **unclamped real elapsed time** and updates its title approximately once per second. VSync is requested with swap interval 1; driver/compositor behavior determines the actual rate.

## Ownership and threading

Window owns its GLFW window through `unique_ptr` with a custom deleter. A private shared RAII runtime initializes GLFW once while any Window exists; a main-thread-only weak registry lets subsequent windows reuse it. The registry itself never keeps GLFW alive. The last Window destroys its native window before releasing the runtime and calling `glfwTerminate`. Construction failures unwind the same owners and report GLFW's error code and description. Invalid dimensions throw `invalid_argument` before initialization.

Window and Application are neither copyable nor movable. Input holds a non-owning Window reference, which must remain valid for its lifetime. Application's declaration order guarantees this. There is no global input state. Native pointers used internally for callbacks/context selection are borrowed, not owning.

All lifecycle, event, input and rendering operations must occur on the macOS main thread. Tiny2D assumes it owns GLFW initialization/termination; do not mix independent external GLFW lifetime managers. The weak registry is intentionally not a cross-thread singleton.

Window size is in logical screen coordinates; framebuffer size is in pixels and may differ on Retina displays. Initial viewport and resize callbacks use framebuffer pixels. The callback temporarily makes the resized window's context current, updates the viewport, then restores the previous context.

## macOS build and run

Requires Apple Silicon macOS, AppleClang with Command Line Tools, C++17, CMake 3.20+, GLFW 3.3+ and system OpenGL.

```bash
brew install cmake glfw
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/tiny2d_sandbox
```

If Homebrew is not in CMake's search paths:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$(brew --prefix)"
```

Dependencies are discovered using `find_package`; configure fails with installation guidance when GLFW is missing. No dependencies are downloaded by CMake. OpenGL requests 3.3 Core with the macOS forward-compatible flag; macOS may supply a higher compatible core context. Only system OpenGL headers and `OpenGL::GL` are used. Future function-loader initialization can be added immediately after context activation in Window.cpp.

## Manual acceptance

1. Launch from a macOS desktop terminal. Expect an 800×600 content area with a blue-gray background.
2. Focus the window. Hold W for green, A for red, D for blue; combine these keys to mix channels. Release to return to the default color. S overrides all channels with dark gray, including in combinations.
3. Confirm the title displays FPS and refreshes roughly once per second.
4. Drag each window edge, maximize/restore and, if available, move between Retina/non-Retina displays. Background must fill the framebuffer without borders or OpenGL errors. Because clear covers the entire framebuffer regardless of viewport, a solid background alone does not prove viewport correctness: use an OpenGL debugger to inspect `GL_VIEWPORT` against framebuffer dimensions when validating the callback precisely.
5. Press Escape and confirm the process exits normally. Relaunch and close using the native close button; repeat several times. For leak verification, use Instruments/Leaks; a clean exit alone is not proof of no leaks.
6. Minimize/restore and pause/resume in a debugger. Confirm responsiveness; simulation dt must not exceed 0.1 seconds.

## Validation record (2026-10-29)

- Debug configure/build succeeded on this Apple Silicon machine using installed Homebrew GLFW and system OpenGL.
- CTest: Vector2Tests passed, 1/1.
- Sandbox was launched. The user confirmed the window appeared, WASD color feedback and simultaneous-key mixing worked. The user also confirmed Escape exits normally and the FPS title updates.
- Automated desktop tooling could not attach to the standalone Sandbox executable. Native close-button behavior, resizing/viewport, Retina transitions and resource-leak checks remain manually unverified. No claim of full GUI acceptance is made.

## Tradeoffs and limitations

This milestone exposes a small clear operation on Window so platform OpenGL includes remain out of Sandbox; a future renderer should own richer rendering operations. It introduces no shader, texture, ECS, scene or worker-thread framework. The frame path performs no intentional dynamic allocations except for the once-per-second title text.

Input is level-triggered polling, with no pressed/released edges, text, mouse or gamepad handling; very short key transitions between polls may be missed. Named keys cover this milestone; integer overloads accept other GLFW tokens. No fixed timestep, minimized-window throttling or context-sharing policy is implemented. Multiwindow runtime ownership is supported structurally but has not been exercised interactively. OpenGL is deprecated by Apple; this milestone deliberately retains it. Initialization/context-creation failure paths are implemented but have not been fault-injected. Automated tests currently cover Vector2, not GUI behavior.
