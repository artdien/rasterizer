#pragma once

#include <glm/mat4x4.hpp>

#include "buffer/framebuffer.hpp"
#include "model/model.hpp"
#include "rasterization/bin.hpp"
#include "rasterization/thread_pool.hpp"
#include "rasterization/triangle.hpp"

namespace rasterizer::rasterization {

// Depending on the context this struct can represent
// a single tile as well as a range of multiple tiles.
struct Tile {
  glm::uvec2 min;
  glm::uvec2 max;
};

class Rasterizer {
public:
  /// @brief Constructs a tiled rasterizer.
  ///
  /// @param width     Width of depth buffer.
  /// @param height    Height of depth buffer.
  /// @param threads   Number of threads to use for parallelized rasterization.
  /// @param tile_size The dimension of each square tile in pixels.
  ///
  /// @note Width and height of depth buffer must match the width and height of output buffer used for rasterizing.
  Rasterizer(u32 width, u32 height, u32 threads, u32 tile_size = 16);

  Rasterizer(const Rasterizer&) = delete;
  Rasterizer(Rasterizer&&) = delete;
  auto operator=(const Rasterizer&) -> Rasterizer& = delete;
  auto operator=(Rasterizer&&) -> Rasterizer& = delete;
  ~Rasterizer() = default;

  /// @brief Executes the full rasterization pipeline to render a model into the output buffer.
  ///
  /// This method executes the following three stages:
  /// 1. Geometry Processing: Transforms vertices, performs clipping and backface culling.
  /// 2. Binning:             Assigns triangles to specific tiles in the image grid to improve cache locality.
  /// 3. Rasterization:       Rasterizes pixels within each tile.
  ///
  /// @param output     Framebuffer where the rasterized image will be stored.
  /// @param model      The 3D model to be rasterized.
  /// @param view       View matrix defining the camera position and orientation.
  /// @param projection Projection matrix defining the perspective or orthographic projection.
  auto rasterize(buffer::FramebufferView<u32> output, const model::Model& model, const glm::mat4& view, const glm::mat4& projection) -> void;

private:
  u32 tile_size_;
  buffer::Framebuffer<f32> depth_;

  BinPool bins_;
  TrianglePool triangles_;
  ThreadPool threads_;

  std::vector<Tile> tiles_;

  auto process_triangles(const model::Model& model, const glm::mat4& view, const glm::mat4& projection) -> void;
  auto bin_triangles() -> void;
  auto rasterize_tiles(buffer::FramebufferView<u32> output) -> void;
};

} // namespace rasterizer::rasterization
