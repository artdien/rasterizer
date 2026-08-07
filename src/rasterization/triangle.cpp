#include "rasterization/triangle.hpp"

namespace rasterizer::rasterization {

TrianglePool::TrianglePool(u32 capacity) : current_ {0u} {
  triangles_.resize(capacity);
}

auto TrianglePool::add(const Triangle& triangle) -> u32 {
  const auto idx {current_.fetch_add(1, std::memory_order_relaxed)};
  triangles_[idx] = triangle;

  return idx;
};

auto TrianglePool::reset() -> void {
  current_.store(0, std::memory_order_relaxed);
}

auto TrianglePool::reserve(u32 capacity) -> void {
  if (triangles_.size() < capacity) {
    triangles_.resize(capacity);
  }
}

} // namespace rasterizer::rasterization
