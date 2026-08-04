#include "buffer/framebuffer.hpp"

namespace rasterizer::buffer {

Framebuffer::Framebuffer(u32 width, u32 height) : width_ {width}, height_ {height}, buffer_ {std::make_unique<u32[]>(width * height)} {
  clear();
}

auto Framebuffer::view() -> FramebufferView {
  return FramebufferView(width_, height_, buffer_.get());
}

auto Framebuffer::clear(u32 value) -> void {
  std::fill_n(buffer_.get(), width_ * height_, value);
}

} // namespace rasterizer::buffer
