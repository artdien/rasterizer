#pragma once

#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "model/material.hpp"

namespace rasterizer::model {

struct Primitive {
  std::vector<u32> indices;
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec4> tangents;
  std::vector<glm::vec2> texcoords;

  const Material* material;
};

struct Mesh {
  std::vector<Primitive> primitives;
};

struct Model {
  std::vector<Mesh> meshes;
  std::vector<Material> materials;
  std::vector<Texture> textures;
};

} // namespace rasterizer::model
