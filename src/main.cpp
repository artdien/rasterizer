#include <print>
#include <string_view>

#include "model/loading.hpp"
#include "platform/types.hpp"
#include "platform/window.hpp"
#include "utils/cli.hpp"

using namespace rasterizer::utils;
using namespace rasterizer::model;
using namespace rasterizer::platform;

namespace {

constexpr auto WINDOW_TITLE {std::string_view {"Software Rasterizer"}};
constexpr auto UPDATE_TIME_MS {1000.0 / 30.0};
constexpr auto MAX_LAG_MS {100.0};

} // namespace

auto main(i32 argc, c8* argv[]) -> int {
  const auto width {parse_cli_argument_u32(argc, argv, "-width").value_or(1920u)};
  const auto height {parse_cli_argument_u32(argc, argv, "-height").value_or(1080u)};
  const auto filepath {parse_cli_argument_str(argc, argv, "-file").value_or("")};

  if (filepath == "") {
    std::println("Use -file <path> to provide a filepath to a .gltf or .glb file");
    return 1;
  }

  load_model(filepath);

  Window window {width, height, std::string(WINDOW_TITLE)};

  auto lag {0.0};

  window.open([&](f64 elapsed_time) {
    lag = std::min(lag + elapsed_time, MAX_LAG_MS);

    while (lag >= UPDATE_TIME_MS) {
      for (auto x {0u}; x < width; ++x) {
        for (auto y {0u}; y < height; ++y) {
          window.buffer().at(x, y) = 0x00FFFFFF;
        }
      }

      lag -= UPDATE_TIME_MS;
    }
  });

  return 0;
}
