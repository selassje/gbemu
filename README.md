# gbemu

[![gbemu CI](https://github.com/selassje/gbemu/actions/workflows/ci.yml/badge.svg)](https://github.com/selassje/gbemu/actions/workflows/ci.yml)
[![gbemu CD](https://github.com/selassje/gbemu/actions/workflows/cd.yml/badge.svg)](https://github.com/selassje/gbemu/actions/workflows/cd.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

The frontend for [libgbemu](https://github.com/selassje/libgbemu), a Game Boy
(DMG) / Game Boy Color (CGB) emulator core. Opens a window, drives the
render/input/audio loop, and links against libgbemu's `gbemu` C++ module.
Written in modern C++23 (named modules), built with CMake + CMake Presets,
using SDL3 for windowing/input/audio and Dear ImGui for the native build's
menu bar.

**[Play in your browser](https://selassje.github.io/gbemu/)** - runs the same
core compiled to WebAssembly via Emscripten, no download or build required.

## Features

- Native desktop build (Linux/Windows) and a browser build (WebAssembly via
  Emscripten), from the same source.
- File > Open ROM (native) / an upload button (web) to load a ROM at
  runtime - no ROM path required to launch, a built-in placeholder cartridge
  keeps the window/menu usable until one is picked.
- `auto`/`dmg`/`cgb` mode selection (native, via a command-line argument) -
  `auto` follows the cartridge header, same as real hardware.
- Uploaded ROMs on the web build persist across reloads (IndexedDB-backed).

## Controls

| Game Boy | Key         |
| -------- | ----------- |
| A        | X           |
| B        | Z           |
| Select   | Backspace   |
| Start    | Enter       |
| Up/Down/Left/Right | Arrow keys |

## Build / run

First-time setup requires the submodule:

```
git submodule update --init --recursive
```

Native (pick any preset from `CMakePresets.json`, e.g. `dev_ninja_gcc`):

```
cmake --preset dev_ninja_gcc
cmake --build --preset dev_ninja_gcc
./builds/dev_ninja_gcc/build/src/gbemu [rom-file] [auto|dmg|cgb]
```

Web (requires a Linux-native [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) -
see `CLAUDE.md` for setup details):

```
cmake --preset release_ninja_emscripten
cmake --build --preset release_ninja_emscripten
cd web && python3 -m http.server 8000   # then open http://localhost:8000
```

The web build outputs `gbemu.js`/`gbemu.wasm` directly into `web/`, alongside
its tracked `index.html`/`styles.css`/`script.js` - the directory is
deploy-ready as-is after a build. `.github/workflows/cd.yml` builds and
deploys it to GitHub Pages on every push to `main`.

See `CLAUDE.md` for the full list of build presets, Emscripten-specific
setup notes, and architecture details.

## License

MIT - see [LICENSE](LICENSE). `libgbemu` (the core this links against) is a
separate repository with its own license and attributions.
