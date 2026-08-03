#include <print>

#include "model/loading.hpp"
#include "utils/cli.hpp"

using namespace rasterizer::utils;
using namespace rasterizer::model;

auto main(i32 argc, c8* argv[]) -> int {
  const auto width {parse_cli_argument_u32(argc, argv, "-width").value_or(1920u)};
  const auto height {parse_cli_argument_u32(argc, argv, "-height").value_or(1080u)};
  const auto filepath {parse_cli_argument_str(argc, argv, "-file").value_or("")};

  if (filepath == "") {
    std::println("Use -file <path> to provide a filepath to a .gltf or .glb file");
    return 1;
  }

  load_model(filepath);

  return 0;
}
