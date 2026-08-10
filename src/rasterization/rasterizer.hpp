#pragma once

#include <glm/mat4x4.hpp>

#include "buffer/framebuffer.hpp"
#include "model/lighting.hpp"
#include "model/model.hpp"
#include "platform/types.hpp"
#include "rasterization/bin.hpp"
#include "rasterization/camera.hpp"
#include "rasterization/shading.hpp"
#include "rasterization/thread_pool.hpp"
#include "rasterization/triangle.hpp"

namespace rasterizer::rasterization {

// Depending on the context this struct can represent
// a single tile as well as a range of multiple tiles.
struct Tile {
  glm::uvec2 min;
  glm::uvec2 max;
};

template <shading::Shader ShaderType>
class Rasterizer {
public:
  /// @brief Constructs a tiled rasterizer.
  ///
  /// @param shader              Shader instance to use for fragment shading.
  /// @param width               Width of depth buffer.
  /// @param height              Height of depth buffer.
  /// @param threads             Number of threads to use for parallelized rasterization.
  /// @param tile_size           The dimension of each square tile in pixels.
  /// @param gradient_background Whether to render a gradient background before rasterizing triangles.
  ///
  /// @note Width and height of depth buffer must match the width and height of output buffer used for rasterizing.
  Rasterizer(ShaderType shader, u32 width, u32 height, u32 threads, u32 tile_size, bool gradient_background);

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
  /// @param output   Framebuffer where the rasterized image will be stored.
  /// @param model    The 3D model to be rasterized.
  /// @param lighting Lighting data for shading.
  /// @param camera   Camera object providing position, view matrix, and projection matrix.
  auto rasterize(buffer::FramebufferView<u32> output, const model::Model& model, const model::Lighting& lighting, const Camera& camera) -> void;

  /// @brief Increments the Y-axis rotation of the model.
  ///
  /// @param dt Delta time in seconds.
  auto rotate_model(f32 dt) -> void;

private:
  ShaderType shader_;

  u32 tile_size_;
  std::vector<Tile> tiles_;

  BinPool bins_;
  TrianglePool triangles_;
  ThreadPool threads_;

  f32 current_rotation_;
  buffer::Framebuffer<f32> depth_;
  bool gradient_background_;

  auto process_triangles(const model::Model& model, const glm::mat4& view, const glm::mat4& projection) -> void;
  auto bin_triangles() -> void;
  auto rasterize_tiles(buffer::FramebufferView<u32> output, const model::Lighting& lighting, glm::vec3 camera_position) -> void;
  auto rasterize_triangle(buffer::FramebufferView<u32> output, buffer::FramebufferView<f32> depth, const Triangle& triangle, Tile tile,
                          const model::Lighting& lighting, glm::vec3 camera_position) -> void;
};

} // namespace rasterizer::rasterization
