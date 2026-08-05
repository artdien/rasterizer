#include "rasterization/rasterizer.hpp"

#include <optional>

#include <glm/gtx/matrix_major_storage.hpp>

#include <model/material.hpp>

namespace rasterizer::rasterization {

namespace {

struct Vertex {
  alignas(16) glm::vec4 position;
  alignas(16) glm::vec3 normal;
  alignas(8) glm::vec2 uv;
};

struct Extent {
  glm::vec2 min;
  glm::vec2 max;
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
  return glm::rowMajor3(glm::vec3 {p1.x, p1.y, p1.w}, //
                        glm::vec3 {p2.x, p2.y, p2.w}, //
                        glm::vec3 {p3.x, p3.y, p3.w}  //
  );
}

auto clip(const Vertex& v0, const Vertex& v1, const Vertex& v2) -> std::optional<Extent> {
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

  update_extent_visible_vertices(v0.position);
  update_extent_visible_vertices(v1.position);
  update_extent_visible_vertices(v2.position);

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

  update_extent_invisible_vertices(v0.position);
  update_extent_invisible_vertices(v1.position);
  update_extent_invisible_vertices(v2.position);

  return std::make_optional(Extent {
      .min = {x_min, y_min},
      .max = {x_max, y_max},
  });
}

auto rasterize_triangle(buffer::FramebufferView<u32> output, buffer::FramebufferView<f32> depth, const Vertex& v0, const Vertex& v1, const Vertex& v2,
                        const model::Material* material) -> void {

  // -- Transform to Viewport Coordinates --

  const auto viewport {viewport_matrix(output.width(), output.height())};
  const auto p0 {viewport * v0.position};
  const auto p1 {viewport * v1.position};
  const auto p2 {viewport * v2.position};

  // -- Calculate Vertex Matrix --

  auto M {vertex_matrix(p0, p1, p2)};

  // If determinant is (close to) zero, the triangle doesn't have visible surface area, so we skip it.
  // If determinant is negative, the triangle is back-facing, so we skip it.
  if (const auto det {glm::determinant(M)}; det < 1e-6) {
    return;
  }

  M = glm::inverse(M);

  // -- Clip Triangles and Calculate Screen Extent --

  const auto result {clip(v0, v1, v2)};
  if (!result.has_value()) {
    return;
  }

  const auto extent {result.value()};
  const auto width {static_cast<f32>(output.width())};
  const auto height {static_cast<f32>(output.height())};

  const auto x_min {static_cast<u32>(glm::max(0.0f, 0.5f * width * (extent.min.x + 1.0f)))};
  const auto x_max {static_cast<u32>(glm::min(width, 0.5f * width * (extent.max.x + 1.0f) + 1.0f))};
  const auto y_min {static_cast<u32>(glm::max(0.0f, 0.5f * height * (extent.min.y + 1.0f)))};
  const auto y_max {static_cast<u32>(glm::min(height, 0.5f * height * (extent.max.y + 1.0f) + 1.0f))};

  // -- Calculate Edge Functions --

  const auto E1 {M[0]};
  const auto E2 {M[1]};
  const auto E3 {M[2]};

  // -- Rasterize Triangle --

  for (auto y {y_min}; y < y_max; ++y) {
    for (auto x {x_min}; x < x_max; ++x) {
      const glm::vec3 p {(static_cast<f32>(x) + 0.5f), (static_cast<f32>(y) + 0.5f), 1.0f};
      auto e1 {glm::dot(E1, p)};
      auto e2 {glm::dot(E2, p)};
      auto e3 {glm::dot(E3, p)};

      if (e1 >= 0 && e2 >= 0 && e3 >= 0) {
        // Coefficients for perspective-correct interpolation
        const auto r {1.0f / (e1 + e2 + e3)};
        const auto f1 {r * e1};
        const auto f2 {r * e2};
        const auto f3 {r * e3};

        if (auto z {f1 * p0.z + f2 * p1.z + f3 * p2.z}; z < depth.at(x, y)) {
          const auto u {f1 * v0.uv.s + f2 * v1.uv.s + f3 * v2.uv.s};
          const auto v {f1 * v0.uv.t + f2 * v1.uv.t + f3 * v2.uv.t};

          depth.at(x, y) = z;
          output.at(x, y) = material->albedo->sample(u, v);
        }
      }
    }
  }
}

} // namespace

Rasterizer::Rasterizer(u32 width, u32 height) : depth_ {width, height} {}

auto Rasterizer::rasterize(buffer::FramebufferView<u32> output, const model::Model& model, const glm::mat4& view, const glm::mat4& projection) -> void {
  auto depth {depth_.view()};

  output.clear(0u);
  depth.clear(std::numeric_limits<f32>::infinity());

  const auto transformation {projection * view};

  for (const auto& mesh : model.meshes) {
    for (const auto& primitive : mesh.primitives) {
      for (auto i {0uz}; i < primitive.indices.size(); i += 3) {
        const auto i0 {primitive.indices[i]};
        const auto i1 {primitive.indices[i + 1]};
        const auto i2 {primitive.indices[i + 2]};

        const auto v0 {Vertex {
            .position = transformation * glm::vec4 {primitive.positions[i0], 1.0f},
            .normal = primitive.normals[i0],
            .uv = primitive.texcoords[i0],
        }};
        const auto v1 {Vertex {
            .position = transformation * glm::vec4 {primitive.positions[i1], 1.0f},
            .normal = primitive.normals[i1],
            .uv = primitive.texcoords[i1],
        }};
        const auto v2 {Vertex {
            .position = transformation * glm::vec4 {primitive.positions[i2], 1.0f},
            .normal = primitive.normals[i2],
            .uv = primitive.texcoords[i2],
        }};

        rasterize_triangle(output, depth, v0, v1, v2, primitive.material);
      }
    }
  }
}

} // namespace rasterizer::rasterization
