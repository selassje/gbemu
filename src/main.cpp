import std;
import frontend;

int
main(int argc, char** argv) // NOLINT(bugprone-exception-escape)
{
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <rom-file>\n";
    return 1;
  }

  frontend::App app;
  const auto result = app.run(argv[1]);
  if (!result) {
    std::cerr << result.error() << '\n';
    return 1;
  }
  return 0;
}
