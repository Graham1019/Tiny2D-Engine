
cmake_minimum_required(VERSION 3.20)

project(Tiny2D VERSION 0.4.0 LANGUAGES CXX)

if(NOT APPLE)
    message(FATAL_ERROR "M2 renderer currently supports macOS system OpenGL only.")
endif()

# Use locally installed dependencies; never fetch sources implicitly.
find_package(glfw3 3.3 QUIET CONFIG)
if(NOT TARGET glfw)
    message(FATAL_ERROR "GLFW 3.3+ not found. On macOS: brew install glfw; configure with -DCMAKE_PREFIX_PATH=$(brew --prefix) if needed.")
endif()
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED) # Local libpng; brew install libpng. No configure-time downloads.

# 引擎静态库
add_library(tiny2d_engine STATIC
    engine/src/Scene/Scene.cpp
    engine/src/Physics/Collision2D.cpp
    engine/src/Renderer/SceneRenderer.cpp
    engine/src/Math/Vector2.cpp
    engine/src/Math/Mat4.cpp
    engine/src/Renderer/Camera2D.cpp
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

add_library(tiny2d_ui STATIC sandbox/src/ui/DifficultyMenu.cpp sandbox/src/ui/BitmapFont.cpp sandbox/src/ui/GameHUD.cpp)
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
