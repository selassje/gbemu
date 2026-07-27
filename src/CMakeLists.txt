add_executable(gbemu-frontend)

target_sources(
  gbemu-frontend
  PUBLIC FILE_SET
         public_modules
         TYPE
         CXX_MODULES
         BASE_DIRS
         ${CMAKE_CURRENT_LIST_DIR}
         FILES
         ${CMAKE_CURRENT_LIST_DIR}/frontend.cppm)

if(MSVC)
  # See libgbemu's src/CMakeLists.txt for why this is needed - same VS
  # IntelliSense /interface-inference bug, same fix.
  set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/frontend.cppm
                              PROPERTIES COMPILE_OPTIONS /interface)
endif()

target_sources(gbemu-frontend PRIVATE ${CMAKE_CURRENT_LIST_DIR}/frontend.cpp
                                      ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

# Same clang+MSVC-STL std module workaround as libgbemu's src/CMakeLists.txt -
# kept verbatim since it's the identical toolchain gap.
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows"
   AND ${CMAKE_GENERATOR} STREQUAL "Ninja"
   AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  set(VC_TOOLS_INSTALL_DIR "$ENV{VcToolsInstallDir}")
  set(STD_MODULE_DIR ${VC_TOOLS_INSTALL_DIR}/modules)
  set(STD_MODULE ${STD_MODULE_DIR}/std.ixx)

  target_sources(
    gbemu-frontend
    PUBLIC FILE_SET
           std_module
           TYPE
           CXX_MODULES
           BASE_DIRS
           ${STD_MODULE_DIR}
           FILES
           ${STD_MODULE_DIR}/std.ixx)

  set_source_files_properties(${STD_MODULE_DIR}/std.ixx PROPERTIES SKIP_LINTING
                                                                   ON)
endif()

target_link_libraries(gbemu-frontend PRIVATE gbemu)

if(EMSCRIPTEN)
  # No Conan for wasm - emscripten's own SDL3 port, pulled in purely via
  # compile/link flags (see sdl3-config.cmake in the emscripten sysroot).
  target_compile_options(gbemu-frontend PRIVATE -sUSE_SDL=3)
  target_link_options(gbemu-frontend PRIVATE -sUSE_SDL=3
                      -sALLOW_MEMORY_GROWTH=1)
else()
  find_package(SDL3 REQUIRED CONFIG)
  target_link_libraries(gbemu-frontend PRIVATE SDL3::SDL3)
endif()

setup_compiler_warnings(gbemu-frontend)
setup_clang_tidy(gbemu-frontend)
setup_msvc_analysis(gbemu-frontend)
setup_coverage_report_flags(gbemu-frontend)
setup_sanitizers(gbemu-frontend)
setup_std_lib(gbemu-frontend)

if(EMSCRIPTEN)
  # Must come after setup_compiler_warnings(): emscan-deps's own diagnostics
  # processes -W flags left-to-right, and -Wall/-Werror (added above) would
  # otherwise re-enable/re-error this after an earlier -Wno-experimental -
  # confirmed by testing both orderings directly. The warning itself is the sdl3
  # port announcing its own maturity on every TU that sees -sUSE_SDL=3
  # (including scanning the synthesized std module for this target's flag set),
  # not anything about our code.
  target_compile_options(gbemu-frontend PRIVATE -Wno-experimental)
endif()
