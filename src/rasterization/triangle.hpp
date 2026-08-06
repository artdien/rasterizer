#pragma once

#include <new>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "model/material.hpp"
#include "platform/types.hpp"

namespace rasterizer::rasterization {

struct Vertex {
  alignas(16) glm::vec4 position;
  alignas(16) glm::vec3 normal;
  alignas(16) glm::vec3 edge;
  alignas(8) glm::vec2 uv;
};

static_assert(alignof(Vertex) == 16);
static_assert(sizeof(Vertex) == 64);

struct alignas(std::hardware_constructive_interference_size) Triangle {
  Vertex v0, v1, v2;
  glm::uvec2 min, max;
  const model::Material* material;
};

static_assert(alignof(Triangle) >= 64);
static_assert(sizeof(Triangle) == 256);

class TrianglePool {
public:
  /// @brief Constructs a triangle pool with an initial capacity.
  ///
  /// The pool minimizes allocations by reusing a pre-allocated buffer.
  /// It acts as a growable vector that can be reset without deallocating the underlying storage.
  ///
  /// @param capacity Initial number of triangles to allocate memory for.
  TrianglePool(u32 capacity = 1u);

  TrianglePool(const TrianglePool&) = delete;
  TrianglePool(TrianglePool&&) = delete;
  auto operator=(const TrianglePool&) -> TrianglePool& = delete;
  auto operator=(TrianglePool&&) -> TrianglePool& = delete;
  ~TrianglePool() = default;

  /// @brief Adds a triangle to the pool and returns its assigned index.
  ///
  /// If the current capacity is exceeded,
  /// the pool will automatically double its internal storage.
  ///
  /// @param triangle Triangle data to store.
  ///
  /// @return Index of the added triangle within the pool.
  auto add(const Triangle& triangle) -> u32;

  /// @brief Resets the pool.
  ///
  /// This marks all existing triangles as available for overwrite,
  /// but keeps the allocated memory intact to avoid repeated allocations.
  auto reset() -> void;

  /// @brief Returns the number of active triangles currently in the pool.
  auto size() const -> u32 {
    return current_;
  }

  auto operator[](usize i) -> Triangle& {
    return triangles_[i];
  }

  auto operator[](usize i) const -> const Triangle& {
    return triangles_[i];
  }

private:
  u32 current_;
  std::vector<Triangle> triangles_;
};

} // namespace rasterizer::rasterization
