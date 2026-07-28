import std;
import frontend;

int
main() // NOLINT(bugprone-exception-escape)
{
  frontend::App app;
  const auto result = app.run();
  if (!result) {
    std::cerr << result.error() << '\n';
    return 1;
  }
  return 0;
}
