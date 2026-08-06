#include "rasterization/triangle.hpp"

namespace rasterizer::rasterization {

TrianglePool::TrianglePool(u32 capacity) : current_ {0u} {
  triangles_.resize(capacity);
}

auto TrianglePool::add(const Triangle& triangle) -> u32 {
  if (current_ >= triangles_.size()) {
    triangles_.resize(2 * triangles_.size());
  }

  triangles_[current_] = triangle;

  return current_++;
};

auto TrianglePool::reset() -> void {
  current_ = 0u;
}

} // namespace rasterizer::rasterization
