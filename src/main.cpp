#include <print>

#include "utils/cli.hpp"

using namespace rasterizer::utils;

auto main(int argc, char* argv[]) -> int {
  const auto width {parse_cli_argument_u32(argc, argv, "-width").value_or(1920u)};
  const auto height {parse_cli_argument_u32(argc, argv, "-height").value_or(1080u)};
  const auto filepath {parse_cli_argument_str(argc, argv, "-file").value_or("")};

  if (filepath == "") {
    std::println("Use -file <filepath> to provide a filepath to a glTF file");
    return 1;
  }

  std::println("Loading file: {}", filepath);

  return 0;
}
