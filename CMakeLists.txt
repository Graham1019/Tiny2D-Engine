
cmake_minimum_required(VERSION 3.20)

project(Tiny2D VERSION 0.1.0 LANGUAGES CXX)

# 引擎静态库
add_library(tiny2d_engine STATIC
    engine/src/Math/Vector2.cpp
)

target_include_directories(tiny2d_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/include"
)

target_compile_features(tiny2d_engine PUBLIC cxx_std_17)

set_target_properties(tiny2d_engine PROPERTIES
    CXX_EXTENSIONS OFF
)

# 游戏测试程序
add_executable(tiny2d_sandbox
    sandbox/src/main.cpp
)

target_link_libraries(tiny2d_sandbox PRIVATE
    tiny2d_engine
)

set_target_properties(tiny2d_sandbox PROPERTIES
    CXX_EXTENSIONS OFF
)

# Enable testing with CTest
include(CTest)

if(BUILD_TESTING)
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
endif()
