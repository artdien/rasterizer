#include <iostream>
#include <print>
#include <string_view>
#include <thread>

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
constexpr auto OUTPUT_INTERVAL_MS {1000.0};

// Leave one core free for the OS to prevent freezes or lags
const auto MAX_THREADS {
    std::thread::hardware_concurrency() > 0u                     //
        ? std::max(1u, std::thread::hardware_concurrency() - 1u) //
        : 1u                                                     //
};

constexpr auto GROUND_COLOR {glm::vec3 {0.2f, 0.2f, 0.2f}};
constexpr auto SKY_COLOR {glm::vec3 {0.2f, 0.5f, 0.8f}};

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
  const auto threads {parse_cli_argument_u32(argc, argv, "-threads").value_or(MAX_THREADS)};
  const auto tile_size {parse_cli_argument_u32(argc, argv, "-tile").value_or(16u)};
  const auto position_x {parse_cli_argument_f32(argc, argv, "-x").value_or(0.0f)};
  const auto position_y {parse_cli_argument_f32(argc, argv, "-y").value_or(0.0f)};
  const auto position_z {parse_cli_argument_f32(argc, argv, "-z").value_or(0.0f)};
  const auto info {parse_cli_flag(argc, argv, "-info")};

  if (filepath == "") {
    std::println("Use -file <path> to provide a filepath to a .gltf or .glb file");
    return 1;
  }

  const auto model {load_model(filepath)};
  auto lighting {load_lighting(filepath)};

  // glTF doesn't support hemispherical lighting,
  // thus we set some pre-defined values here.
  lighting.hemispherical.ground = GROUND_COLOR;
  lighting.hemispherical.sky = SKY_COLOR;

  auto window {Window {width, height, std::string(WINDOW_TITLE)}};
  auto rasterizer {Rasterizer {width, height, threads, tile_size}};
  auto camera {Camera {width, height, {position_x, position_y, position_z}}};

  if (info) {
    std::println("PARAMETERS");
    std::println("  Width: {}", width);
    std::println("  Height: {}", height);
    std::println("  Threads: {}", threads);
    std::println("  Tile Size: {}", tile_size);
    std::println("  Initial Camera Position: ({:.2f}, {:.2f}, {:.2f})", position_x, position_y, position_z);
    std::print("\n");

    print_model_info(model);
    std::print("\n");

    std::println("FRAME DATA");
    std::print("  Calculating ...");
    std::cout.flush();
  }

  auto lag_ms {0.0};
  auto last_output_time_ms {0.0};

  window.open([&](KeyboardInput keyboard, f64 elapsed_time_ms) {
    process_input(&window, &camera, keyboard, static_cast<f32>(elapsed_time_ms / 1000.0f));

    lag_ms = std::min(lag_ms + elapsed_time_ms, MAX_LAG_MS);
    while (lag_ms >= UPDATE_TIME_MS) {
      rasterizer.rasterize(window.buffer(), model, lighting, camera);
      lag_ms -= UPDATE_TIME_MS;
    }

    if (info) {
      last_output_time_ms += elapsed_time_ms;

      if (last_output_time_ms >= OUTPUT_INTERVAL_MS) {
        // Add spacing at end to avoid only partially overwritten output from last frame due to different frame or lag times
        std::print("\r  Frame Time: {:.2f}ms | Lag: {:.2f}ms      ", elapsed_time_ms, lag_ms);
        std::cout.flush();
        last_output_time_ms = 0.0;
      }
    }
  });

  if (info) {
    std::print("\n");
  }

  return 0;
}
