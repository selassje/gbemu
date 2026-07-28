import std;
import frontend;

int
main(int argc, char** argv) // NOLINT(bugprone-exception-escape)
{
  const std::span<char*> args(argv, static_cast<std::size_t>(argc));
  if (argc < 2) {
    // argc is always >= 1 per the standard, so args[0] is always valid.
    std::cerr << "usage: "
              << args[0] // NOLINT(*-bounds-avoid-unchecked-container-access)
              << " <rom-file>\n";
    return 1;
  }

  frontend::App app;
  // argc >= 2 is checked above, so args[1] is always valid.
  const auto result =
    app.run(args[1]); // NOLINT(*-bounds-avoid-unchecked-container-access)
  if (!result) {
    std::cerr << result.error() << '\n';
    return 1;
  }
  return 0;
}
