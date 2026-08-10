# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The frontend for `libgbemu` (a Game Boy DMG/CGB emulator core, vendored here
as a git submodule at `external/libgbemu`) - opens a window, drives the
render/input loop, and links against `libgbemu`'s `gbemu` C++ module. Written
in C++23 named modules, same as `libgbemu`, built with CMake + CMake Presets.
Windowing/input/audio is SDL3, via Conan for native builds and Emscripten's
own built-in port for the WebAssembly build. The menu bar is ImGui, a Conan
package on *both* platforms - Emscripten included, cross-built from source
since conancenter ships no prebuilt `wasm32-emscripten` binary for it and
Emscripten's own port doesn't cover ImGui (see "Building for the web" below).

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
`dev_ninja_msvc`, `dev_ninja_emscripten`, `release_ninja_emscripten_windows`,
`release_ninja_gcc`, `release_ninja_clang_linux`, etc.). Conan bootstraps
itself automatically on first `cmake --preset` for the native presets (no
separate `conan install` step) - see `cmake/bootstrap_conan.cmake`.

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

Two presets, depending on host and shell:

- **`dev_ninja_emscripten`** - Linux-only (its `condition` checks
  `hostSystemName == Linux`); the one to use from a real Linux box or CI.
- **`release_ninja_emscripten_windows`** - for a Windows host. `condition`
  only checks `hostSystemName == Windows`, but the real requirement is
  narrower than that: `Emscripten.cmake` shells out to a literal `emcc`
  (no `.exe`), so this only works from a shell that can execute `emcc`'s
  shebang script directly. **Confirmed working from Git Bash** (this repo's
  own `Bash` tool included) with `$EMSDK` set by sourcing a Windows emsdk
  clone's `emsdk_env.sh` (yes, `.sh` - emsdk ships it even on a Windows
  install, and Git Bash/MSYS2 can run it). Plain `cmd.exe`/PowerShell after
  `emsdk_env.bat` is untested here - the preset's own `displayName` implies
  that path is intended to work too, but don't assume it without checking;
  `emcc.exe` alone (no shebang needed) is NOT sufficient for
  `Emscripten.cmake`'s toolchain probing, only `emcc` is looked for by name.
  Previously this file claimed a Windows emsdk plus WSL interop couldn't
  satisfy this at all - that was wrong, or at least incomplete: it just
  hadn't been tried from a shell that actually runs `emcc` as the shell
  script it is.

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh   # sets $EMSDK, which the preset's toolchainFile needs
cd ..
embuilder build sdl3     # see below for why this goes first, on a cold cache
cmake --preset dev_ninja_emscripten                 # Linux
cmake --preset release_ninja_emscripten_windows     # Windows, from Git Bash
cmake --build --preset dev_ninja_emscripten                 # Linux
cmake --build --preset release_ninja_emscripten_windows     # Windows
node web/gbemu.js   # smoke-test only, see below for what to expect
```

**Windows-specific flakiness**: `emscan-deps.exe`'s dependency-scan step
(writes a `.ddi.tmp` file, then `cmake -E rename`s it) intermittently failed
here with `The process cannot access the file because it is being used by
another process` - repeatably enough on one specific `.ddi.tmp` (not just a
one-off) that a plain re-run of the same `cmake --build` didn't clear it.
Configuring into a fresh, never-before-used binary dir (`cmake --preset
release_ninja_emscripten_windows -B builds/some-new-dir`) did. Not root-caused
(a lingering `python.exe` from emsdk's own tooling was observed holding a
handle at one point, but killing it alone didn't fix a from-that-same-stale-dir
retry) - if hit, don't fight the existing build dir, just point `-B` at a new
one. `RUNTIME_OUTPUT_DIRECTORY` is `${CMAKE_SOURCE_DIR}/web` regardless of
which binary dir configured it, so this doesn't change where `gbemu.js`/
`gbemu.wasm` end up.

**What the Node smoke-test actually shows**: it does *not* fail at
`SDL_Init`/`SDL_INIT_VIDEO` - it gets further than that, into
`emscripten_asm_const_int_sync_on_main_thread`, and throws
`ReferenceError: window is not defined` (Node has no DOM `window` global,
which the SDL3 Emscripten backend's video init reaches for). That's the
expected failure mode confirming the module loaded and started up correctly;
it's still not a real test of anything past that point - a real browser
canvas is still required for that.

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

**ImGui, unlike SDL3 above, *is* Conan-cross-built for Emscripten** - there's
no Emscripten-native port of it to fall back on instead.
`cmake/bootstrap_emscripten_conan.cmake` (included from the top-level
`CMakeLists.txt` right after `project()`, so `EMSCRIPTEN` is already known)
runs a standalone `conan install --requires=imgui/1.92.8` against a dedicated
profile, `cmake/emscripten_conan_profile`
(`os=Emscripten`/`arch=wasm`/`compiler=emcc` - Conan 2 has this as a
first-class built-in setting; confirmed directly before wiring any of this
in that it actually builds a real `libimgui.a` via `em++`) - deliberately
not routed through this repo's normal `conan install <sourceDir>`, since
that would install conanfile.txt's full `[requires]` list (sdl included)
against this profile too, cross-building SDL3 from source for wasm for no
reason (SDL3 already comes from Emscripten's own port, see above). Two
non-obvious problems had to be solved to get this far, both specific to
cross-compiling a *second*, unrelated package this way while the main
project itself uses `Emscripten.cmake` as its toolchain:

- **`find_package(imgui CONFIG)` silently ignored the freshly-added
  `CMAKE_PREFIX_PATH` entry.** `Emscripten.cmake` sets
  `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE` to `ONLY` (general cross-compilation
  sandboxing - restricts `find_package` to `CMAKE_FIND_ROOT_PATH`, the
  emscripten sysroot, alone) unless something already set it beforehand.
  Conan's CMakeDeps output for this package was obviously never going to
  live inside that sysroot, so `bootstrap_emscripten_conan.cmake` overrides
  it back to `BOTH` right after extending `CMAKE_PREFIX_PATH`.
- **`with_sdl3_binding=True` (the option native's `conanfile.txt` uses)
  doesn't work here.** Opting into it makes `imgui::imgui`'s own generated
  CMake target declare a hard dependency on an `sdl::sdl` target - meaning a
  *second*, Conan-cross-built SDL3 would get linked into `gbemu` alongside
  Emscripten's own real one (`-sUSE_SDL=3`), two different SDL3
  implementations in one binary. The Emscripten install leaves
  `with_sdl3_binding` at its recipe default (`False`) instead, and
  `src/CMakeLists.txt` compiles `imgui_impl_sdl3.cpp` itself from the
  package's `res/bindings/` directory to make up for it - the exact same
  loose-source-file treatment `imgui_impl_sdlrenderer3.cpp` already gets on
  *both* platforms (that backend was never part of `with_sdl3_binding` to
  begin with, on either platform).

## Architecture

**Module structure**: mirrors `libgbemu`'s interface/implementation split, but
without a separate `modules/` directory - `libgbemu` keeps every `.cppm`
interface unit (its primary unit and all partitions alike) together in
`modules/`, apart from the `.cpp` implementation files in `src/`; this repo
isn't consumed by anything and has only one interface unit, so
`frontend.cppm` (the primary module interface unit, defining the `App` facade
directly - same pattern as `libgbemu`'s `modules/gbemu.cppm` defining
`GameBoy` directly) lives straight in `src/`, paired with `src/frontend.cpp`
(`module frontend;` +
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

**`release_ninja_clang` needs `CMAKE_TRY_COMPILE_CONFIGURATION: Release`
explicitly set (already in the preset)** - without it, CMake's own internal
ABI-detection `try_compile` (`CMakeDetermineCompilerABI.cmake`) hardcodes its
throwaway probe project's config to `Debug` regardless of the outer build's
`CMAKE_BUILD_TYPE`. Conan's toolchain sets `CMAKE_MSVC_RUNTIME_LIBRARY` as a
generator expression keyed on the *outer* config
(`$<$<CONFIG:Release>:MultiThreadedDLL>` here); inside that always-`Debug`
probe project the genex's condition never matches, so it resolves to an empty
string, no `-Xclang --dependent-lib=` gets embedded in the probe's object
file, and linking it fails with `lld-link: error: <root>: undefined symbol:
mainCRTStartup` - CMake then reports "Check for working CXX compiler -
broken" and aborts configure entirely, before this repo's own CMakeLists.txt
code ever runs. `dev_ninja_clang`'s equivalent genex
(`$<$<CONFIG:Debug>:MultiThreadedDebugDLL>`) never hit this: it only "works"
because it happens to match the probe's hardcoded `Debug`, not because
Debug configs are actually exempt from the underlying bug - confirmed by
inspecting the preserved probe project (`cmake --debug-trycompile`) under
`CMakeFiles/CMakeScratch/TryCompile-*`, whose generated `build.ninja`
literally reads `# Configurations: Debug` no matter what the outer preset's
`CMAKE_BUILD_TYPE` is. Every other preset
either isn't Clang-on-Windows (no `CMAKE_MSVC_RUNTIME_LIBRARY` genex
involved) or is already `Debug` (`dev_ninja_clang*`) - `release_ninja_clang`
was the first Release-config Windows+Clang preset in this repo, which is why
this had never surfaced before.

## Monitoring CI

Same as `libgbemu` - `gh` is available and authenticated:

```
gh run list --branch main --limit 5
gh run view <run-id>                # job/step summary
gh run view <run-id> --log-failed   # full logs of only the failed steps
```
