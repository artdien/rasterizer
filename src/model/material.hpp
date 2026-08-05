#pragma once

#include <functional>
#include <memory>

#include <glm/vec4.hpp>

#include "platform/types.hpp"

namespace rasterizer::model {

using ImageDeleter = std::function<void(uc8*)>;

enum class TextureWrap {
  REPEAT,
  MIRRORED_REPEAT,
  CLAMP_TO_EDGE,
};

class Texture {
public:
  /// @brief Constructs an empty texture.
  ///
  /// The resulting texture has no dimensions and contains no data.
  /// Sampling from an empty texture will return 0.
  Texture();

  /// @brief Constructs a texture from existing image data.
  ///
  /// This constructor takes ownership of the provided data.
  /// When the Texture object is destroyed,
  /// the specified deleter function is called to free the memory.
  ///
  /// @param data     Pointer to the raw image data.
  /// @param deleter  Function used to deallocate the image data.
  /// @param width    Width of the image in pixels.
  /// @param height   Height of the image in pixels.
  /// @param channels Number of color channels per pixel.
  Texture(uc8* data, ImageDeleter deleter, u32 width, u32 height, u32 channels);

  Texture(const Texture&) = delete;
  Texture(Texture&&) = default;
  auto operator=(const Texture&) -> Texture& = delete;
  auto operator=(Texture&&) -> Texture& = default;
  ~Texture() = default;

  /// @brief Samples a value from the texture using normalized coordinates.
  ///
  /// This method maps the normalized coordinates (u, v) to
  /// actual pixel coordinates based on the texture's dimensions.
  /// It applies the configured wrap modes to handle
  /// normalized coordinates outside the [0, 1] range before sampling.
  ///
  /// @param u Horizontal normalized coordinate.
  /// @param v Vertical normalized coordinate.
  /// @return Sampled value packed as u32 in RGB format
  ///         with the most significant eight bits being zero.
  auto sample(f32 u, f32 v) -> u32;

  TextureWrap wrap_s;
  TextureWrap wrap_t;

private:
  std::unique_ptr<uc8[], ImageDeleter> data_;
  u32 width_;
  u32 height_;
  u32 channels_;
};

struct Material {
  glm::vec4 base;
  const Texture* albedo;
};

} // namespace rasterizer::model
