# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The frontend for `libgbemu` (a Game Boy DMG/CGB emulator core, vendored here
as a git submodule at `external/libgbemu`) - opens a window, drives the
render/input loop, and links against `libgbemu`'s `gbemu` C++ module. Written
in C++23 named modules, same as `libgbemu`, built with CMake + CMake Presets.
Windowing/input/audio is SDL3, via Conan for native builds and Emscripten's
own built-in port for the WebAssembly build.

**Naming note**: the executable *CMake target* built by this repo is also
called `gbemu` - a coincidence with the `gbemu` C++ module name (`import
gbemu;`, from `libgbemu`) it links against, not the same thing. The library
*CMake target* those C++ module declarations live in is `libgbemu` (renamed
from its former `gbemu` specifically to free up that name for this
executable), still exposing an `import gbemu;`-named module either way. Watch
for which sense a given `gbemu` refers to.

**Current state:** `App::run(romPath)` loads the ROM at the given path,
constructs a `gbemu::GameBoy`, and runs a loop that calls `runNextFrame()`,
blits the resulting pixel buffer into an SDL3 streaming texture, and presents
it, paced to 60 FPS (native: a manual `SDL_Delay` after each frame; Emscripten:
`emscripten_set_main_loop_arg`'s own `fps` argument). `main.cpp` treats
`argv[1]` as the ROM path. Keypad input is not read yet - that's the natural
next step.

## Build / test commands

First-time setup requires the submodule:

```
git submodule update --init --recursive
```

Building is preset-driven, same as `libgbemu` - pick one from
`CMakePresets.json` (`dev_ninja_gcc`, `dev_ninja_clang_linux`,
`dev_ninja_clang_tidy_linux`, `dev_ninja_clang_coverage_linux`,
`dev_ninja_msvc`, `dev_ninja_emscripten`, `release_ninja_gcc`,
`release_ninja_clang_linux`, etc.). Conan bootstraps itself automatically on
first `cmake --preset` for the native presets (no separate `conan install`
step) - see `cmake/bootstrap_conan.cmake`.

**Native Linux presets need SDL3's system dependencies.** Unlike `libgbemu`,
this repo pulls in `sdl` via Conan, and SDL3's recipe needs a long list of
Linux X11/Wayland/EGL/ALSA dev packages to build from source whenever
conancenter has no prebuilt binary matching the exact
compiler/version/stdlib combo (common - hit locally and in CI both).
`cmake/bootstrap_conan.cmake` sets `tools.system.package_manager:mode=install`
+ `:sudo=True` so Conan installs whatever's missing itself via `apt`
automatically (works out of the box with passwordless sudo, e.g. GitHub
Actions runners); on a dev machine without passwordless sudo this may still
prompt/fail on first configure, in which case install the packages Conan's
error message lists manually, then reconfigure.

```
cmake --preset dev_ninja_gcc
cmake --build --preset dev_ninja_gcc
```

There is no `ctest`/test target yet (see Architecture below for why), so
there's no `ctest --preset ...` step for any preset currently.

Formatting (dedicated build targets, not a separate script, same as
`libgbemu`):

```
ninja clang-format          # or clang-format-check for a dry-run/CI-style check
ninja cmake-format          # or cmake-format-check
```

### Building for the web (Emscripten)

Requires a **Linux-native emsdk**, not the Windows one if you also have one
of those around for other projects - `Emscripten.cmake` shells out to a
literal `emcc` (no `.exe`), which a Windows emsdk install won't provide even
under WSL interop (confirmed while setting this preset up: `emcc.exe` runs
fine standalone via interop, but CMake's toolchain file still fails to find
plain `emcc`).

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh   # sets $EMSDK, which the preset's toolchainFile needs
cd ..
embuilder build sdl3     # see below for why this goes first, on a cold cache
cmake --preset dev_ninja_emscripten
cmake --build --preset dev_ninja_emscripten
node builds/dev_ninja_emscripten/build/gbemu.js   # smoke-test only; SDL_INIT_VIDEO needs a real browser canvas
```

**Cold-cache first build: pre-build the SDL3 port with `embuilder build sdl3`
before `cmake --build`.** The port builds lazily (first TU that needs it
triggers the build), and on a cold cache that build isn't safe against
multiple `ninja` jobs hitting that trigger concurrently - hit for real in CI:
`AssertionError: attempt to lock the cache while a parent process is holding
the lock`. Once the port's built once (this machine's emsdk cache, or CI's
if it ever gets an actions/cache step), subsequent builds don't need this -
only a genuinely first-time/cold cache does.

**C++20 modules + Emscripten + the SDL3 port do work together**, verified
directly (build+link, with a real `SDL_Init`/SDL call) against emsdk `6.0.4`
before this was wired into the real build. Upstream had open issues about
`emscan-deps` choking on `-s` settings flags like `-sUSE_SDL=3`
([#24454](https://github.com/emscripten-core/emscripten/issues/24454)) - not
reproducible on `6.0.4`, looks fixed since those were filed. If a future
emsdk regresses this, the fallback is compiling `src/frontend.cpp` as a
non-modular translation unit for the `EMSCRIPTEN` case only (guarded in
`src/CMakeLists.txt`); `libgbemu`'s own modules are unaffected either way
since they never see SDL flags.

`CMAKE_CXX_MODULE_STD=ON` + `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD=f35a9ac6-...`
(same cache variables `libgbemu`'s own `_linux`-suffixed clang presets use)
is required for `import std;` to resolve under Emscripten - without it, `import
std;` fails with "module 'std' not found" even though plain named-module
scanning/building otherwise works.

SDL3 comes from Emscripten's own port (`-sUSE_SDL=3` compile+link flags, set
in `src/CMakeLists.txt` when `EMSCRIPTEN` is set) instead of Conan - Conan's
`sdl` package has no prebuilt `wasm32-emscripten` binaries, so using it there
would mean Conan cross-building SDL3 from source for strictly no benefit over
the built-in port.

## Architecture

**Module structure**: mirrors `libgbemu`'s interface/implementation split, but
without a separate `import/` directory - `libgbemu` splits that out because
it's a library meant to be consumed externally (the public entry point vs.
internal partitions); this repo isn't consumed by anything, so `frontend.cppm`
(the primary module interface unit, defining the `App` facade directly - same
pattern as `libgbemu`'s `import/gbemu.cppm` defining `GameBoy` directly) lives
straight in `src/`, paired with `src/frontend.cpp` (`module frontend;` +
`namespace frontend { ... }`). `App` hides SDL's window/renderer handles
behind an incomplete
`Impl` (pimpl) so the interface unit itself never needs `<SDL3/SDL.h>` -
unlike `libgbemu`'s `Cpu`/`Mmu`/`Ppu`, which don't need this since they don't
wrap a third-party C API. `src/main.cpp` is a plain non-module TU (`import
frontend;`), matching `libgbemu`'s own pattern of keeping the module boundary
at the "library" layer, not the entry point.

**Submodule, not (yet) a Conan package.** `external/libgbemu` (its own root,
not just `src/`) is `add_subdirectory()`'d from the top-level
`CMakeLists.txt`. This works cleanly because libgbemu's own root
`CMakeLists.txt` guards its `clang-format`/`cmake-format`/coverage-report
target creation behind CMake's built-in `PROJECT_IS_TOP_LEVEL` (auto-set by
`project()`, true only when a project is the outermost one being configured):
without that guard, its `include(cmake/clang_format.cmake)` etc. would try to
define the same global `clang-format`/`cmake-format` targets this repo's own
top-level already defines (hard CMake error), and separately
`CMAKE_SOURCE_DIR` inside a nested `add_subdirectory()` resolves to *this*
repo's root, not libgbemu's, so its copy of those scripts would glob this
repo's files instead of its own even without the name collision. That guard
lives in `external/libgbemu/CMakeLists.txt` itself - a change to the
submodule's own repo, committed/pushed there separately from this repo's
history, not something this repo's `.git` tracks. `ENABLE_TESTS` stays
unset/off here so libgbemu's Catch2/test-ROM machinery never builds as part
of the frontend. A Conan-package path for `libgbemu` is possible later but
intentionally not done now.

**Why `cmake/coverage_report.cmake` differs from `libgbemu`'s copy**: it
originally does `get_property(... DIRECTORY ${CMAKE_SOURCE_DIR}/tests ...)`,
which hard-errors at configure time if `tests/` was never `add_subdirectory`'d
- this repo has no `tests/` yet, so it was repointed at `src/` (where the
`gbemu` executable itself lives) instead, and the coverage report's
`-ignore-filename-regex` was changed from `tests` to `external` so the report
reflects this repo's own code, not the vendored `libgbemu` submodule.

**Why CI has no `ctest`/coverage steps** (unlike `libgbemu`'s): no test
target exists yet (see above), and `gbemu` is a windowed app with nothing
exercising it headlessly. Both should come back once real tests
exist - re-add `ENABLE_TESTS` wiring plus a `tests/` directory, then restore
the `Run Tests`/`Generate Coverage Report`/`Upload Coverage to Codecov` steps
`.github/workflows/ci.yml` had in `libgbemu` before trimming them here.

**Known-harmless CMake diagnostic on `dev_ninja_clang_tidy`/other
Windows+Clang presets**: `CMake Error: Disagreement of the location of the
'std' module` printed 2-3 times during configure, referencing
`external/libgbemu/src/CMakeFiles/libgbemu.dir/std.pcm` vs.
`src/CMakeFiles/gbemu.dir/std.pcm`. Cosmetic, not fatal (seen in a
real CI run that built 28/36 targets past it and only failed on an unrelated
`clang-tidy` finding) - it's `libgbemu`'s and this repo's `src/CMakeLists.txt`
each independently attaching their own copy of the manual clang+MSVC-STL
`std.ixx` workaround (see comment there) for their own target, which only
matters here because this is the first time that workaround is shared by
*two different targets* in one build rather than `libgbemu`'s usual
single-target case. Worth revisiting if CMake ever escalates this from a
diagnostic to a real error.

## Monitoring CI

Same as `libgbemu` - `gh` is available and authenticated:

```
gh run list --branch main --limit 5
gh run view <run-id>                # job/step summary
gh run view <run-id> --log-failed   # full logs of only the failed steps
```
