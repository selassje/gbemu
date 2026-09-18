import std;
import gbemu;
import frontend;

namespace {

std::optional<gbemu::Mode>
parseMode(std::string_view text)
{
  std::string lower(text);
  std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "auto") {
    return gbemu::Mode::Auto;
  }
  if (lower == "dmg") {
    return gbemu::Mode::Dmg;
  }
  if (lower == "cgb") {
    return gbemu::Mode::Cgb;
  }
  return std::nullopt;
}

}

int
main(int argc, char** argv) // NOLINT(bugprone-exception-escape)
{
  const std::span<char*> args(argv, static_cast<std::size_t>(argc));
  const std::string_view programName =
    args[0]; // NOLINT(*-bounds-avoid-unchecked-container-access)
  if (argc > 3) {
    std::cerr << "usage: " << programName << " [rom-file] [auto|dmg|cgb]\n";
    return 1;
  }

  std::optional<std::string_view> romPath;
  if (argc >= 2) {
    romPath = args[1]; // NOLINT(*-bounds-avoid-unchecked-container-access)
    if (argc == 2 && parseMode(*romPath)) {
      std::cerr << "usage: " << programName << " [rom-file] [auto|dmg|cgb]\n";
      return 1;
    }
  }

  auto mode = gbemu::Mode::Auto;
  if (argc >= 3) {
    // NOLINTNEXTLINE(*-bounds-avoid-unchecked-container-access)
    const auto parsedMode = parseMode(args[2]);
    if (!parsedMode) {
      std::cerr << "usage: " << programName << " [rom-file] [auto|dmg|cgb]\n";
      return 1;
    }
    mode = *parsedMode;
  }

  frontend::App app;
  const auto result = app.run(romPath, mode);
  if (!result) {
    std::cerr << result.error() << '\n';
    return 1;
  }
  return 0;
}
