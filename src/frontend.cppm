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
  // Split out of renderImGuiFrame() purely to keep its cognitive complexity
  // under clang-tidy's threshold - only ever called from there, as the
  // Game menu's Mode submenu.
  static void renderModeMenu(Impl& impl);
  // Also split out of renderImGuiFrame(), same reason - draws Impl::error
  // (when set and still within its display window) as red text in a black
  // bar along the bottom of the window. Distinct from Impl::error itself:
  // that keeps gating frameStep()'s runNextFrame() calls until something
  // recovers it (see its own comment), while this only controls how long
  // the bar stays visible.
  static void renderErrorBar(Impl& impl);

  // Shared by the Game menu's Reset item and its native-only Ctrl+R
  // shortcut - the ImGui menu itself now renders on both platforms (see
  // frontend.cpp's __EMSCRIPTEN__ guards, narrowed to just the File menu).
  static void resetGame(Impl& impl);
  // Shared by the Game menu's Pause item and its native-only Ctrl+P
  // shortcut.
  static void togglePause(Impl& impl);
  // Backs the Game menu's Mode submenu (Auto/DMG/CGB) - no keyboard
  // shortcut, there being three mutually-exclusive options rather than one
  // to toggle.
  static void setMode(Impl& impl, gbemu::Mode mode);
  // Shared by the Audio menu's Enabled item and its Ctrl+A shortcut.
  static void toggleAudioEnabled(Impl& impl);
  // Resumes/pauses the audio device to match Impl::audioEnabled and
  // Impl::paused - shared by togglePause() and toggleAudioEnabled() since
  // either one flipping can change whether the device should be playing.
  static void syncAudioDeviceState(Impl& impl);

  // The actual GameBoy::saveState()/loadState() <-> file byte I/O, shared by
  // both platforms' own save-state entry points below - they differ only in
  // how each arrives at the file path to use.
  static void writeStateToFile(Impl& impl, const std::filesystem::path& path);
  static void readStateFromFile(Impl& impl, const std::filesystem::path& path);

  // Native: the File menu's Save State/Load State items and their
  // Ctrl+S/Ctrl+L shortcuts - each opens a real save/open file dialog
  // (seeded with a default <romPath>.state location derived from
  // Impl::currentRomPath) rather than writing to a single fixed path
  // outright - see their own __EMSCRIPTEN__ guard in frontend.cpp.
  static void saveGameState(Impl& impl);
  static void loadGameState(Impl& impl);
  // Those dialogs' own SDL_DialogFileCallback - same cross-thread hand-off
  // shape as onRomFileChosen() above, into Impl::pendingSaveStatePath/
  // pendingLoadStatePath.
  static void onSaveStateFileChosen(void* userdata,
                                    const char* const* filelist,
                                    int filter);
  static void onLoadStateFileChosen(void* userdata,
                                    const char* const* filelist,
                                    int filter);
  // frameStep()'s per-frame pickup of whatever the callbacks above hand
  // off - same reason loadPendingRom() exists instead of doing the file
  // I/O straight from the callback's (possibly non-main) thread.
  static void applyPendingStateRequests(Impl& impl);

  // Emscripten's equivalent of showOpenRomDialog()/onRomFileChosen(): the
  // web page (see web/script.js) has no native file dialog to hook, so it
  // writes an uploaded ROM into Emscripten's virtual filesystem and this
  // polls for a one-shot "load this one" marker file instead - see the
  // definition in frontend.cpp.
  static void checkEmscriptenLoadRequest(Impl& impl);
  // web/script.js's own save-state sidebar list writes a user-chosen
  // filename into one of these one-shot request markers under
  // /gbemu_saves, mirroring checkEmscriptenLoadRequest() above - IDBFS
  // persistence itself is handled entirely on the JS side (see
  // script.js's own comment), this only needs to notice the request and
  // perform the save/load through writeStateToFile()/readStateFromFile()
  // above.
  static void checkEmscriptenSaveStateRequest(Impl& impl);
  static void checkEmscriptenLoadStateRequest(Impl& impl);
};

}
