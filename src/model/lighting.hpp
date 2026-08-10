#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "platform/types.hpp"

namespace rasterizer::model {

struct HemisphericalLight {
  glm::vec3 ground;
  glm::vec3 sky;
};

struct DirectionalLight {
  glm::vec3 direction;
  glm::vec3 color;
  f32 intensity;
};

struct PointLight {
  glm::vec3 position;
  glm::vec3 color;
  f32 range;
  f32 intensity;
};

struct Lighting {
  HemisphericalLight hemispherical;
  DirectionalLight directional;
  std::vector<PointLight> points;
};

} // namespace rasterizer::model
