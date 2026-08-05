#include "platform/input.hpp"

#include <queue>

namespace rasterizer::platform {

namespace {

std::queue<KeyboardInput> keyboard_input_queue {};

} // namespace

auto add_keyboard_input_event(KeyboardInput keyboard_input) -> void {
  keyboard_input_queue.push(keyboard_input);
}

auto get_keyboard_input_event() -> std::optional<KeyboardInput> {
  auto keyboard_input {std::optional<KeyboardInput> {}};
  if (!keyboard_input_queue.empty()) {
    keyboard_input.emplace(keyboard_input_queue.front());
    keyboard_input_queue.pop();
  }
  return keyboard_input;
}

} // namespace rasterizer::platform
