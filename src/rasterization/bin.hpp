#include <atomic>
#include <span>
#include <vector>

#include "platform/types.hpp"

namespace rasterizer::rasterization {

class BinPool {
public:
  /// @brief Constructs a bin pool for a given framebuffer resolution.
  ///
  /// The BinPool operates in a multi-stage process:
  /// 1. Counting:   Call count() to count number of triangles per tile.
  /// 2. Allocation: Call allocate() to allocate the exact memory needed for bins.
  /// 3. Binning:    Call add() to add triangle indices to corresponding bin.
  ///
  /// @param width     Width of the framebuffer.
  /// @param height    Height of the framebuffer.
  /// @param tile_size The dimension of each square tile in pixels.
  BinPool(u32 width, u32 height, u32 tile_size = 16);

  BinPool(const BinPool&) = delete;
  BinPool(BinPool&&) = delete;
  auto operator=(const BinPool&) -> BinPool& = delete;
  auto operator=(BinPool&&) -> BinPool& = delete;
  ~BinPool() = default;

  /// @brief Increments the triangle count for a specific tile.
  ///
  /// This method is thread-safe and can be called concurrently by multiple threads
  /// during the counting phase of rasterization.
  ///
  /// @param tx Tile index along X-axis.
  /// @param ty Tile index along Y-axis.
  auto count(u32 tx, u32 ty) -> void;

  /// @brief Finalizes the counts and allocates memory for the bins.
  ///
  /// It must be called after all count() calls are complete and before any add() calls begin.
  auto allocate() -> void;

  /// @brief Adds a triangle index to the bin of a specific tile.
  ///
  /// This assumes that allocate() has already been called to allocate sufficient memory.
  ///
  /// @param tx           Tile index along X-axis.
  /// @param ty           Tile index along Y-axis.
  /// @param triangle_idx The unique index of the triangle being added.
  auto add(u32 tx, u32 ty, u32 triangle_idx) -> void;

  /// @brief Returns a view of all triangle indices assigned to a specific tile.
  ///
  /// @param tx Tile index along X-axis.
  /// @param ty Tile index along Y-axis.
  ///
  /// @return A span containing the triangle indices for this tile.
  auto bin(u32 tx, u32 ty) -> std::span<u32>;

  /// @brief Resets all tile counts to zero.
  ///
  /// This prepares the pool for a new frame of rasterization.
  auto reset() -> void;

  /// @brief Returns the number of tiles along the X-axis.
  auto tiles_x() const -> u32 {
    return tiles_x_;
  }

  /// @brief Returns the number of tiles along the Y-axis.
  auto tiles_y() const -> u32 {
    return tiles_y_;
  }

private:
  u32 tiles_x_;
  u32 tiles_y_;

  std::vector<u32> bins_;
  std::vector<u32> offsets_;
  std::vector<std::atomic<u32>> counts_;
};

} // namespace rasterizer::rasterization
