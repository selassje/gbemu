export module frontend;

import std;
import gbemu;

export namespace frontend {

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

  static void frameStep(void* userData);

  static void showOpenRomDialog(Impl& impl);
  static void onRomFileChosen(void* userdata,
                              const char* const* filelist,
                              int filter);

  static void loadPendingRom(Impl& impl);
  static void pollEvents(Impl& impl);
  static bool handleCtrlShortcut(Impl& impl, int scancode);
  static void renderImGuiFrame(Impl& impl);
  static void renderMenuBar(Impl& impl);
  static void renderModeMenu(Impl& impl);
  static void renderSpeedMenu(Impl& impl);
  static void renderErrorBar(Impl& impl);
  static void renderAudioMenu(Impl& impl);
  static void renderVideoMenu(Impl& impl);

  static void resetGame(Impl& impl);
  static void togglePause(Impl& impl);
  static void setMode(Impl& impl, gbemu::Mode mode);
  static void toggleAudioEnabled(Impl& impl);
  static void syncAudioDeviceState(Impl& impl);
  static void setSpeedFps(Impl& impl, double fps);
  static void increaseSpeed(Impl& impl);
  static void decreaseSpeed(Impl& impl);
  static void applyWindowSize(Impl& impl);
  static void setVideoScale(Impl& impl, int scale);
  static void toggleFullscreen(Impl& impl);
  static void handleVideoShortcut(Impl& impl, int scancode);

  static void writeStateToFile(Impl& impl, const std::filesystem::path& path);
  static void readStateFromFile(Impl& impl, const std::filesystem::path& path);

  static void saveGameState(Impl& impl);
  static void loadGameState(Impl& impl);
  static void onSaveStateFileChosen(void* userdata,
                                    const char* const* filelist,
                                    int filter);
  static void onLoadStateFileChosen(void* userdata,
                                    const char* const* filelist,
                                    int filter);
  static void applyPendingStateRequests(Impl& impl);

  static void checkEmscriptenLoadRequest(Impl& impl);
  static void checkEmscriptenSaveStateRequest(Impl& impl);
  static void checkEmscriptenLoadStateRequest(Impl& impl);
};

}
