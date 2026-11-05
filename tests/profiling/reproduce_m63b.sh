#!/bin/sh
# Run from repository root. Requires local GLFW/OpenGL and a graphical macOS session.
set -eu
baseline_dir=$(mktemp -d /tmp/tiny2d-m63b-baseline.XXXXXX)
git archive 4be1df0ef4ed7ef46d407f1036895ecd519e4168 | tar -x -C "$baseline_dir"
cp tests/benchmark_m63b.cpp "$baseline_dir/tests/"
cat >> "$baseline_dir/CMakeLists.txt" <<'CMAKE'
add_executable(m63b_baseline tests/benchmark_m63b.cpp)
target_link_libraries(m63b_baseline PRIVATE tiny2d_ui tiny2d_engine glfw OpenGL::GL)
CMAKE
cmake -S "$baseline_dir" -B "$baseline_dir/build" -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build "$baseline_dir/build" --target m63b_baseline --parallel
cmake -S . -B build/m63b-release -DCMAKE_BUILD_TYPE=Release -DTINY2D_OPENGL_TESTS=ON
cmake --build build/m63b-release --target tiny2d_m63b_benchmark --parallel
"$baseline_dir/build/m63b_baseline" > docs/data/m63b/baseline.csv
./build/m63b-release/tiny2d_m63b_benchmark > docs/data/m63b/polish.csv
python3 tests/profiling/summarize_m63b.py
