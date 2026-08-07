#include "model/material.hpp"

#include <algorithm>
#include <cmath>

namespace rasterizer::model {

namespace {

auto wrap_value(f32 value, TextureWrap mode) -> f32 {
  switch (mode) {
    case TextureWrap::REPEAT: {
      value = fmodf(value, 1.0f);
      if (value < 0.0f)
        value += 1.0f;
      return value;
    }

    case TextureWrap::MIRRORED_REPEAT: {
      if (const auto floor_value {std::floor(value)}; static_cast<i32>(floor_value) % 2 != 0) {
        return 1.0f - (value - floor_value);
      } else {
        return value - floor_value;
      }
    }

    case TextureWrap::CLAMP_TO_EDGE: {
      return std::clamp(value, 0.0f, 1.0f);
    }
  }
}

} // namespace

Texture::Texture() : data_ {nullptr, [](auto _) {}}, width_ {0}, height_ {0}, channels_ {0} {}

Texture::Texture(uc8* data, ImageDeleter deleter, u32 width, u32 height, u32 channels)
    : data_ {std::unique_ptr<uc8[], ImageDeleter>(data, deleter)}, width_ {width}, height_ {height}, channels_ {channels} {}

auto Texture::sample(f32 u, f32 v) const -> glm::vec4 {
  if (!data_) [[unlikely]] {
    return {0.0f, 0.0f, 0.0f, 0.0f};
  }

  u = wrap_value(u, wrap_s);
  v = wrap_value(v, wrap_t);

  const auto x {std::min(width_ - 1, static_cast<u32>(u * static_cast<f32>(width_)))};
  const auto y {std::min(height_ - 1, static_cast<u32>(v * static_cast<f32>(height_)))};

  const auto idx {channels_ * (x + y * width_)};
  const auto r {static_cast<f32>(data_[idx] / 255.0f)};
  const auto g {static_cast<f32>(data_[idx + 1] / 255.0f)};
  const auto b {static_cast<f32>(data_[idx + 2] / 255.0f)};
  const auto a {static_cast<f32>(data_[idx + 3] / 255.0f)};

  return {r, g, b, a};
}

} // namespace rasterizer::model
