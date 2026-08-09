#pragma once

#include <atomic>
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
  alignas(16) glm::vec3 position_world;
  alignas(16) glm::vec3 normal;
  alignas(16) glm::vec4 tangent;
  alignas(16) glm::vec3 edge;
  alignas(8) glm::vec2 uv;
};

static_assert(alignof(Vertex) == 16);
static_assert(sizeof(Vertex) == 96);

struct alignas(std::hardware_constructive_interference_size) Triangle {
  Vertex v0, v1, v2;
  glm::uvec2 min, max;
  const model::Material* material;
};

static_assert(alignof(Triangle) >= 64);
static_assert(sizeof(Triangle) == 320);

class TrianglePool {
public:
  /// @brief Constructs a triangle pool with an initial capacity.
  ///
  /// The pool pre-allocates a buffer for the given number of triangles.
  /// It can be reset without deallocating the underlying storage,
  /// and its capacity can be increased explicitly via reserve().
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
  /// The caller must ensure that the pool has sufficient capacity using reserve()
  /// before adding triangles past the initial capacity.
  /// No automatic growth occurs, so out-of-bounds access is possible if not managed externally.
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

  /// @brief Reserves capacity for a given number of triangles.
  ///
  /// This ensures that the pool can hold at least the specified number of triangles.
  ///
  /// @param capacity Minimum number of triangles to reserve space for.
  auto reserve(u32 capacity) -> void;

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
  std::atomic<u32> current_;
  std::vector<Triangle> triangles_;
};

} // namespace rasterizer::rasterization
