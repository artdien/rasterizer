#pragma once

#include <functional>
#include <string>

#include "buffer/framebuffer.hpp"
#include "platform/types.hpp"

namespace rasterizer::platform {

class Window {
public:
  /// @brief Constructs window.
  ///
  /// Constructing a window does not open it automatically.
  /// The open method must be called for that.
  ///
  /// @param width  Width of the window.
  /// @param height Height of the window.
  /// @param title  Title of the window.
  Window(u32 width, u32 height, const std::string& title = "");

  Window(const Window&) = delete;
  Window(Window&&) = delete;
  auto operator=(const Window&) -> Window& = delete;
  auto operator=(Window&&) -> Window& = delete;
  ~Window() = default;

  /// @brief Opens a window and runs it indefinitely until it is closed.
  ///
  /// @param execute_per_frame A function which will be executed once per frame.
  ///                          Typically this function should contain update and rendering logic.
  ///                          The arguments for this function are:
  ///                          - Elapsed time since last frame.
  auto open(std::function<void(f64)> execute_per_frame) -> void;

  /// @brief Closes an opened window.
  auto close() -> void;

  /// @brief Returns internal framebuffer of window.
  ///
  /// Modifications made to internal framebuffer will immediately be visible.
  ///
  /// @return Modifiable view of internal framebuffer.
  auto buffer() -> buffer::FramebufferView<u32>;

private:
  std::string title_;
  buffer::Framebuffer<u32> buffer_;
};

} // namespace rasterizer::platform
