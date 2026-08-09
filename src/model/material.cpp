#include "model/material.hpp"

#include <cmath>

namespace rasterizer::model {

namespace {

auto wrap_value(f32 value, TextureWrap mode) -> f32 {
  switch (mode) {
    case TextureWrap::REPEAT: {
      value = std::fmod(value, 1.0f);
      return value < 0.0f ? value + 1.0f : value;
    }
    case TextureWrap::MIRRORED_REPEAT: {
      const auto floored {std::floorf(value)};
      const auto frac {value - floored};
      return static_cast<i32>(floored) % 2 != 0 ? 1.0f - frac : frac;
    }
    case TextureWrap::CLAMP_TO_EDGE:
      return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
  }
}

} // namespace

Texture::Texture() : data_ {nullptr}, width_ {0}, height_ {0}, channels_ {0} {}

Texture::Texture(uc8* data, ImageDeleter deleter, u32 width, u32 height, u32 channels)
    : wrap_s {TextureWrap::REPEAT}, wrap_t {TextureWrap::REPEAT},                                                         //
      data_ {std::make_unique<f32[]>(width * height * channels)}, width_ {width}, height_ {height}, channels_ {channels}, //
      scale_u_ {static_cast<f32>(width - 1u)}, scale_v_ {static_cast<f32>(height - 1u)} {
  const f32 inv {1.0f / 255.0f};

  for (auto i {0u}; i < width_ * height_; ++i) {
    const auto idx {i * channels_};
    data_[idx] = static_cast<f32>(data[idx]) * inv;
    data_[idx + 1] = static_cast<f32>(data[idx + 1]) * inv;
    data_[idx + 2] = static_cast<f32>(data[idx + 2]) * inv;
    data_[idx + 3] = static_cast<f32>(data[idx + 3]) * inv;
  }

  deleter(data);
}

auto Texture::sample(f32 u, f32 v) const -> glm::vec4 {
  if (!data_) [[unlikely]] {
    return {0.0f, 0.0f, 0.0f, 0.0f};
  }

  if (u < 0.0f || u > 1.0f) {
    u = wrap_value(u, wrap_s);
  }
  if (v < 0.0f || v > 1.0f) {
    v = wrap_value(v, wrap_t);
  }

  const auto x {static_cast<u32>(u * scale_u_)};
  const auto y {static_cast<u32>(v * scale_v_)};

  const auto idx {channels_ * (x + y * width_)};
  return {data_[idx], data_[idx + 1], data_[idx + 2], data_[idx + 3]};
}

} // namespace rasterizer::model
