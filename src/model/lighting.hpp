#pragma once

#include <glm/vec3.hpp>

namespace rasterizer::model {

struct HemisphericalLighting {
  glm::vec3 ground;
  glm::vec3 sky;
};

struct Lighting {
  HemisphericalLighting hemispherical;
};

} // namespace rasterizer::model
