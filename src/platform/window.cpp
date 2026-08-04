#include "platform/window.hpp"

#include <fenster.h>

#include <chrono>

namespace rasterizer::platform {

namespace {

auto window_opened {true};

}; // namespace

Window::Window(u32 width, u32 height, const std::string& title) : title_ {title}, buffer_ {width, height} {}

auto Window::open(std::function<void(f64)> execute_per_frame) -> void {
  auto view {buffer_.view()};
  auto window {fenster {
      .title = title_.c_str(),
      .width = static_cast<i32>(view.width),
      .height = static_cast<i32>(view.height),
      .buf = &view.at(0, view.height - 1),
  }};

  fenster_open(&window);
  window_opened = true;

  auto previous_time {std::chrono::steady_clock::now()};

  while (window_opened && fenster_loop(&window) == 0) {
    const auto current_time {std::chrono::steady_clock::now()};
    const auto elapsed_time {std::chrono::round<std::chrono::microseconds>(current_time - previous_time).count() / 1000.0};
    previous_time = current_time;

    execute_per_frame(elapsed_time);
  }

  fenster_close(&window);
}

auto Window::close() -> void {
  window_opened = false;
}

auto Window::buffer() -> buffer::FramebufferView<u32> {
  return buffer_.view();
}

} // namespace rasterizer::platform
