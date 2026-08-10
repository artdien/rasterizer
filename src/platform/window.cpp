#include "platform/window.hpp"

#include <fenster.h>

#include <chrono>

namespace rasterizer::platform {

namespace {

auto window_opened {true};

auto register_key_press(const fenster& window) {
  if (window.keys[27]) {
    add_keyboard_input_event(KeyboardInput {.key = "esc"});
  } else if (window.keys['W']) {
    add_keyboard_input_event(KeyboardInput {.key = "w"});
  } else if (window.keys['S']) {
    add_keyboard_input_event(KeyboardInput {.key = "s"});
  } else if (window.keys['A']) {
    add_keyboard_input_event(KeyboardInput {.key = "a"});
  } else if (window.keys['D']) {
    add_keyboard_input_event(KeyboardInput {.key = "d"});
  } else if (window.keys['E']) {
    add_keyboard_input_event(KeyboardInput {.key = "e"});
  } else if (window.keys['Q']) {
    add_keyboard_input_event(KeyboardInput {.key = "q"});
  } else if (window.keys[17]) {
    add_keyboard_input_event(KeyboardInput {.key = "up"});
  } else if (window.keys[18]) {
    add_keyboard_input_event(KeyboardInput {.key = "down"});
  } else if (window.keys[19]) {
    add_keyboard_input_event(KeyboardInput {.key = "right"});
  } else if (window.keys[20]) {
    add_keyboard_input_event(KeyboardInput {.key = "left"});
  } else if (window.keys[32]) {
    add_keyboard_input_event(KeyboardInput {.key = "space"});
  }
}

}; // namespace

Window::Window(u32 width, u32 height, const std::string& title) : title_ {title}, buffer_ {width, height} {}

auto Window::open(std::function<void(KeyboardInput, f64)> execute_per_frame) -> void {
  auto view {buffer_.view()};
  auto window {fenster {
      .title = title_.c_str(),
      .width = static_cast<i32>(view.width()),
      .height = static_cast<i32>(view.height()),
      .buf = &view.at(0, view.height() - 1),
  }};

  fenster_open(&window);
  window_opened = true;

  auto previous_time {std::chrono::steady_clock::now()};

  while (window_opened && fenster_loop(&window) == 0) {
    const auto current_time {std::chrono::steady_clock::now()};
    const auto elapsed_time {std::chrono::round<std::chrono::microseconds>(current_time - previous_time).count() / 1000.0};
    previous_time = current_time;

    register_key_press(window);
    auto keyboard_input {get_keyboard_input_event()};

    execute_per_frame(keyboard_input.value_or({}), elapsed_time);
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
