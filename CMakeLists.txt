
cmake_minimum_required(VERSION 3.20)

project(Tiny2D VERSION 0.1.0 LANGUAGES CXX)

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
