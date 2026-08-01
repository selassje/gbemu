export module frontend;

import std;
import gbemu;

export namespace frontend {

// Hides SDL's window/renderer handles behind an incomplete Impl (defined in
// frontend.cpp, the only TU that needs to see <SDL3/SDL.h>) so this
// interface unit - and everything that imports it - stays free of SDL's C
// API surface.
class App
{
public:
  App();
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;
  App(App&&) = delete;
  App& operator=(App&&) = delete;

  [[nodiscard]] std::expected<void, std::string> run(std::string_view romPath,
                                                     gbemu::Mode mode);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

  // Free-function shape (via `static`), not a member call: this doubles as
  // the emscripten_set_main_loop_arg callback, which requires a plain
  // void(*)(void*), not a bound member function.
  static void frameStep(void* userData);
};

}
