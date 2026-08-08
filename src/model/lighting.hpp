#pragma once

#include <glm/vec3.hpp>

namespace rasterizer::model {

struct HemisphericalLighting {
  glm::vec3 ground;
  glm::vec3 sky;
};

struct DirectionalLighting {
  glm::vec3 direction;
  glm::vec3 color;
};

struct Lighting {
  HemisphericalLighting hemispherical;
  DirectionalLighting directional;
};

} // namespace rasterizer::model
