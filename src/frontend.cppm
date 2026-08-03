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

  [[nodiscard]] std::expected<void, std::string> run(
    std::optional<std::string_view> romPath,
    gbemu::Mode mode);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

  // Free-function shape (via `static`), not a member call: this doubles as
  // the emscripten_set_main_loop_arg callback, which requires a plain
  // void(*)(void*), not a bound member function.
  static void frameStep(void* userData);

  // Also plain-free-function shaped (SDL_ShowOpenFileDialog needs a
  // SDL_DialogFileCallback, not a bound member function) - static rather
  // than free functions in frontend.cpp only because Impl is private, and
  // these need to reach into it.
  static void showOpenRomDialog(Impl& impl);
  static void onRomFileChosen(void* userdata,
                              const char* const* filelist,
                              int filter);

  // Split out of frameStep() purely to keep its cognitive complexity under
  // clang-tidy's threshold - each is only ever called from there.
  static void loadPendingRom(Impl& impl);
  static void pollEvents(Impl& impl);
  static void renderImGuiFrame(Impl& impl);

  // Shared by the Game menu's Reset item and its native-only Ctrl+R
  // shortcut - the ImGui menu itself now renders on both platforms (see
  // frontend.cpp's __EMSCRIPTEN__ guards, narrowed to just the File menu).
  static void resetGame(Impl& impl);
  // Shared by the Game menu's Pause item and its native-only Ctrl+P
  // shortcut.
  static void togglePause(Impl& impl);

  // Emscripten's equivalent of showOpenRomDialog()/onRomFileChosen(): the
  // web page (see web/script.js) has no native file dialog to hook, so it
  // writes an uploaded ROM into Emscripten's virtual filesystem and this
  // polls for a one-shot "load this one" marker file instead - see the
  // definition in frontend.cpp.
  static void checkEmscriptenLoadRequest(Impl& impl);
};

}
