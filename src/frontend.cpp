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

namespace frontend {

namespace {

constexpr int WINDOW_WIDTH = 640;
constexpr int WINDOW_HEIGHT = 576;
constexpr const char* WINDOW_TITLE = "gbemu";

}

struct App::Impl
{
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  bool running = true;
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

  SDL_SetRenderDrawColor(impl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(impl.renderer);
  SDL_RenderPresent(impl.renderer);

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
  if (m_impl->renderer != nullptr) {
    SDL_DestroyRenderer(m_impl->renderer);
  }
  if (m_impl->window != nullptr) {
    SDL_DestroyWindow(m_impl->window);
  }
  SDL_Quit();
}

std::expected<void, std::string>
App::run()
{
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

  // Emscripten: the browser owns the main loop (blocking here would freeze
  // the tab, since it never yields back to the JS event loop). Native: no
  // such constraint, so a plain blocking loop is simplest.
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(&App::frameStep, m_impl.get(), 0, 1);
#else
  while (m_impl->running) {
    frameStep(m_impl.get());
  }
#endif

  return {};
}

}
