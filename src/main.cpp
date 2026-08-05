#include <print>
#include <string_view>

#include "model/loading.hpp"
#include "platform/types.hpp"
#include "platform/window.hpp"
#include "rasterization/camera.hpp"
#include "rasterization/rasterizer.hpp"
#include "utils/cli.hpp"

using namespace rasterizer::utils;
using namespace rasterizer::model;
using namespace rasterizer::platform;
using namespace rasterizer::rasterization;

namespace {

constexpr auto WINDOW_TITLE {std::string_view {"Software Rasterizer"}};
constexpr auto UPDATE_TIME_MS {1000.0 / 30.0};
constexpr auto MAX_LAG_MS {100.0};

auto process_input(Window* window, Camera* camera, const KeyboardInput& keyboard, f32 dt) -> void {
  if (keyboard.key == "esc") {
    window->close();
  } else if (keyboard.key == "w") {
    camera->move_forward(dt);
  } else if (keyboard.key == "s") {
    camera->move_backward(dt);
  } else if (keyboard.key == "a") {
    camera->move_left(dt);
  } else if (keyboard.key == "d") {
    camera->move_right(dt);
  } else if (keyboard.key == "e") {
    camera->move_up(dt);
  } else if (keyboard.key == "q") {
    camera->move_down(dt);
  } else if (keyboard.key == "left") {
    camera->look_left(dt);
  } else if (keyboard.key == "right") {
    camera->look_right(dt);
  } else if (keyboard.key == "up") {
    camera->look_up(dt);
  } else if (keyboard.key == "down") {
    camera->look_down(dt);
  }
}

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
  auto camera {Camera {width, height}};

  auto lag {0.0};

  window.open([&](KeyboardInput keyboard, f64 elapsed_time_ms) {
    process_input(&window, &camera, keyboard, static_cast<f32>(elapsed_time_ms / 1000.0f));

    lag = std::min(lag + elapsed_time_ms, MAX_LAG_MS);
    while (lag >= UPDATE_TIME_MS) {
      rasterizer.rasterize(window.buffer(), model, camera.view_matrix(), camera.projection_matrix());
      lag -= UPDATE_TIME_MS;
    }
  });

  return 0;
}
