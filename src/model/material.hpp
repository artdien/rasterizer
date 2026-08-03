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

struct Texture {
  std::unique_ptr<uc8[], ImageDeleter> image;
  u32 width;
  u32 height;
  u32 channels;

  TextureWrap wrap_s;
  TextureWrap wrap_t;
};

struct Material {
  glm::vec4 base;
  const Texture* albedo;
};

} // namespace rasterizer::model
