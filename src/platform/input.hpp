#pragma once

#include <optional>
#include <string>

#include <glm/vec2.hpp>

namespace rasterizer::platform {

struct KeyboardInput {
  std::string key {""};
};

/// Stores a keyboard input event in a queue for later retrieval.
///
/// @param mouse_input Event to be stored.
auto add_keyboard_input_event(KeyboardInput keyboard_input) -> void;

/// Retrieves a stored keyboard input event.
///
/// Since events are stored in a queue, they are retrieved in FIFO order.
///
/// @return Optional containing keyboard input event if queue is non-empty, otherwise std::nullopt.
auto get_keyboard_input_event() -> std::optional<KeyboardInput>;

} // namespace rasterizer::platform
