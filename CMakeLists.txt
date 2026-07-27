cmake_minimum_required(VERSION 3.30)

include(cmake/bootstrap_conan.cmake)

project(gbemu-frontend LANGUAGES CXX)

# See libgbemu's CMakeLists.txt for why this is pinned post-project() rather
# than relying on the Conan toolchain's default.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

option(ENABLE_CLANG_TIDY "Enable Clang Tidy" OFF)
option(ENABLE_MSVC_ANALYSIS "Enable MSVC Analysis" OFF)
option(ENABLE_COVERAGE_REPORT "Enable Test Coverage Report" OFF)
option(ENABLE_SANITIZERS "Enable Sanitizers" OFF)

include(cmake/compiler_and_linker_flags.cmake)
include(cmake/clang_format.cmake)
include(cmake/cmake_format.cmake)
include(cmake/setup_static_analysis.cmake)
include(cmake/coverage_report.cmake)

# Vendored source, not a Conan package (yet) - see CLAUDE.md. Deliberately
# add_subdirectory()'d at src/, not libgbemu's own root: its root CMakeLists.txt
# include()s the very same cmake/clang_format.cmake and cmake/cmake_format.cmake
# we already include()d above, which would then try to define the same global
# clang-format/cmake-format targets twice (a hard CMake error) - and even
# without that collision, CMAKE_SOURCE_DIR inside a nested add_subdirectory()
# resolves to *our* root, not libgbemu's, so its copy of those scripts would
# glob our files instead of its own anyway. Going straight to src/ skips all of
# that; the setup_*() functions it calls are already in scope from our own
# include()s above, and ENABLE_TESTS stays unset/OFF so libgbemu's own
# Catch2/test-ROM machinery never builds as part of the frontend.
add_subdirectory(external/libgbemu/src)

add_subdirectory(src)

setup_coverage_report_target()
