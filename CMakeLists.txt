
cmake_minimum_required(VERSION 3.20)

set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS deployment target")

project(Tiny2D VERSION 0.5.0 LANGUAGES CXX)

if(NOT APPLE)
    message(FATAL_ERROR "M2 renderer currently supports macOS system OpenGL only.")
endif()
enable_language(OBJCXX)
find_library(TINY2D_APPKIT AppKit REQUIRED)

# Use locally installed dependencies; never fetch sources implicitly.
find_package(glfw3 3.3 QUIET CONFIG)
if(NOT TARGET glfw)
    message(FATAL_ERROR "GLFW 3.3+ not found. On macOS: brew install glfw; configure with -DCMAKE_PREFIX_PATH=$(brew --prefix) if needed.")
endif()
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED) # Local libpng; brew install libpng. No configure-time downloads.

# 引擎静态库
add_library(tiny2d_engine STATIC
    engine/src/Renderer/DebugDraw.cpp
    engine/src/Renderer/TimestampQueries.cpp
    engine/src/Scene/Scene.cpp
    engine/src/Physics/Collision2D.cpp
    engine/src/Renderer/SceneRenderer.cpp
    engine/src/Math/Vector2.cpp
    engine/src/Math/Mat4.cpp
    engine/src/Renderer/Camera2D.cpp
    engine/src/Renderer/RenderTarget2D.cpp
    engine/src/Renderer/Texture2D.cpp
    engine/src/Renderer/BatchRenderer.cpp
    engine/src/Renderer/SpriteRenderer.cpp
    engine/src/Assets/ImageLoader.cpp
    engine/src/Platform/Window.cpp
    engine/src/Platform/Input.cpp
    engine/src/Core/Application.cpp
    engine/src/Renderer/GLObject.cpp
    engine/src/Renderer/ShaderProgram.cpp
    engine/src/Renderer/VertexBuffer.cpp
    engine/src/Renderer/VertexArray.cpp
)

target_include_directories(tiny2d_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/include"
)

target_compile_features(tiny2d_engine PUBLIC cxx_std_17)
target_link_libraries(tiny2d_engine PRIVATE glfw OpenGL::GL PNG::PNG)

set_target_properties(tiny2d_engine PROPERTIES
    CXX_EXTENSIONS OFF
)

# 游戏测试程序
add_executable(tiny2d_sandbox
    sandbox/src/main.cpp
    sandbox/src/ArenaDemo.cpp
    sandbox/src/SurvivorDemo.cpp
    sandbox/src/ShaderAssets.cpp
)

target_link_libraries(tiny2d_sandbox PRIVATE
    tiny2d_engine
)

set_target_properties(tiny2d_sandbox PROPERTIES
    CXX_EXTENSIONS OFF
)

# Runs on every build so editing only a shader still refreshes runtime assets.
add_custom_target(tiny2d_shader_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/assets"
        "$<TARGET_FILE_DIR:tiny2d_sandbox>/assets"
    VERBATIM
)
add_dependencies(tiny2d_sandbox tiny2d_shader_assets)

# Enable testing with CTest
include(CTest)

if(BUILD_TESTING)
    add_executable(tiny2d_m3_cpu_tests tests/test_m3_cpu.cpp sandbox/src/ShaderAssets.cpp)
    target_include_directories(tiny2d_m3_cpu_tests PRIVATE sandbox/src)
    target_link_libraries(tiny2d_m3_cpu_tests PRIVATE tiny2d_engine)
    add_dependencies(tiny2d_m3_cpu_tests tiny2d_shader_assets)
    add_test(NAME M3CPUTests COMMAND tiny2d_m3_cpu_tests)
    add_executable(tiny2d_tests
        tests/test_vector2.cpp
    )

    target_link_libraries(tiny2d_tests PRIVATE
        tiny2d_engine
    )

    add_test(
        NAME Vector2Tests
        COMMAND tiny2d_tests
    )

    add_executable(tiny2d_failure_tests
        tests/test_render_failures.cpp
        engine/src/Renderer/SceneRenderer.cpp
        engine/src/Scene/Scene.cpp
        engine/src/Physics/Collision2D.cpp
        engine/src/Renderer/BatchRenderer.cpp
        engine/src/Renderer/Camera2D.cpp
        engine/src/Math/Mat4.cpp
        engine/src/Math/Vector2.cpp
        engine/src/Renderer/RenderTarget2D.cpp
        engine/src/Renderer/Texture2D.cpp
        engine/src/Renderer/GLObject.cpp
        engine/src/Renderer/ShaderProgram.cpp
        engine/src/Renderer/VertexBuffer.cpp
        engine/src/Renderer/VertexArray.cpp
    )
    target_include_directories(tiny2d_failure_tests PRIVATE engine/include)
    target_compile_features(tiny2d_failure_tests PRIVATE cxx_std_17)
    set_target_properties(tiny2d_failure_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(tiny2d_failure_tests PRIVATE glfw)
    add_test(NAME RenderFailureTests COMMAND tiny2d_failure_tests)

    option(TINY2D_OPENGL_TESTS "Run tests requiring a macOS graphical login and real OpenGL context" ON)
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m3_gpu_tests tests/test_m3_gpu.cpp)
        target_link_libraries(tiny2d_m3_gpu_tests PRIVATE tiny2d_engine glfw OpenGL::GL)
        add_test(NAME M3GPUTests COMMAND tiny2d_m3_gpu_tests)
        add_test(NAME TriangleRegression COMMAND tiny2d_sandbox --scene triangle --frames 4)
        set_tests_properties(M3GPUTests TriangleRegression PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 30)
        add_executable(tiny2d_render_tests tests/test_render.cpp sandbox/src/ShaderAssets.cpp)
        target_include_directories(tiny2d_render_tests PRIVATE sandbox/src)
        target_link_libraries(tiny2d_render_tests PRIVATE tiny2d_engine glfw OpenGL::GL)
        set_target_properties(tiny2d_render_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_dependencies(tiny2d_render_tests tiny2d_shader_assets)
        add_test(NAME OpenGLResourceTests COMMAND tiny2d_render_tests)
        add_test(NAME SandboxSmokeRoot COMMAND tiny2d_sandbox --frames 4)
        add_test(NAME SandboxSmokeBuild COMMAND tiny2d_sandbox --frames 4)
        set_tests_properties(SandboxSmokeRoot PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(SandboxSmokeBuild PROPERTIES WORKING_DIRECTORY "$<TARGET_FILE_DIR:tiny2d_sandbox>")
        set_tests_properties(OpenGLResourceTests SandboxSmokeRoot SandboxSmokeBuild
            PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 30)
    endif()
endif()

add_executable(tiny2d_benchmark sandbox/src/benchmark.cpp)
target_link_libraries(tiny2d_benchmark PRIVATE tiny2d_engine glfw OpenGL::GL)
if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m4_gpu_tests tests/test_m4_gpu.cpp)
    target_link_libraries(tiny2d_m4_gpu_tests PRIVATE tiny2d_engine glfw OpenGL::GL)
    add_test(NAME M4GPUTests COMMAND tiny2d_m4_gpu_tests)
    set_tests_properties(M4GPUTests PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
endif()

if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m41_gpu_tests tests/test_m41_gpu.cpp)
    target_link_libraries(tiny2d_m41_gpu_tests PRIVATE tiny2d_engine glfw OpenGL::GL)
    add_test(NAME M41GPUTests COMMAND tiny2d_m41_gpu_tests)
    set_tests_properties(M41GPUTests PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
endif()

if(BUILD_TESTING)
    foreach(suite scene collision fixed_step)
        add_executable(tiny2d_m5_${suite}_tests tests/test_m5_${suite}.cpp)
        target_link_libraries(tiny2d_m5_${suite}_tests PRIVATE tiny2d_engine)
        add_test(NAME M5_${suite} COMMAND tiny2d_m5_${suite}_tests)
        set_tests_properties(M5_${suite} PROPERTIES LABELS "cpu")
    endforeach()
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m5_gpu_tests tests/test_m5_gpu.cpp)
        target_link_libraries(tiny2d_m5_gpu_tests PRIVATE tiny2d_engine glfw OpenGL::GL)
        add_test(NAME M5GPU COMMAND tiny2d_m5_gpu_tests)
        add_test(NAME M5Arena COMMAND tiny2d_sandbox --scene arena-demo --frames 120)
        set_tests_properties(M5GPU M5Arena PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    endif()
endif()

if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m51_gpu_tests tests/test_m51_gpu.cpp)
    target_link_libraries(tiny2d_m51_gpu_tests PRIVATE tiny2d_engine glfw OpenGL::GL)
    add_test(NAME M51GPU COMMAND tiny2d_m51_gpu_tests)
    set_tests_properties(M51GPU PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
endif()
if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m51_benchmark tests/benchmark_m51.cpp)
    target_link_libraries(tiny2d_m51_benchmark PRIVATE tiny2d_engine glfw OpenGL::GL)
endif()


# CPU gameplay has no renderer/platform translation units or GL link dependency.
add_library(tiny2d_game STATIC game/src/GameSession.cpp game/src/Difficulty.cpp
    engine/src/Scene/Scene.cpp engine/src/Physics/Collision2D.cpp engine/src/Math/Vector2.cpp)
target_include_directories(tiny2d_game PUBLIC game/include engine/include)
target_compile_features(tiny2d_game PUBLIC cxx_std_17)
target_link_libraries(tiny2d_sandbox PRIVATE tiny2d_game)
if(BUILD_TESTING)
    foreach(suite combat projectiles spawning)
        add_executable(tiny2d_m61_${suite}_tests tests/test_m61_${suite}.cpp)
        target_link_libraries(tiny2d_m61_${suite}_tests PRIVATE tiny2d_game)
        add_test(NAME M61_${suite} COMMAND tiny2d_m61_${suite}_tests)
        set_tests_properties(M61_${suite} PROPERTIES LABELS "cpu")
    endforeach()
    add_executable(tiny2d_m61_benchmark tests/benchmark_m61.cpp)
    target_link_libraries(tiny2d_m61_benchmark PRIVATE tiny2d_game)
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m61_gpu_tests tests/test_m61_gpu.cpp)
        target_link_libraries(tiny2d_m61_gpu_tests PRIVATE tiny2d_game tiny2d_engine glfw OpenGL::GL)
        add_test(NAME M61GPU COMMAND tiny2d_m61_gpu_tests)
        add_test(NAME M61Survivor COMMAND tiny2d_sandbox --scene survivor-demo --frames 600)
        set_tests_properties(M61GPU M61Survivor PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    endif()
endif()
if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m61_gpu_benchmark tests/benchmark_m61_gpu.cpp)
    target_link_libraries(tiny2d_m61_gpu_benchmark PRIVATE tiny2d_game tiny2d_engine glfw OpenGL::GL)
endif()

add_library(tiny2d_ui STATIC sandbox/src/vfx/CombatVFX.cpp sandbox/src/ui/DifficultyMenu.cpp sandbox/src/ui/BitmapFont.cpp sandbox/src/ui/GameHUD.cpp)
target_include_directories(tiny2d_ui PUBLIC sandbox/src)
target_link_libraries(tiny2d_ui PUBLIC tiny2d_game)
target_link_libraries(tiny2d_sandbox PRIVATE tiny2d_ui)
if(BUILD_TESTING)
    foreach(suite game_phase restart hud)
        add_executable(tiny2d_m62_${suite}_tests tests/test_m62_${suite}.cpp)
        target_link_libraries(tiny2d_m62_${suite}_tests PRIVATE tiny2d_ui)
        add_test(NAME M62_${suite} COMMAND tiny2d_m62_${suite}_tests)
    endforeach()
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m62_gpu_tests tests/test_m62_gpu.cpp)
        target_link_libraries(tiny2d_m62_gpu_tests PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
        add_test(NAME M62GPU COMMAND tiny2d_m62_gpu_tests)
        set_tests_properties(M62GPU PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    endif()
endif()
if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m62_benchmark tests/benchmark_m62.cpp)
    target_link_libraries(tiny2d_m62_benchmark PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
endif()

if(BUILD_TESTING)
    foreach(suite difficulty flow pacing)
        add_executable(tiny2d_m63a_${suite}_tests tests/test_m63a_${suite}.cpp)
        target_link_libraries(tiny2d_m63a_${suite}_tests PRIVATE tiny2d_ui)
        add_test(NAME M63a_${suite} COMMAND tiny2d_m63a_${suite}_tests)
    endforeach()
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m63a_gpu_tests tests/test_m63a_gpu.cpp)
        target_link_libraries(tiny2d_m63a_gpu_tests PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
        add_test(NAME M63aGPU COMMAND tiny2d_m63a_gpu_tests)
        set_tests_properties(M63aGPU PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
        add_executable(tiny2d_m63a_benchmark tests/benchmark_m63a.cpp)
        target_link_libraries(tiny2d_m63a_benchmark PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
    endif()
endif()
if(BUILD_TESTING)
    foreach(value 0 6 -1 abc)
        add_test(NAME M63a_invalid_difficulty_${value} COMMAND tiny2d_sandbox --scene survivor-demo --difficulty ${value} --frames 1)
        set_tests_properties(M63a_invalid_difficulty_${value} PROPERTIES WILL_FAIL TRUE)
    endforeach()
    if(TINY2D_OPENGL_TESTS)
        add_test(NAME M63aSurvivorExtreme COMMAND tiny2d_sandbox --scene survivor-demo --difficulty 5 --frames 600)
        set_tests_properties(M63aSurvivorExtreme PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60 PASS_REGULAR_EXPRESSION "difficulty=5.*draws_world=1 draws_ui=2 draws_total=3")
    endif()
endif()

if(BUILD_TESTING)
    foreach(suite events vfx fps)
        add_executable(tiny2d_m63b_${suite}_tests tests/test_m63b_${suite}.cpp)
        target_link_libraries(tiny2d_m63b_${suite}_tests PRIVATE tiny2d_ui)
        add_test(NAME M63b_${suite} COMMAND tiny2d_m63b_${suite}_tests)
    endforeach()
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m63b_gpu_tests tests/test_m63b_gpu.cpp)
        target_link_libraries(tiny2d_m63b_gpu_tests PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
        add_test(NAME M63bGPU COMMAND tiny2d_m63b_gpu_tests)
        set_tests_properties(M63bGPU PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 180)
    endif()
endif()
if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m63b_benchmark tests/benchmark_m63b.cpp)
    target_compile_definitions(tiny2d_m63b_benchmark PRIVATE M63B)
    target_link_libraries(tiny2d_m63b_benchmark PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
endif()
if(BUILD_TESTING)
    add_executable(tiny2d_m63b_priority_tests tests/test_m63b_priority.cpp)
    target_link_libraries(tiny2d_m63b_priority_tests PRIVATE tiny2d_ui)
    add_test(NAME M63b_priority COMMAND tiny2d_m63b_priority_tests)
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m63b_recovery_tests tests/test_m63b_render_recovery.cpp)
        target_link_libraries(tiny2d_m63b_recovery_tests PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
        add_test(NAME M63b_render_recovery COMMAND tiny2d_m63b_recovery_tests)
        set_tests_properties(M63b_render_recovery PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    endif()
endif()

if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m63b1_benchmark tests/benchmark_m63b1.cpp)
    target_compile_definitions(tiny2d_m63b1_benchmark PRIVATE M63B M63B1)
    target_link_libraries(tiny2d_m63b1_benchmark PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
endif()

# Dedicated distribution entry; shared Survivor implementation, unchanged sandbox CLI.
add_executable(Tiny2DSurvivor MACOSX_BUNDLE
    sandbox/src/SurvivorMain.cpp sandbox/src/SurvivorDemo.cpp sandbox/src/ShaderAssets.cpp)
target_link_libraries(Tiny2DSurvivor PRIVATE tiny2d_engine tiny2d_ui)
set_target_properties(Tiny2DSurvivor PROPERTIES
    CXX_EXTENSIONS OFF
    MACOSX_BUNDLE_BUNDLE_NAME "Tiny2D Survivor"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
# Output name must remain the executable name; bundle is renamed in distribution staging.
add_custom_target(tiny2d_app_resources ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_BUNDLE_CONTENT_DIR:Tiny2DSurvivor>/Resources/assets"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_BUNDLE_CONTENT_DIR:Tiny2DSurvivor>/Resources/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/assets"
        "$<TARGET_BUNDLE_CONTENT_DIR:Tiny2DSurvivor>/Resources/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/docs/third-party"
        "$<TARGET_BUNDLE_CONTENT_DIR:Tiny2DSurvivor>/Resources/licenses"
    VERBATIM)
add_dependencies(Tiny2DSurvivor tiny2d_app_resources)
add_custom_target(package_macos
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_macos.sh" "${CMAKE_BINARY_DIR}"
    DEPENDS Tiny2DSurvivor VERBATIM)

# Seeded world data can be used without a renderer, window or graphics context.
add_library(tiny2d_world STATIC
    engine/src/Math/Vector2.cpp
    engine/src/World/TileMap.cpp
    engine/src/World/MapGenerator.cpp
    engine/src/World/TileCollision.cpp
    engine/src/World/MapSession.cpp
    engine/src/World/NavigationGrid.cpp
    engine/src/World/DistanceField.cpp
    engine/src/World/GridNavigator.cpp)
target_include_directories(tiny2d_world PUBLIC engine/include)
target_compile_features(tiny2d_world PUBLIC cxx_std_17)
set_target_properties(tiny2d_world PROPERTIES CXX_EXTENSIONS OFF)
target_sources(tiny2d_game PRIVATE game/src/NavigationSession.cpp game/src/UpgradeSystem.cpp)
target_link_libraries(tiny2d_game PUBLIC tiny2d_world)
if(BUILD_TESTING)
    add_executable(tiny2d_m91_generation tests/m91_map_generation.cpp)
    target_link_libraries(tiny2d_m91_generation PRIVATE tiny2d_world)
    add_test(NAME M91_MapGenerationCPU COMMAND tiny2d_m91_generation)
    set_tests_properties(M91_MapGenerationCPU PROPERTIES LABELS "cpu")
    add_executable(tiny2d_m91_collision tests/m91_tile_collision.cpp)
    add_executable(tiny2d_m91_session tests/m91_map_session.cpp)
    target_link_libraries(tiny2d_m91_collision PRIVATE tiny2d_world)
    target_link_libraries(tiny2d_m91_session PRIVATE tiny2d_world)
    add_test(NAME M91_TileCollisionCPU COMMAND tiny2d_m91_collision)
    add_test(NAME M91_MapSessionCPU COMMAND tiny2d_m91_session)
    set_tests_properties(M91_TileCollisionCPU M91_MapSessionCPU PROPERTIES LABELS "cpu")
    add_executable(tiny2d_m92_navigation tests/m92_navigation.cpp)
    add_executable(tiny2d_m92_motion tests/m92_enemy_motion.cpp)
    target_link_libraries(tiny2d_m92_navigation PRIVATE tiny2d_world)
    target_link_libraries(tiny2d_m92_motion PRIVATE tiny2d_world)
    add_test(NAME M92_NavigationCPU COMMAND tiny2d_m92_navigation)
    add_test(NAME M92_EnemyMotionCPU COMMAND tiny2d_m92_motion)
    set_tests_properties(M92_NavigationCPU M92_EnemyMotionCPU PROPERTIES LABELS "cpu")
    add_executable(tiny2d_m92_upgrade tests/m92_upgrade.cpp)
    add_executable(tiny2d_m92_session tests/m92_navigation_session.cpp)
    target_link_libraries(tiny2d_m92_upgrade PRIVATE tiny2d_game)
    target_link_libraries(tiny2d_m92_session PRIVATE tiny2d_game)
    add_test(NAME M92_UpgradeCPU COMMAND tiny2d_m92_upgrade)
    add_test(NAME M92_NavigationSessionCPU COMMAND tiny2d_m92_session)
    set_tests_properties(M92_UpgradeCPU M92_NavigationSessionCPU PROPERTIES LABELS "cpu")
endif()

# Particle simulation and its tests have no platform, renderer or ImGui link dependencies.
add_library(tiny2d_particles STATIC lab/src/ParticleSimulation.cpp engine/src/Math/Vector2.cpp)
target_include_directories(tiny2d_particles PUBLIC lab/src engine/include)
target_compile_features(tiny2d_particles PUBLIC cxx_std_17)
set_target_properties(tiny2d_particles PROPERTIES CXX_EXTENSIONS OFF)
if(BUILD_TESTING)
    # Exercise the production pause/step clock alongside the simulation.
    add_executable(tiny2d_m81_cpu tests/test_m81_cpu.cpp lab/src/Neon.cpp lab/src/NormalMaterial.cpp)
    target_link_libraries(tiny2d_m81_cpu PRIVATE tiny2d_particles)
    add_test(NAME M81_ParticleSimulationCPU COMMAND tiny2d_m81_cpu)
    set_tests_properties(M81_ParticleSimulationCPU PROPERTIES LABELS "cpu")
endif()

# Lab has its own executable-adjacent asset tree and reuses the macOS locator.
add_library(tiny2d_lab_core STATIC lab/src/Neon.cpp lab/src/Selection.cpp lab/src/Lighting.cpp lab/src/NormalMaterial.cpp lab/src/Occlusion.cpp lab/src/Presentation.cpp
    lab/src/FrameState.cpp lab/src/Pipeline.cpp lab/src/ParticleRenderer.cpp lab/src/MapPresentation.cpp
    lab/src/ShaderReloadManager.cpp lab/src/ShaderGallery.cpp sandbox/src/ShaderAssets.cpp)
target_include_directories(tiny2d_lab_core PUBLIC lab/src PRIVATE engine/src/Renderer)
target_link_libraries(tiny2d_lab_core PUBLIC tiny2d_engine tiny2d_ui tiny2d_particles tiny2d_world PRIVATE glfw OpenGL::GL)
add_library(tiny2d_lab_imgui STATIC
    third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_tables.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(tiny2d_lab_imgui PUBLIC third_party/imgui third_party/imgui/backends)
target_compile_definitions(tiny2d_lab_imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM GLFW_INCLUDE_NONE)
target_compile_features(tiny2d_lab_imgui PRIVATE cxx_std_17)
target_link_libraries(tiny2d_lab_imgui PRIVATE glfw OpenGL::GL)
add_library(tiny2d_inspector STATIC lab/src/Inspector.cpp lab/src/ParticleInspector.cpp lab/src/ShaderPanel.cpp
    lab/src/ParticlePresentation.cpp lab/src/Capture.cpp lab/src/CocoaKeyEvent.mm)
target_link_libraries(tiny2d_inspector PUBLIC tiny2d_lab_core tiny2d_lab_imgui PRIVATE glfw OpenGL::GL PNG::PNG ${TINY2D_APPKIT})
target_include_directories(tiny2d_inspector PRIVATE engine/src/Renderer)
add_executable(tiny2d_lab lab/src/main.cpp lab/src/ParticleApp.cpp lab/src/ShaderApp.cpp
    lab/src/MapApp.cpp lab/src/MapInspector.cpp lab/src/Metrics.cpp)
target_include_directories(tiny2d_lab PRIVATE engine/src/Renderer)
target_link_libraries(tiny2d_lab PRIVATE tiny2d_inspector glfw OpenGL::GL PNG::PNG)
set_target_properties(tiny2d_lab PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
add_custom_target(tiny2d_lab_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/lab/assets"
        "$<TARGET_FILE_DIR:tiny2d_lab>/assets"
    VERBATIM)
add_dependencies(tiny2d_lab tiny2d_lab_assets)
if(BUILD_TESTING)
    add_executable(tiny2d_m71_cpu tests/test_m71_cpu.cpp)
    target_link_libraries(tiny2d_m71_cpu PRIVATE tiny2d_lab_core)
    add_test(NAME M71CPU COMMAND tiny2d_m71_cpu)
    set_tests_properties(M71CPU PROPERTIES LABELS "cpu")
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m71_gpu tests/test_m71_gpu.cpp)
        target_include_directories(tiny2d_m71_gpu PRIVATE engine/src/Renderer)
        target_link_libraries(tiny2d_m71_gpu PRIVATE tiny2d_lab_core glfw OpenGL::GL)
        set_target_properties(tiny2d_m71_gpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
        add_dependencies(tiny2d_m71_gpu tiny2d_lab_assets)
        add_test(NAME M71GPU COMMAND tiny2d_m71_gpu)
        add_test(NAME M71RelocatedCLI COMMAND ${CMAKE_COMMAND}
            "-DEXE=$<TARGET_FILE:tiny2d_lab>" "-DSTAGE=${CMAKE_BINARY_DIR}/m71 relocation"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/m71_relocated.cmake")
        set_tests_properties(M71GPU M71RelocatedCLI PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(tiny2d_m72_cpu tests/test_m72_cpu.cpp lab/src/Metrics.cpp)
    target_link_libraries(tiny2d_m72_cpu PRIVATE tiny2d_lab_core)
    add_test(NAME M72CPU COMMAND tiny2d_m72_cpu)
    set_tests_properties(M72CPU PROPERTIES LABELS "cpu")
endif()
if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m72_gl tests/test_m72_gl.cpp tests/m72_cocoa_events.mm)
    target_link_libraries(tiny2d_m72_gl PRIVATE tiny2d_inspector glfw OpenGL::GL ${TINY2D_APPKIT})
    target_include_directories(tiny2d_m72_gl PRIVATE engine/src/Renderer)
    set_target_properties(tiny2d_m72_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
    add_dependencies(tiny2d_m72_gl tiny2d_lab_assets)
    add_test(NAME M72InspectorGL COMMAND tiny2d_m72_gl)
    set_tests_properties(M72InspectorGL PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
endif()

if(BUILD_TESTING)
    foreach(suite cpu controls)
        add_executable(tiny2d_m73_${suite} tests/test_m73_${suite}.cpp)
        target_link_libraries(tiny2d_m73_${suite} PRIVATE tiny2d_lab_core)
        add_test(NAME M73_${suite} COMMAND tiny2d_m73_${suite})
        set_tests_properties(M73_${suite} PROPERTIES LABELS "cpu")
    endforeach()
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m73_gl tests/test_m73_gl.cpp)
        target_include_directories(tiny2d_m73_gl PRIVATE engine/src/Renderer)
        target_link_libraries(tiny2d_m73_gl PRIVATE tiny2d_lab_core glfw OpenGL::GL)
        set_target_properties(tiny2d_m73_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
        add_dependencies(tiny2d_m73_gl tiny2d_lab_assets)
        add_test(NAME M73_LightingGL COMMAND tiny2d_m73_gl)
        set_tests_properties(M73_LightingGL PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(tiny2d_m74a_cpu tests/test_m74a_cpu.cpp)
    target_link_libraries(tiny2d_m74a_cpu PRIVATE tiny2d_lab_core)
    add_test(NAME M74a_NormalMaterialsCPU COMMAND tiny2d_m74a_cpu)
    set_tests_properties(M74a_NormalMaterialsCPU PROPERTIES LABELS "cpu")
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m74a_gl tests/test_m74a_gl.cpp)
        target_include_directories(tiny2d_m74a_gl PRIVATE engine/src/Renderer)
        target_link_libraries(tiny2d_m74a_gl PRIVATE tiny2d_inspector glfw OpenGL::GL)
        set_target_properties(tiny2d_m74a_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
        add_dependencies(tiny2d_m74a_gl tiny2d_lab_assets)
        add_test(NAME M74a_NormalLightingGL COMMAND tiny2d_m74a_gl)
        set_tests_properties(M74a_NormalLightingGL PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(tiny2d_m74b_cpu tests/test_m74b_cpu.cpp)
    target_link_libraries(tiny2d_m74b_cpu PRIVATE tiny2d_lab_core)
    add_test(NAME M74b_OcclusionCPU COMMAND tiny2d_m74b_cpu)
    set_tests_properties(M74b_OcclusionCPU PROPERTIES LABELS "cpu")
    if(TINY2D_OPENGL_TESTS)
        add_executable(tiny2d_m74b_gl tests/test_m74b_gl.cpp)
        target_include_directories(tiny2d_m74b_gl PRIVATE engine/src/Renderer)
        target_link_libraries(tiny2d_m74b_gl PRIVATE tiny2d_inspector glfw OpenGL::GL)
        set_target_properties(tiny2d_m74b_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
        add_dependencies(tiny2d_m74b_gl tiny2d_lab_assets)
        add_test(NAME M74b_OcclusionGL COMMAND tiny2d_m74b_gl)
        set_tests_properties(M74b_OcclusionGL PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    endif()
endif()

if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m81_gl tests/test_m81_gl.cpp)
    target_include_directories(tiny2d_m81_gl PRIVATE engine/src/Renderer)
    target_link_libraries(tiny2d_m81_gl PRIVATE tiny2d_lab_core glfw OpenGL::GL)
    set_target_properties(tiny2d_m81_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
    add_dependencies(tiny2d_m81_gl tiny2d_lab_assets)
    add_test(NAME M81_ParticleInstancingGL COMMAND tiny2d_m81_gl)
    add_executable(tiny2d_m81_app tests/test_m81_app.cpp)
    target_include_directories(tiny2d_m81_app PRIVATE engine/src/Renderer)
    target_link_libraries(tiny2d_m81_app PRIVATE tiny2d_inspector glfw OpenGL::GL)
    set_target_properties(tiny2d_m81_app PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
    add_dependencies(tiny2d_m81_app tiny2d_lab_assets)
    add_test(NAME M81_ParticleInspectorGL COMMAND tiny2d_m81_app)
    add_test(NAME M81_ParticleFiniteCLI COMMAND ${CMAKE_COMMAND}
        "-DEXE=$<TARGET_FILE:tiny2d_lab>" "-DSTAGE=${CMAKE_BINARY_DIR}/m81-finite"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/m81_finite.cmake")
    set_tests_properties(M81_ParticleInstancingGL M81_ParticleInspectorGL M81_ParticleFiniteCLI PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
endif()

if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m82_gl tests/test_m82_gl.cpp)
    target_include_directories(tiny2d_m82_gl PRIVATE engine/src/Renderer)
    target_link_libraries(tiny2d_m82_gl PRIVATE tiny2d_inspector glfw OpenGL::GL)
    set_target_properties(tiny2d_m82_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
    add_dependencies(tiny2d_m82_gl tiny2d_lab_assets)
    add_test(NAME M82_ShaderGalleryGL COMMAND tiny2d_m82_gl)
    add_test(NAME M82_ShaderFiniteCLI COMMAND /usr/bin/env python3
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/m82_finite.py" "$<TARGET_FILE:tiny2d_lab>" "${CMAKE_BINARY_DIR}/m82-finite")
    set_tests_properties(M82_ShaderGalleryGL M82_ShaderFiniteCLI PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 90)
endif()

if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m91_gl tests/m91_map_gpu.cpp)
    target_include_directories(tiny2d_m91_gl PRIVATE engine/src/Renderer)
    target_link_libraries(tiny2d_m91_gl PRIVATE tiny2d_lab_core glfw OpenGL::GL)
    set_target_properties(tiny2d_m91_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
    add_dependencies(tiny2d_m91_gl tiny2d_lab_assets)
    add_test(NAME M91_TilemapGL COMMAND tiny2d_m91_gl)
    set_tests_properties(M91_TilemapGL PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 60)
    add_test(NAME M91_MapFiniteCLI COMMAND /usr/bin/env python3
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/m91_finite.py" "$<TARGET_FILE:tiny2d_lab>")
    set_tests_properties(M91_MapFiniteCLI PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 120)
endif()

target_sources(tiny2d_lab_core PRIVATE lab/src/NavigationPresentation.cpp)
target_sources(tiny2d_inspector PRIVATE lab/src/NavigationInspector.cpp)
target_sources(tiny2d_lab PRIVATE lab/src/NavigationApp.cpp)
if(BUILD_TESTING)
    add_executable(tiny2d_m92_search tests/benchmark_m92_search.cpp)
    target_link_libraries(tiny2d_m92_search PRIVATE tiny2d_game)
endif()
if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m92_gl tests/m92_navigation_gl.cpp)
    add_executable(tiny2d_m92_benchmark tests/benchmark_m92.cpp)
    foreach(target tiny2d_m92_gl tiny2d_m92_benchmark)
        target_include_directories(${target} PRIVATE engine/src/Renderer)
        target_link_libraries(${target} PRIVATE tiny2d_inspector glfw OpenGL::GL)
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
        add_dependencies(${target} tiny2d_lab_assets)
    endforeach()
    add_test(NAME M92_NavigationGL COMMAND tiny2d_m92_gl)
    add_test(NAME M92_NavigationFiniteCLI COMMAND /usr/bin/env python3
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/m92_finite.py" "$<TARGET_FILE:tiny2d_lab>")
    set_tests_properties(M92_NavigationGL M92_NavigationFiniteCLI PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 180)
endif()

target_sources(tiny2d_game PRIVATE
    game/src/NavigationSnapshot.cpp game/src/SnapshotCodec.cpp
    game/src/SnapshotCompare.cpp game/src/SnapshotFile.cpp)
if(BUILD_TESTING)
    add_executable(tiny2d_m101_snapshot tests/m101_snapshot.cpp)
    add_executable(tiny2d_m101_snapshot_codec tests/m101_snapshot_codec.cpp)
    target_link_libraries(tiny2d_m101_snapshot PRIVATE tiny2d_game)
    target_link_libraries(tiny2d_m101_snapshot_codec PRIVATE tiny2d_game)
    add_test(NAME M101_SnapshotCPU COMMAND tiny2d_m101_snapshot)
    add_test(NAME M101_SnapshotCodecCPU COMMAND tiny2d_m101_snapshot_codec)
    set_tests_properties(M101_SnapshotCPU M101_SnapshotCodecCPU PROPERTIES LABELS cpu)
endif()

target_sources(tiny2d_game PRIVATE game/src/Replay.cpp game/src/ReplayCodec.cpp)
if(BUILD_TESTING)
    add_executable(tiny2d_m101_replay tests/m101_replay.cpp)
    target_link_libraries(tiny2d_m101_replay PRIVATE tiny2d_game)
    add_test(NAME M101_EventDispatchCPU COMMAND tiny2d_m101_replay dispatch)
    add_test(NAME M101_ReplayCPU COMMAND tiny2d_m101_replay replay)
    add_test(NAME M101_ReplayCodecCPU COMMAND tiny2d_m101_replay codec)
    set_tests_properties(M101_EventDispatchCPU M101_ReplayCPU M101_ReplayCodecCPU PROPERTIES LABELS cpu)
endif()

target_sources(tiny2d_lab PRIVATE lab/src/ReplayApp.cpp)
if(BUILD_TESTING)
    add_executable(tiny2d_m101_benchmark tests/benchmark_m101.cpp)
    target_link_libraries(tiny2d_m101_benchmark PRIVATE tiny2d_game)
    add_executable(tiny2d_m101_evidence tests/m101_evidence.cpp)
    target_link_libraries(tiny2d_m101_evidence PRIVATE tiny2d_game)
    add_test(NAME M101_CrossProcessCPU COMMAND /usr/bin/env python3
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/m101_replay_evidence.py" --tool "$<TARGET_FILE:tiny2d_m101_evidence>")
    set_tests_properties(M101_CrossProcessCPU PROPERTIES LABELS cpu)
endif()
if(BUILD_TESTING AND TINY2D_OPENGL_TESTS)
    add_executable(tiny2d_m101_gl tests/m101_replay_gl.cpp lab/src/ReplayApp.cpp lab/src/NavigationApp.cpp)
    target_include_directories(tiny2d_m101_gl PRIVATE engine/src/Renderer)
    target_link_libraries(tiny2d_m101_gl PRIVATE tiny2d_inspector tiny2d_game glfw OpenGL::GL)
    set_target_properties(tiny2d_m101_gl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lab")
    add_dependencies(tiny2d_m101_gl tiny2d_lab_assets)
    add_test(NAME M101_ReplayGL COMMAND tiny2d_m101_gl)
    add_test(NAME M101_ReplayFiniteCLI COMMAND /usr/bin/env python3
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/m101_finite.py" "$<TARGET_FILE:tiny2d_lab>")
    set_tests_properties(M101_ReplayGL M101_ReplayFiniteCLI PROPERTIES LABELS "opengl;gui" RUN_SERIAL TRUE TIMEOUT 180)
endif()

target_sources(tiny2d_game PRIVATE game/src/Timeline.cpp)
if(BUILD_TESTING)
    add_executable(tiny2d_m102_timeline tests/m102_timeline.cpp)
    target_link_libraries(tiny2d_m102_timeline PRIVATE tiny2d_game)
    add_test(NAME M102_TimelineCPU COMMAND tiny2d_m102_timeline)
    set_tests_properties(M102_TimelineCPU PROPERTIES LABELS cpu)
endif()
