# Emscripten builds don't use Conan for the project's own toolchain (see
# CMakeLists.txt's include(cmake/bootstrap_conan.cmake), which only fires when
# CMAKE_TOOLCHAIN_FILE - here, Emscripten.cmake itself, which always exists once
# emsdk's installed - is missing) or for SDL3 (pulled in via Emscripten's own
# port instead - see src/CMakeLists.txt). ImGui is the one exception:
# conancenter has no prebuilt wasm32-emscripten binary for it, and there's no
# Emscripten-native package manager equivalent, so this cross-builds it from
# source via Conan's own os=Emscripten/compiler=emcc support
# (cmake/emscripten_conan_profile) instead - confirmed working directly before
# wiring this in. Deliberately not routed through this repo's normal `conan
# install <sourceDir>` (that would install conanfile.txt's full [requires] list
# - sdl included - against this profile, trying to cross-build SDL3 from source
# for wasm too, which is unwanted since SDL3 already comes from Emscripten's own
# port here).

if(NOT EMSCRIPTEN)
  return()
endif()

set(EMSCRIPTEN_IMGUI_CONAN_DIR
    "${CMAKE_BINARY_DIR}/generators/emscripten_imgui")

if(NOT EXISTS "${EMSCRIPTEN_IMGUI_CONAN_DIR}/imgui-config.cmake")
  message(
    STATUS
      "imgui (Emscripten/wasm) not found at ${EMSCRIPTEN_IMGUI_CONAN_DIR} - bootstrapping via 'conan install'"
  )

  get_filename_component(CMAKE_BIN_DIR "${CMAKE_COMMAND}" DIRECTORY)
  if(CMAKE_HOST_WIN32)
    set(PATH_LIST_SEP ";")
  else()
    set(PATH_LIST_SEP ":")
  endif()

  # Debug is this project's only Emscripten CMAKE_BUILD_TYPE so far
  # (dev_ninja_emscripten) other than release_ninja_emscripten_windows's Release
  # - either way CMAKE_BUILD_TYPE is already set by the time this runs (see
  # CMakeLists.txt: this include() comes after project()). with_sdl3_binding
  # left at its recipe default (False) here, unlike native's conanfile.txt -
  # opting in pulls in a Conan-cross-built sdl/3.4.8 as a hard dependency of
  # imgui::imgui's own generated CMake target (confirmed directly: `sdl::sdl`
  # target-not-found errors at generate time without this), which would then
  # also get linked into gbemu alongside Emscripten's own real SDL3
  # (-sUSE_SDL=3) - two different SDL3 implementations in one binary.
  # imgui_impl_sdl3.cpp is instead compiled ourselves against Emscripten's SDL3
  # headers, same as imgui_impl_sdlrenderer3.cpp already is for both platforms -
  # see src/CMakeLists.txt.
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env
      "PATH=${CMAKE_BIN_DIR}${PATH_LIST_SEP}$ENV{PATH}" conan install
      --requires=imgui/1.92.8 -pr:h
      "${CMAKE_SOURCE_DIR}/cmake/emscripten_conan_profile" -g CMakeDeps -of
      "${EMSCRIPTEN_IMGUI_CONAN_DIR}" -s "build_type=${CMAKE_BUILD_TYPE}"
      --build=missing -c tools.cmake.cmaketoolchain:user_presets=
    RESULT_VARIABLE EMSCRIPTEN_IMGUI_CONAN_RESULT)

  if(NOT EMSCRIPTEN_IMGUI_CONAN_RESULT EQUAL 0)
    message(
      FATAL_ERROR
        "conan install (Emscripten imgui) failed (exit ${EMSCRIPTEN_IMGUI_CONAN_RESULT})"
    )
  endif()

  if(NOT EXISTS "${EMSCRIPTEN_IMGUI_CONAN_DIR}/imgui-config.cmake")
    message(
      FATAL_ERROR
        "conan install ran but imgui-config.cmake still missing: ${EMSCRIPTEN_IMGUI_CONAN_DIR}"
    )
  endif()
endif()

list(APPEND CMAKE_PREFIX_PATH "${EMSCRIPTEN_IMGUI_CONAN_DIR}")

# Emscripten.cmake sets this to ONLY (confirmed by reading it directly) as
# general cross-compilation sandboxing, restricting find_package() to
# CMAKE_FIND_ROOT_PATH (the emscripten sysroot) alone - which would silently
# make src/CMakeLists.txt's find_package(imgui CONFIG) ignore the
# CMAKE_PREFIX_PATH entry just added above (confirmed directly: it failed with
# exactly this symptom before adding this override). BOTH restores the normal
# behavior of also searching CMAKE_PREFIX_PATH, needed since Conan's CMakeDeps
# output for a Conan-cross-built package like this one was never going to live
# inside the emscripten sysroot itself.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
