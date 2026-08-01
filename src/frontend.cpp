module;

// C headers belong in the global module fragment (before `module frontend;`
// below), not after it - two separate #includes of overlapping system
// headers both attributed to "module frontend" (rather than one of them
// living in the unattached global module) is what caused a real
// "declaration ... in the global module follows declaration in module
// frontend" conflict between SDL3's and emscripten's headers when this was
// tried the other way round.
#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#endif

module frontend;

import gbemu;

namespace frontend {

namespace {

constexpr int WINDOW_SCALE = 3;
constexpr int WINDOW_WIDTH =
  static_cast<int>(gbemu::SCREEN_WIDTH) * WINDOW_SCALE;
constexpr int WINDOW_HEIGHT =
  static_cast<int>(gbemu::SCREEN_HEIGHT) * WINDOW_SCALE;
constexpr const char* WINDOW_TITLE = "gbemu";
constexpr int TARGET_FPS = 60;
constexpr double TARGET_FRAME_MS = 1000.0 / TARGET_FPS;

// Fixed physical-key layout (scancode-based, so it stays put regardless of
// keyboard locale/layout) - not user-configurable yet.
std::optional<gbemu::Button>
mapKey(SDL_Scancode scancode)
{
  switch (scancode) {
    case SDL_SCANCODE_RIGHT:
      return gbemu::Button::Right;
    case SDL_SCANCODE_LEFT:
      return gbemu::Button::Left;
    case SDL_SCANCODE_UP:
      return gbemu::Button::Up;
    case SDL_SCANCODE_DOWN:
      return gbemu::Button::Down;
    case SDL_SCANCODE_X:
      return gbemu::Button::A;
    case SDL_SCANCODE_Z:
      return gbemu::Button::B;
    case SDL_SCANCODE_RETURN:
      return gbemu::Button::Start;
    case SDL_SCANCODE_BACKSPACE:
      return gbemu::Button::Select;
    default:
      return std::nullopt;
  }
}

std::expected<std::vector<std::uint8_t>, std::string>
readRomFile(std::string_view romPath)
{
  std::ifstream file{ std::filesystem::path(romPath), std::ios::binary };
  if (!file) {
    return std::unexpected(std::string("failed to open ROM file: ") +
                           std::string(romPath));
  }
  return std::vector<std::uint8_t>{ std::istreambuf_iterator<char>(file),
                                    std::istreambuf_iterator<char>() };
}

}

struct App::Impl
{
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  // Bound directly to a playback device (see SDL_OpenAudioDeviceStream in
  // run()) - pushing data via SDL_PutAudioStreamData is all that's needed
  // per frame, SDL pulls from it into the device on its own.
  SDL_AudioStream* audioStream = nullptr;
  bool imguiInitialized = false;
  gbemu::GameBoy gameBoy;
  bool running = true;
  std::optional<std::string> error;
  // Written from SDL's file-dialog callback (see showOpenRomDialog below),
  // which SDL may invoke from a thread other than this one - guarded so
  // frameStep() can safely pick it up once per frame instead of loading the
  // ROM straight from that callback's thread.
  std::mutex pendingRomPathMutex;
  std::optional<std::string> pendingRomPath;
};

#ifndef __EMSCRIPTEN__
namespace {

constexpr SDL_DialogFileFilter ROM_FILE_FILTERS[] = {
  { "Game Boy ROM", "gb;gbc" },
};

}

void SDLCALL
App::onRomFileChosen(void* userdata, const char* const* filelist, int)
{
  if (filelist == nullptr || filelist[0] == nullptr) {
    return; // Error, or the user canceled the dialog - nothing to load.
  }
  auto& impl = *static_cast<Impl*>(userdata);
  const std::lock_guard lock{ impl.pendingRomPathMutex };
  impl.pendingRomPath = filelist[0];
}

void
App::showOpenRomDialog(Impl& impl)
{
  SDL_ShowOpenFileDialog(onRomFileChosen,
                        &impl,
                        impl.window,
                        ROM_FILE_FILTERS,
                        static_cast<int>(std::size(ROM_FILE_FILTERS)),
                        nullptr,
                        false);
}
#endif

void
App::frameStep(void* userData)
{
  auto& impl = *static_cast<Impl*>(userData);

#ifndef __EMSCRIPTEN__
  {
    std::optional<std::string> romToLoad;
    {
      const std::lock_guard lock{ impl.pendingRomPathMutex };
      romToLoad = std::move(impl.pendingRomPath);
      impl.pendingRomPath.reset();
    }
    if (romToLoad) {
      const auto rom = readRomFile(*romToLoad);
      if (!rom) {
        std::cerr << "Warning: " << rom.error() << '\n';
      } else if (const auto loadResult = impl.gameBoy.loadRom(*rom);
                 !loadResult) {
        std::cerr << "Warning: failed to load ROM: " << loadResult.error()
                  << '\n';
      }
    }
  }
#endif

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
#ifndef __EMSCRIPTEN__
    ImGui_ImplSDL3_ProcessEvent(&event);
#endif
    if (event.type == SDL_EVENT_QUIT) {
      impl.running = false;
#ifndef __EMSCRIPTEN__
    } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
               event.key.scancode == SDL_SCANCODE_O &&
               (event.key.mod & SDL_KMOD_CTRL) != 0) {
      showOpenRomDialog(impl);
#endif
    } else if (event.type == SDL_EVENT_KEY_DOWN ||
               event.type == SDL_EVENT_KEY_UP) {
      if (event.key.repeat) {
        continue;
      }
      const auto button = mapKey(event.key.scancode);
      if (button) {
        impl.gameBoy.setButtonState(*button, event.type == SDL_EVENT_KEY_DOWN);
      }
    }
  }

#ifndef __EMSCRIPTEN__
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open ROM...", "Ctrl+O")) {
        showOpenRomDialog(impl);
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  ImGui::Render();
#endif

  const auto frame = impl.gameBoy.runNextFrame();
  if (!frame) {
    impl.error = frame.error();
    impl.running = false;
  } else {
    SDL_UpdateTexture(impl.texture,
                      nullptr,
                      frame->pixels.data_handle(),
                      static_cast<int>(gbemu::SCREEN_WIDTH * 3));
    SDL_RenderClear(impl.renderer);
    SDL_RenderTexture(impl.renderer, impl.texture, nullptr, nullptr);
#ifndef __EMSCRIPTEN__
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), impl.renderer);
#endif
    SDL_RenderPresent(impl.renderer);

    if (impl.audioStream != nullptr) {
      const auto audioByteCount =
        static_cast<int>(frame->audio.size() * sizeof(float));
      SDL_PutAudioStreamData(
        impl.audioStream, frame->audio.data_handle(), audioByteCount);
    }
  }

#ifdef __EMSCRIPTEN__
  if (!impl.running) {
    emscripten_cancel_main_loop();
  }
#endif
}

App::App()
  : m_impl(std::make_unique<Impl>())
{
}

App::~App()
{
#ifndef __EMSCRIPTEN__
  if (m_impl->imguiInitialized) {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
#endif
  if (m_impl->audioStream != nullptr) {
    // Also closes the device it was opened alongside (see
    // SDL_OpenAudioDeviceStream in run()) - no separate SDL_CloseAudioDevice
    // call needed.
    SDL_DestroyAudioStream(m_impl->audioStream);
  }
  if (m_impl->texture != nullptr) {
    SDL_DestroyTexture(m_impl->texture);
  }
  if (m_impl->renderer != nullptr) {
    SDL_DestroyRenderer(m_impl->renderer);
  }
  if (m_impl->window != nullptr) {
    SDL_DestroyWindow(m_impl->window);
  }
  SDL_Quit();
}

std::expected<void, std::string>
App::run(std::string_view romPath)
{
  const auto rom = readRomFile(romPath);
  if (!rom) {
    return std::unexpected(rom.error());
  }

  const auto loadResult = m_impl->gameBoy.loadRom(*rom);
  if (!loadResult) {
    return std::unexpected(loadResult.error());
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    return std::unexpected(std::string("SDL_Init failed: ") + SDL_GetError());
  }

  m_impl->window =
    SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (m_impl->window == nullptr) {
    return std::unexpected(std::string("SDL_CreateWindow failed: ") +
                           SDL_GetError());
  }

  m_impl->renderer = SDL_CreateRenderer(m_impl->window, nullptr);
  if (m_impl->renderer == nullptr) {
    return std::unexpected(std::string("SDL_CreateRenderer failed: ") +
                           SDL_GetError());
  }

  m_impl->texture = SDL_CreateTexture(m_impl->renderer,
                                      SDL_PIXELFORMAT_RGB24,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      static_cast<int>(gbemu::SCREEN_WIDTH),
                                      static_cast<int>(gbemu::SCREEN_HEIGHT));
  if (m_impl->texture == nullptr) {
    return std::unexpected(std::string("SDL_CreateTexture failed: ") +
                           SDL_GetError());
  }
  // Nearest-neighbor, not the default linear filter - the window scales the
  // native 160x144 framebuffer up 3x, and linear filtering blurs the pixel
  // art instead of keeping crisp per-pixel edges.
  SDL_SetTextureScaleMode(m_impl->texture, SDL_SCALEMODE_NEAREST);

#ifndef __EMSCRIPTEN__
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  // No imgui.ini for a single always-present menu bar - there's no window
  // layout worth persisting between runs.
  ImGui::GetIO().IniFilename = nullptr;
  if (!ImGui_ImplSDL3_InitForSDLRenderer(m_impl->window, m_impl->renderer)) {
    ImGui::DestroyContext();
    return std::unexpected(
      std::string("ImGui_ImplSDL3_InitForSDLRenderer failed"));
  }
  if (!ImGui_ImplSDLRenderer3_Init(m_impl->renderer)) {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    return std::unexpected(
      std::string("ImGui_ImplSDLRenderer3_Init failed"));
  }
  m_impl->imguiInitialized = true;
#endif

  // Matches EmulationFrame::audio's own layout exactly (see gbemu.cppm) -
  // interleaved float stereo at gbemu::SAMPLE_RATE - so each frame's
  // samples can be pushed to the stream as-is, no conversion needed.
  const SDL_AudioSpec audioSpec = {
    .format = SDL_AUDIO_F32,
    .channels = 2,
    .freq = static_cast<int>(gbemu::SAMPLE_RATE),
  };
  m_impl->audioStream = SDL_OpenAudioDeviceStream(
    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
  if (m_impl->audioStream == nullptr) {
    // Not fatal: a missing/misconfigured audio device shouldn't prevent
    // the emulator from running at all - fall back to silent operation
    // (frameStep() skips SDL_PutAudioStreamData() when this is null).
    std::cerr << "Warning: SDL_OpenAudioDeviceStream failed, running "
                 "without audio: "
              << SDL_GetError() << '\n';
  } else {
    // Streams bound to a device via SDL_OpenAudioDeviceStream start
    // paused - without this, SDL_PutAudioStreamData()'s queued samples
    // would never actually reach the device.
    SDL_ResumeAudioStreamDevice(m_impl->audioStream);
  }

  // Emscripten: the browser owns the main loop (blocking here would freeze
  // the tab, since it never yields back to the JS event loop) and paces it
  // to TARGET_FPS itself. Native: no such constraint, so a plain blocking
  // loop with an explicit per-frame delay achieves the same target rate.
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(&App::frameStep, m_impl.get(), TARGET_FPS, 1);
#else
  while (m_impl->running) {
    const auto frameStart = SDL_GetTicks();
    frameStep(m_impl.get());
    const auto elapsedMs = static_cast<double>(SDL_GetTicks() - frameStart);
    if (elapsedMs < TARGET_FRAME_MS) {
      SDL_Delay(static_cast<Uint32>(TARGET_FRAME_MS - elapsedMs));
    }
  }
#endif

  if (m_impl->error) {
    return std::unexpected(*m_impl->error);
  }

  return {};
}

}
