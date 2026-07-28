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

# Vendored source, not a Conan package (yet) - see CLAUDE.md. libgbemu's own
# root CMakeLists.txt guards its clang-format/cmake-format/coverage-report
# target creation behind PROJECT_IS_TOP_LEVEL, so it's safe to
# add_subdirectory() directly here without colliding with the identically-named
# targets this repo's own cmake/clang_format.cmake etc. already define.
# ENABLE_TESTS stays unset/OFF so libgbemu's own Catch2/test-ROM machinery never
# builds as part of the frontend.
add_subdirectory(external/libgbemu)

add_subdirectory(src)

setup_coverage_report_target()
