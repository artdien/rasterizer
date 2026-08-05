#include <print>
#include <string_view>

#include "model/loading.hpp"
#include "platform/types.hpp"
#include "platform/window.hpp"
#include "rasterization/rasterizer.hpp"
#include "utils/cli.hpp"

#include <glm/gtc/matrix_transform.hpp>

using namespace rasterizer::utils;
using namespace rasterizer::model;
using namespace rasterizer::platform;
using namespace rasterizer::rasterization;

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

  const auto model {load_model(filepath)};

  auto window {Window {width, height, std::string(WINDOW_TITLE)}};
  auto rasterizer {Rasterizer {width, height}};

  glm::mat4 view {glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f))};
  glm::mat4 projection {glm::perspective(glm::radians(45.0f), static_cast<f32>(width) / static_cast<f32>(height), 0.1f, 100.0f)};

  auto lag {0.0};

  window.open([&](f64 elapsed_time) {
    lag = std::min(lag + elapsed_time, MAX_LAG_MS);

    while (lag >= UPDATE_TIME_MS) {
      rasterizer.rasterize(window.buffer(), model, view, projection);
      lag -= UPDATE_TIME_MS;
    }
  });

  return 0;
}
