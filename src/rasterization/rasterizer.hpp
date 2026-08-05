#pragma once

#include <glm/mat4x4.hpp>

#include "buffer/framebuffer.hpp"
#include "model/model.hpp"

namespace rasterizer::rasterization {

class Rasterizer {
public:
  /// @brief Constructs a Rasterizer.
  ///
  /// @param width  Width of depth buffer.
  /// @param height Height of depth buffer.
  ///
  /// @note Width and height of depth buffer must match the width and height of the output buffer used for rasterizing.
  Rasterizer(u32 width, u32 height);

  Rasterizer(const Rasterizer&) = delete;
  Rasterizer(Rasterizer&&) = delete;
  auto operator=(const Rasterizer&) -> Rasterizer& = delete;
  auto operator=(Rasterizer&&) -> Rasterizer& = delete;
  ~Rasterizer() = default;

  /// @brief Rasterizes a model into the provided output buffer.
  ///
  /// This method projects a 3D model onto a 2D plane using the view and projection matrices
  /// and fills the output buffer with the rasterized pixels.
  ///
  /// @param output     Framebuffer where the rasterized image will be stored.
  /// @param model      The 3D model to be rasterized.
  /// @param view       View matrix defining the camera position and orientation.
  /// @param projection Projection matrix defining the perspective or orthographic projection.
  auto rasterize(buffer::FramebufferView<u32> output, const model::Model& model, const glm::mat4& view, const glm::mat4& projection) -> void;

private:
  buffer::Framebuffer<f32> depth_;
};

} // namespace rasterizer::rasterization
