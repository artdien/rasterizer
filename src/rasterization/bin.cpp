#include "rasterization/bin.hpp"

namespace rasterizer::rasterization {

BinPool::BinPool(u32 width, u32 height, u32 tile_size)
    : tiles_x_ {(width + tile_size - 1) / tile_size}, tiles_y_ {(height + tile_size - 1) / tile_size}, offsets_(tiles_x_ * tiles_y_),
      counts_(tiles_x_ * tiles_y_) {}

auto BinPool::count(u32 tx, u32 ty) -> void {
  const auto idx {tx + ty * tiles_x_};

  counts_[idx].fetch_add(1, std::memory_order_relaxed);
}

auto BinPool::allocate() -> void {
  const auto tiles {tiles_x_ * tiles_y_};
  u32 total_triangles {0};

  for (auto i {0u}; i < tiles; ++i) {
    offsets_[i] = total_triangles;
    total_triangles += counts_[i].load(std::memory_order_relaxed);
  }

  bins_.resize(total_triangles);
}

auto BinPool::add(u32 tx, u32 ty, u32 triangle_idx) -> void {
  const auto idx {tx + ty * tiles_x_};
  const auto tile {offsets_[idx]};

  bins_[tile] = triangle_idx;
  offsets_[idx] += 1;
}

auto BinPool::bin(u32 tx, u32 ty) -> std::span<u32> {
  const auto idx {tx + ty * tiles_x_};
  const auto count {counts_[idx].load(std::memory_order_relaxed)};

  return std::span {bins_.data() + offsets_[idx] - count, count};
}

auto BinPool::reset() -> void {
  const auto tiles {tiles_x_ * tiles_y_};

  for (auto i {0u}; i < tiles; ++i) {
    counts_[i].store(0, std::memory_order_relaxed);
  }
}

} // namespace rasterizer::rasterization
