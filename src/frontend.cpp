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
#endif

module frontend;

import gbemu;

namespace frontend {

namespace {

constexpr int WINDOW_WIDTH = static_cast<int>(gbemu::SCREEN_WIDTH);
constexpr int WINDOW_HEIGHT = static_cast<int>(gbemu::SCREEN_HEIGHT);
constexpr const char* WINDOW_TITLE = "gbemu";
constexpr int TARGET_FPS = 60;
constexpr double TARGET_FRAME_MS = 1000.0 / TARGET_FPS;

}

struct App::Impl
{
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  gbemu::GameBoy gameBoy;
  bool running = true;
  std::optional<std::string> error;
};

void
App::frameStep(void* userData)
{
  auto& impl = *static_cast<Impl*>(userData);

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      impl.running = false;
    }
  }

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
    SDL_RenderPresent(impl.renderer);
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
  std::ifstream file{ std::filesystem::path(romPath), std::ios::binary };
  if (!file) {
    return std::unexpected(std::string("failed to open ROM file: ") +
                           std::string(romPath));
  }
  const std::vector<std::uint8_t> rom{ std::istreambuf_iterator<char>(file),
                                       std::istreambuf_iterator<char>() };

  const auto loadResult = m_impl->gameBoy.loadRom(rom);
  if (!loadResult) {
    return std::unexpected(loadResult.error());
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
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
