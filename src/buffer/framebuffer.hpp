#pragma once

#include <memory>

#include "platform/types.hpp"

namespace rasterizer::buffer {

template <typename T>
struct FramebufferView {
  /// @brief Constructs a view into an existing framebuffer.
  ///
  /// The view does not own the underlying memory.
  /// It simply provides a coordinate-based interface to access it.
  ///
  /// @param width  Width of the buffer.
  /// @param height Height of the buffer.
  /// @param buffer Pointer to the raw buffer data.
  FramebufferView(u32 width, u32 height, T* buffer) : width {width}, height {height}, buffer_ {buffer} {};

  /// @brief Returns a reference to the value at coordinates (x, y).
  ///
  /// The coordinate system is bottom-up:
  /// (0, 0) corresponds to the bottom-left corner of the buffer.
  ///
  /// @param x X-coordinate (column).
  /// @param y Y-coordinate (row).
  auto at(u32 x, u32 y) -> u32& {
    return buffer_[x + (height - (y + 1)) * width];
  }

  /// @brief Returns a constant reference to the value at coordinates (x, y).
  ///
  /// The coordinate system is bottom-up:
  /// (0, 0) corresponds to the bottom-left corner of the buffer.
  ///
  /// @param x X-coordinate (column).
  /// @param y Y-coordinate (row).
  auto at(u32 x, u32 y) const -> const u32& {
    return buffer_[x + (height - (y + 1)) * width];
  }

  u32 width;
  u32 height;

private:
  T* buffer_;
};

template <typename T>
class Framebuffer {
public:
  /// @brief Constructs a framebuffer with the specified dimensions.
  ///
  /// This allocates the necessary memory for the buffer.
  ///
  /// @param width  Width of the framebuffer.
  /// @param height Height of the framebuffer.
  Framebuffer(u32 width, u32 height);

  Framebuffer(const Framebuffer&) = delete;
  Framebuffer(Framebuffer&&) = delete;
  auto operator=(const Framebuffer&) -> Framebuffer& = delete;
  auto operator=(Framebuffer&&) -> Framebuffer& = delete;
  ~Framebuffer() = default;

  /// @brief Creates and returns a view of the framebuffer.
  ///
  /// @return Modifiable view of framebuffer.
  auto view() -> FramebufferView<T>;

  /// @brief Fills the entire framebuffer with a specific value.
  ///
  /// @param value The value to fill the buffer with.
  auto clear(T value = T {}) -> void;

private:
  u32 width_;
  u32 height_;
  std::unique_ptr<T[]> buffer_;
};

} // namespace rasterizer::buffer
