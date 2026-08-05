#include "buffer/framebuffer.hpp"

namespace rasterizer::buffer {

template <typename T>
Framebuffer<T>::Framebuffer(u32 width, u32 height) : width_ {width}, height_ {height}, buffer_ {std::make_unique<T[]>(width * height)} {
  std::fill_n(buffer_.get(), width_ * height_, T {});
}

template <typename T>
auto Framebuffer<T>::view() -> FramebufferView<T> {
  return FramebufferView(width_, height_, buffer_.get());
}

template class Framebuffer<u32>;
template class Framebuffer<f32>;
template struct FramebufferView<u32>;
template struct FramebufferView<f32>;

} // namespace rasterizer::buffer
