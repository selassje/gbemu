add_executable(gbemu)

target_sources(
  gbemu
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

target_sources(gbemu PRIVATE ${CMAKE_CURRENT_LIST_DIR}/frontend.cpp
                             ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

# Same clang+MSVC-STL std module workaround as libgbemu's src/CMakeLists.txt -
# kept verbatim since it's the identical toolchain gap. Excludes Emscripten:
# emcc also reports CMAKE_CXX_COMPILER_ID "Clang" (it's a clang wrapper), but
# on a Windows host targeting wasm there's no MSVC STL involved at all - the
# Emscripten toolchain provides its own std module via
# CMAKE_CXX_MODULE_STD/CMAKE_EXPERIMENTAL_CXX_IMPORT_STD instead (see the
# dev_ninja_emscripten/release_ninja_emscripten* presets), and
# $ENV{VcToolsInstallDir} is unset outside an MSVC dev shell, which without
# this guard resolved STD_MODULE_DIR to a bare "/modules" and failed to find
# "/modules/std.ixx".
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows"
   AND ${CMAKE_GENERATOR} STREQUAL "Ninja"
   AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang"
   AND NOT EMSCRIPTEN)
  set(VC_TOOLS_INSTALL_DIR "$ENV{VcToolsInstallDir}")
  set(STD_MODULE_DIR ${VC_TOOLS_INSTALL_DIR}/modules)
  set(STD_MODULE ${STD_MODULE_DIR}/std.ixx)

  target_sources(
    gbemu
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

target_link_libraries(gbemu PRIVATE libgbemu)

if(EMSCRIPTEN)
  # No Conan for wasm - emscripten's own SDL3 port, pulled in purely via
  # compile/link flags (see sdl3-config.cmake in the emscripten sysroot).
  target_compile_options(gbemu PRIVATE -sUSE_SDL=3)
  # -lidbfs.js: links the IDBFS filesystem backend web/script.js mounts
  # (FS.mount(IDBFS, ...)) so uploaded ROMs persist across page reloads.
  target_link_options(gbemu PRIVATE -sUSE_SDL=3 -sALLOW_MEMORY_GROWTH=1
                      -lidbfs.js)

  # Places gbemu.js/.wasm directly alongside web/'s own tracked files
  # (index.html, styles.css, script.js, favicon) so the whole directory is ready
  # to serve/deploy as-is after a build - see web's own .gitignore for why the
  # build outputs themselves aren't tracked.
  set_target_properties(gbemu PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                         ${CMAKE_SOURCE_DIR}/web)
else()
  find_package(SDL3 REQUIRED CONFIG)
  target_link_libraries(gbemu PRIVATE SDL3::SDL3)

  # Dear ImGui, for the menu bar (File > Open ROM...) - native builds only, same
  # as SDL3 above; not offered to the Emscripten build (see frontend.cpp's
  # __EMSCRIPTEN__ guards around every ImGui call).
  find_package(imgui REQUIRED CONFIG)
  target_link_libraries(gbemu PRIVATE imgui::imgui)

  # with_sdl3_binding (see conanfile.txt) only builds imgui's SDL3
  # platform/event backend into imgui::imgui - the SDL_Renderer backend
  # (imgui_impl_sdlrenderer3) isn't part of that option and isn't built into the
  # library at all; conan's recipe ships it as a loose source file under the
  # package's res/bindings, meant to be compiled straight into the consumer.
  # imgui_INCLUDE_DIR (.../include) is a normal, non-generator-expr path here
  # because this project only ever configures a single Conan build_type per
  # binary dir (see CMakePresets.json), so walking up from it to the package
  # root is safe.
  cmake_path(GET imgui_INCLUDE_DIR PARENT_PATH IMGUI_PACKAGE_ROOT)
  set(IMGUI_BACKENDS_DIR ${IMGUI_PACKAGE_ROOT}/res/bindings)

  # A separate OBJECT library, not a source added straight to gbemu: it keeps
  # this vendored backend file out from under setup_compiler_warnings() /-Werror
  # below, which it wasn't written to satisfy.
  add_library(imgui_sdlrenderer3_backend OBJECT
              ${IMGUI_BACKENDS_DIR}/imgui_impl_sdlrenderer3.cpp)
  target_include_directories(imgui_sdlrenderer3_backend
                             PUBLIC ${IMGUI_BACKENDS_DIR})
  target_link_libraries(imgui_sdlrenderer3_backend PRIVATE imgui::imgui
                                                           SDL3::SDL3)

  target_link_libraries(gbemu PRIVATE imgui_sdlrenderer3_backend)
endif()

setup_compiler_warnings(gbemu)
setup_clang_tidy(gbemu)
setup_msvc_analysis(gbemu)
setup_coverage_report_flags(gbemu)
setup_sanitizers(gbemu)
setup_std_lib(gbemu)

if(EMSCRIPTEN)
  # Must come after setup_compiler_warnings(): emscan-deps's own diagnostics
  # processes -W flags left-to-right, and -Wall/-Werror (added above) would
  # otherwise re-enable/re-error this after an earlier -Wno-experimental -
  # confirmed by testing both orderings directly. The warning itself is the sdl3
  # port announcing its own maturity on every TU that sees -sUSE_SDL=3
  # (including scanning the synthesized std module for this target's flag set),
  # not anything about our code.
  target_compile_options(gbemu PRIVATE -Wno-experimental)
endif()
