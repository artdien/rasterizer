#include "rasterization/rasterizer.hpp"

#include <algorithm>
#include <optional>

#include <glm/gtx/matrix_major_storage.hpp>

#include "model/material.hpp"

namespace rasterizer::rasterization {

namespace {

struct Extent {
  glm::vec2 min;
  glm::vec2 max;
};

struct Tile {
  glm::uvec2 min;
  glm::uvec2 max;
};

auto viewport_matrix(u32 width, u32 height) -> glm::mat4 {
  const auto half_width {0.5f * width};
  const auto half_height {0.5f * height};

  return glm::rowMajor4(glm::vec4 {half_width, 0.0f, 0.0f, half_width},   //
                        glm::vec4 {0.0f, half_height, 0.0f, half_height}, //
                        glm::vec4 {0.0f, 0.0f, 1.0f, 0.0f},               //
                        glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f}                //
  );
}

auto vertex_matrix(const glm::vec4& p1, const glm::vec4& p2, const glm::vec4& p3) -> glm::mat3 {
  // This is actually the adjugate of the vertex matrix.
  // It is used for performance reasons to avoid computing the inverse of the vertex matrix.
  return glm::rowMajor3(glm::vec3 {p2.y * p3.w - p2.w * p3.y, p1.w * p3.y - p1.y * p3.w, p1.y * p2.w - p1.w * p2.y}, //
                        glm::vec3 {p2.w * p3.x - p2.x * p3.w, p1.x * p3.w - p1.w * p3.x, p1.w * p2.x - p1.x * p2.w}, //
                        glm::vec3 {p2.x * p3.y - p2.y * p3.x, p1.y * p3.x - p1.x * p3.y, p1.x * p2.y - p1.y * p2.x}  //
  );
}

auto clip(const glm::vec4& p0, const glm::vec4& p1, const glm::vec4& p2) -> std::optional<Extent> {
  auto x_min {1.0f}, x_max {-1.0f};
  auto y_min {1.0f}, y_max {-1.0f};
  auto visibility_count {u8 {0u}};

  // For all visible points of vertices (i.e. points inside the view frustum)
  // we can safely calculate their screen coordinates
  // and use that information to already update the screen extent.
  const auto update_extent_visible_vertices {[&](const glm::vec4 p) {
    if (-p.w < p.x && p.x < p.w) {
      visibility_count++;

      if (p.x < x_min * p.w) {
        x_min = p.x / p.w;
      }
      if (p.x > x_max * p.w) {
        x_max = p.x / p.w;
      }
    }

    if (-p.w < p.y && p.y < p.w) {
      visibility_count++;

      if (p.y < y_min * p.w) {
        y_min = p.y / p.w;
      }
      if (p.y > y_max * p.w) {
        y_max = p.y / p.w;
      }
    }
  }};

  update_extent_visible_vertices(p0);
  update_extent_visible_vertices(p1);
  update_extent_visible_vertices(p2);

  // There are three cases that we need to handle in total,
  // depending on how many vertices of the triangle are actually visible.

  // -- CASE 1 --

  // All vertices are invisible, i.e. outside view frustum.
  // We can skip this triangle.
  if (visibility_count == 0) {
    return {};
  }

  // -- CASE 2 --

  // All vertices are visible, i.e. inside view frustum.
  // In this case we already calculated the screen extent above and can return it.
  if (visibility_count == 6) {
    return std::make_optional(Extent {
        .min = {x_min, y_min},
        .max = {x_max, y_max},
    });
  }

  // -- CASE 3 --

  // Vertices are partially visible and invisible, i.e. triangle overlaps view frustum.
  // - For visible vertices we already calculated and updated the screen extent above.
  // - For invisible vertices we now 'push' the already calculated screen extent
  //   to their respective view frustum boundaries (where applicable).
  const auto update_extent_invisible_vertices {[&](glm::vec4 p) {
    if (p.x < -p.w && p.x < x_min * p.w) {
      x_min = -1.0f;
    }
    if (p.x > p.w && p.x > x_max * p.w) {
      x_max = 1.0f;
    }

    if (p.y < -p.w && p.y < y_min * p.w) {
      y_min = -1.0f;
    }
    if (p.y > p.w && p.y > y_max * p.w) {
      y_max = 1.0f;
    }
  }};

  update_extent_invisible_vertices(p0);
  update_extent_invisible_vertices(p1);
  update_extent_invisible_vertices(p2);

  return std::make_optional(Extent {
      .min = {x_min, y_min},
      .max = {x_max, y_max},
  });
}

auto overlapping_tiles(const Triangle& triangle, u32 tile_size) -> Tile {
  return {
      .min = {triangle.min.x / tile_size,                                   //
              triangle.min.y / tile_size},                                  //
      .max = {triangle.max.x > 0u ? (triangle.max.x - 1) / tile_size : 0u,  //
              triangle.max.y > 0u ? (triangle.max.y - 1) / tile_size : 0u}, //
  };
}

auto rasterize_triangle(buffer::FramebufferView<u32> output, buffer::FramebufferView<f32> depth, const Triangle& triangle, const Tile& tile) -> void {
  // We need to calculate the intersection between the tile coordinates and the screen extent of the triangle,
  // otherwise we might rasterize outside the tile if the triangle is overlapping multiple tiles.
  const auto x_min {std::max(triangle.min.x, tile.min.x)};
  const auto x_max {std::min(triangle.max.x, tile.max.x)};
  const auto y_min {std::max(triangle.min.y, tile.min.y)};
  const auto y_max {std::min(triangle.max.y, tile.max.y)};

  const auto E0 {triangle.v0.edge};
  const auto E1 {triangle.v1.edge};
  const auto E2 {triangle.v2.edge};

  const glm::vec3 p {(static_cast<f32>(x_min) + 0.5f), (static_cast<f32>(y_min) + 0.5f), 1.0f};
  auto e0_start {glm::dot(E0, p)};
  auto e1_start {glm::dot(E1, p)};
  auto e2_start {glm::dot(E2, p)};

  for (auto y {y_min}; y < y_max; ++y) {
    auto e0 {e0_start};
    auto e1 {e1_start};
    auto e2 {e2_start};

    for (auto x {x_min}; x < x_max; ++x) {
      if (e0 >= 0 && e1 >= 0 && e2 >= 0) {
        // Coefficients for perspective-correct interpolation
        const auto r {1.0f / (e0 + e1 + e2)};
        const auto f0 {r * e0};
        const auto f1 {r * e1};
        const auto f2 {r * e2};

        if (auto z {f0 * triangle.v0.position.z + f1 * triangle.v1.position.z + f2 * triangle.v2.position.z}; z < depth.at(x, y)) {
          const auto u {f0 * triangle.v0.uv.s + f1 * triangle.v1.uv.s + f2 * triangle.v2.uv.s};
          const auto v {f0 * triangle.v0.uv.t + f1 * triangle.v1.uv.t + f2 * triangle.v2.uv.t};

          depth.at(x, y) = z;
          output.at(x, y) = triangle.material->albedo->sample(u, v);
        }
      }

      e0 += E0.x;
      e1 += E1.x;
      e2 += E2.x;
    }

    e0_start += E0.y;
    e1_start += E1.y;
    e2_start += E2.y;
  }
}

} // namespace

Rasterizer::Rasterizer(u32 width, u32 height, u32 tile_size) : tile_size_ {tile_size}, depth_ {width, height}, bins_ {width, height, tile_size} {}

auto Rasterizer::rasterize(buffer::FramebufferView<u32> output, const model::Model& model, const glm::mat4& view, const glm::mat4& projection) -> void {
  bins_.reset();
  triangles_.reset();

  output.clear(0u);
  depth_.view().clear(std::numeric_limits<f32>::infinity());

  process_triangles(model, view, projection);
  bin_triangles();
  rasterize_tiles(output);
}

auto Rasterizer::process_triangles(const model::Model& model, const glm::mat4& view, const glm::mat4& projection) -> void {
  const auto depth {depth_.view()};
  const auto width {depth.width()};
  const auto height {depth.height()};

  const auto transformation {projection * view};
  const auto viewport {viewport_matrix(width, height)};

  for (const auto& mesh : model.meshes) {
    for (const auto& primitive : mesh.primitives) {
      for (auto i {0uz}; i < primitive.indices.size(); i += 3) {
        const auto i0 {primitive.indices[i]};
        const auto i1 {primitive.indices[i + 1]};
        const auto i2 {primitive.indices[i + 2]};

        // -- Apply View and Projection Transformation --

        auto p0 {transformation * glm::vec4 {primitive.positions[i0], 1.0f}};
        auto p1 {transformation * glm::vec4 {primitive.positions[i1], 1.0f}};
        auto p2 {transformation * glm::vec4 {primitive.positions[i2], 1.0f}};

        // -- Clip Triangle --

        const auto result {clip(p0, p1, p2)};
        if (!result.has_value()) {
          continue;
        }

        // -- Transform to Viewport Coordinates --

        p0 = viewport * p0;
        p1 = viewport * p1;
        p2 = viewport * p2;

        // -- Backface Culling --

        auto M {vertex_matrix(p0, p1, p2)};

        // If determinant is (close to) zero, the triangle doesn't have visible surface area, so we skip it.
        // If determinant is negative, the triangle is back-facing, so we skip it.
        if (const auto det {M[0].z * p0.w + M[1].z * p1.w + M[2].z * p2.w}; det < 1e-6) {
          continue;
        }

        // -- Calculate Screen Extent --

        const auto extent {result.value()};
        const auto x_min {static_cast<u32>(glm::max(0.0f, 0.5f * width * (extent.min.x + 1.0f)))};
        const auto x_max {static_cast<u32>(glm::min(static_cast<f32>(width), 0.5f * width * (extent.max.x + 1.0f) + 1.0f))};
        const auto y_min {static_cast<u32>(glm::max(0.0f, 0.5f * height * (extent.min.y + 1.0f)))};
        const auto y_max {static_cast<u32>(glm::min(static_cast<f32>(height), 0.5f * height * (extent.max.y + 1u) + 1.0f))};

        // -- Add Processed Triangle --

        const auto v0 {Vertex {
            .position = p0,
            .normal = primitive.normals[i0],
            .edge = M[0],
            .uv = primitive.texcoords[i0],
        }};
        const auto v1 {Vertex {
            .position = p1,
            .normal = primitive.normals[i1],
            .edge = M[1],
            .uv = primitive.texcoords[i1],
        }};
        const auto v2 {Vertex {
            .position = p2,
            .normal = primitive.normals[i2],
            .edge = M[2],
            .uv = primitive.texcoords[i2],
        }};

        triangles_.add({
            .v0 = v0,
            .v1 = v1,
            .v2 = v2,
            .min = {x_min, y_min},
            .max = {x_max, y_max},
            .material = primitive.material,
        });
      }
    }
  }
}

auto Rasterizer::bin_triangles() -> void {

  // -- Count Triangles per Tile --

  for (auto idx {0u}; idx < triangles_.size(); ++idx) {
    const auto [min, max] {overlapping_tiles(triangles_[idx], tile_size_)};

    for (u32 ty = min.y; ty <= max.y; ++ty) {
      for (u32 tx = min.x; tx <= max.x; ++tx) {
        bins_.count(tx, ty);
      }
    }
  }

  // --  Allocate Memory for Binning --

  bins_.allocate();

  // --  Bin Triangles --

  for (auto idx {0u}; idx < triangles_.size(); ++idx) {
    const auto [min, max] {overlapping_tiles(triangles_[idx], tile_size_)};

    for (u32 ty = min.y; ty <= max.y; ++ty) {
      for (u32 tx = min.x; tx <= max.x; ++tx) {
        bins_.add(tx, ty, idx);
      }
    }
  }
}

auto Rasterizer::rasterize_tiles(buffer::FramebufferView<u32> output) -> void {
  const auto tiles_x {bins_.tiles_x()};
  const auto tiles_y {bins_.tiles_y()};

  for (auto t {0u}; t < tiles_x * tiles_y; ++t) {
    const auto tx {t % tiles_x};
    const auto ty {t / tiles_x};

    const auto x_min {tx * tile_size_};
    const auto x_max {std::min(x_min + tile_size_, output.width())};
    const auto y_min {ty * tile_size_};
    const auto y_max {std::min(y_min + tile_size_, output.height())};

    for (auto idx : bins_.bin(tx, ty)) {
      rasterize_triangle(output, depth_.view(), triangles_[idx], {.min = {x_min, y_min}, .max = {x_max, y_max}});
    }
  }
}

} // namespace rasterizer::rasterization
