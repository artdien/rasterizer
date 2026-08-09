#include "rasterization/rasterizer.hpp"

#include <algorithm>
#include <optional>
#include <ranges>

#include <glm/gtx/matrix_major_storage.hpp>

#include "model/material.hpp"

namespace rasterizer::rasterization {

namespace {

const auto WORLD_UP {glm::vec3 {0.0f, 1.0f, 0.0f}}; // glTF specification assumes positive Y-axis as world up vector
constexpr auto BASE_REFLECTIVITY {0.04f};           // commonly used base reflectivity for dielectrics

struct Extent {
  glm::vec2 min;
  glm::vec2 max;
};

auto color(glm::vec3 value) -> u32 {
  // Convert to sRGB space for correct display
  value = glm::pow(glm::clamp(value, 0.0f, 1.0f), glm::vec3(1.0f / 2.2f));

  const auto r {static_cast<u32>(value.r * 255.0f)};
  const auto g {static_cast<u32>(value.g * 255.0f)};
  const auto b {static_cast<u32>(value.b * 255.0f)};

  return (r << 16) | (g << 8) | b;
}

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
    return Extent {
        .min = {x_min, y_min},
        .max = {x_max, y_max},
    };
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

  return Extent {
      .min = {x_min, y_min},
      .max = {x_max, y_max},
  };
}

auto overlapping_tiles(const Triangle& triangle, u32 tile_size) -> Tile {
  return {
      .min = {triangle.min.x / tile_size,                                   //
              triangle.min.y / tile_size},                                  //
      .max = {triangle.max.x > 0u ? (triangle.max.x - 1) / tile_size : 0u,  //
              triangle.max.y > 0u ? (triangle.max.y - 1) / tile_size : 0u}, //
  };
}

auto rasterize_triangle(buffer::FramebufferView<u32> output, buffer::FramebufferView<f32> depth, const Triangle& triangle, const Tile& tile,
                        const model::Lighting& lighting, const glm::vec3& camera_position) -> void {
  // We need to calculate the intersection between the tile coordinates and the screen extent of the triangle,
  // otherwise we might rasterize outside the tile if the triangle is overlapping multiple tiles.
  const auto x_min {std::max(triangle.min.x, tile.min.x)};
  const auto x_max {std::min(triangle.max.x, tile.max.x)};
  const auto y_min {std::max(triangle.min.y, tile.min.y)};
  const auto y_max {std::min(triangle.max.y, tile.max.y)};

  // Hoist material and lighting constants to avoid repeated pointer dereferences
  const auto* material {triangle.material};
  const auto alpha_cutoff {material->alpha_cutoff};
  const auto masked {material->masked};
  const auto base_color_texture {material->base_color};
  const auto base_color_factor {material->base_color_factor};
  const auto metallic_roughness_texture {material->metallic_roughness};
  const auto metallic_factor {material->metallic_factor};
  const auto roughness_factor {material->roughness_factor};
  const auto normal_texture {material->normal};
  const auto normal_scale {material->normal_scale};
  const auto occlusion_texture {material->occlusion};
  const auto occlusion_strength {material->occlusion_strength};
  const auto emissive_texture {material->emissive};
  const auto emissive_factor {material->emissive_factor};

  const auto L_d {-lighting.directional.direction};
  const auto directional_color {lighting.directional.color};
  const auto ground_color {lighting.hemispherical.ground};
  const auto sky_color {lighting.hemispherical.sky};

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

          auto base_color {base_color_factor};
          if (base_color_texture) {
            const auto sampled {base_color_texture->sample(u, v)};
            // Convert to linear space for correct lighting calculations
            base_color *= glm::vec4 {glm::pow(glm::vec3 {sampled}, glm::vec3 {2.2f}), sampled.a};
          }

          // Alpha cutoff: skip lighting and depth update if the pixel is discarded
          if (masked && base_color.a < alpha_cutoff) {
            e0 += E0.x;
            e1 += E1.x;
            e2 += E2.x;
            continue;
          }

          const auto position {f0 * triangle.v0.position_world + f1 * triangle.v1.position_world + f2 * triangle.v2.position_world};
          const auto normal {glm::normalize(f0 * triangle.v0.normal + f1 * triangle.v1.normal + f2 * triangle.v2.normal)};
          const auto tangent_signed {f0 * triangle.v0.tangent + f1 * triangle.v1.tangent + f2 * triangle.v2.tangent};
          const auto tangent {glm::normalize(glm::vec3 {tangent_signed} - normal * glm::dot(normal, glm::vec3 {tangent_signed}))};
          const auto bitangent {glm::cross(normal, tangent) * tangent_signed.w};

          auto N {normal};
          if (normal_texture) {
            auto sampled_normal {glm::normalize(glm::vec3 {material->normal->sample(u, v)} * 2.0f - 1.0f)};
            sampled_normal *= glm::vec3 {normal_scale, normal_scale, 1.0f};

            N = glm::normalize(tangent * sampled_normal.x + bitangent * sampled_normal.y + normal * sampled_normal.z);
          }

          auto occlusion {1.0f};
          if (occlusion_texture) {
            occlusion = glm::mix(1.0f, occlusion_texture->sample(u, v).r, occlusion_strength);
          }

          auto emissive {emissive_factor};
          if (emissive_texture) {
            emissive *= glm::vec3 {emissive_texture->sample(u, v)};
          }

          auto metallic {metallic_factor};
          auto roughness {roughness_factor};
          if (metallic_roughness_texture) {
            const auto sampled {metallic_roughness_texture->sample(u, v)};
            metallic *= sampled.b;
            roughness *= sampled.g;
          }

          const auto albedo {glm::vec3 {base_color}};

          const auto diffuse_color {albedo * (1.0f - metallic)};
          const auto specular_color {glm::mix(glm::vec3 {BASE_REFLECTIVITY}, albedo, metallic)};
          const auto specular_exponent {glm::pow(2.0f, 10.0f * (1.0f - roughness)) * 128.0f};

          const auto V {glm::normalize(camera_position - position)};
          const auto H_d {glm::normalize(L_d + V)};

          auto diffuse {glm::vec3 {0.0f}};
          auto specular {glm::vec3 {0.0f}};

          // -- Ambient Lighting --

          const auto weight {glm::dot(N, WORLD_UP) * 0.5f + 0.5f};
          const auto ambient {glm::mix(ground_color, sky_color, weight) * albedo * occlusion};

          // -- Directional Lighting --

          diffuse += glm::max(0.0f, glm::dot(N, L_d)) * diffuse_color * directional_color;
          specular += glm::pow(glm::max(0.0f, glm::dot(N, H_d)), specular_exponent) * specular_color * directional_color;

          // -- Point Lighting --

          for (const auto& point : lighting.points) {
            const auto light {point.position - position};
            auto distance {glm::length(light)};

            if (distance < point.range) {
              distance = glm::max(0.01f, distance);

              const auto L_p {light / distance};
              const auto H_p {glm::normalize(L_p + V)};

              const auto ratio {distance / point.range};
              const auto attenuation {glm::max(glm::min(1.0f - ratio * ratio * ratio * ratio, 1.0f), 0.0f) / (distance * distance)};

              diffuse += glm::max(0.0f, glm::dot(N, L_p)) * diffuse_color * point.color * attenuation;
              specular += glm::pow(glm::max(0.0f, glm::dot(N, H_p)), specular_exponent) * specular_color * point.color * attenuation;
            }
          }

          output.at(x, y) = color(ambient + diffuse + specular + emissive);
          depth.at(x, y) = z;
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

Rasterizer::Rasterizer(u32 width, u32 height, u32 threads, u32 tile_size)
    : tile_size_ {tile_size}, depth_ {width, height}, bins_ {width, height, tile_size}, threads_ {threads} {}

auto Rasterizer::rasterize(buffer::FramebufferView<u32> output, const model::Model& model, const model::Lighting& lighting, const Camera& camera) -> void {
  bins_.reset();
  triangles_.reset();
  threads_.reset();

  output.clear(0u);
  depth_.view().clear(std::numeric_limits<f32>::infinity());

  process_triangles(model, camera.view_matrix(), camera.projection_matrix());
  bin_triangles();
  rasterize_tiles(output, lighting, camera.position());
}

auto Rasterizer::process_triangles(const model::Model& model, const glm::mat4& view, const glm::mat4& projection) -> void {
  const auto depth {depth_.view()};
  const auto width {depth.width()};
  const auto height {depth.height()};

  const auto transformation {projection * view};
  const auto viewport {viewport_matrix(width, height)};

  const auto triangles_total {std::ranges::fold_left(                                   //
      model.meshes                                                                      //
          | std::views::transform([](const auto& m) { return m.primitives; })           //
          | std::views::join                                                            //
          | std::views::transform([](const auto& p) { return p.indices.size() / 3u; }), //
      0u, std::plus<u32>())};

  triangles_.reserve(triangles_total);

  for (const auto& mesh : model.meshes) {
    for (const auto& primitive : mesh.primitives) {
      const auto triangles_per_primitive {primitive.indices.size() / 3u};
      const auto triangles_per_thread {triangles_per_primitive / threads_.capacity()};
      const auto remainder {triangles_per_primitive % threads_.capacity()};

      auto begin {0u};
      auto end {0u};

      for (auto i {0u}; i < threads_.capacity(); ++i) {
        end += triangles_per_thread + (i < remainder ? 1u : 0u);

        // Since we're inside an inner loop and we only synchronize after the outer loop has finished,
        // we need to capture a pointer to the primitive by value to avoid a potential race condition
        // when the reference goes stale in the next loop iteration.
        const auto* prim {&primitive};

        threads_.schedule(
            [&, prim, begin, end] {
              for (auto i {begin}; i < end; ++i) {
                const auto i0 {prim->indices[3u * i]};
                const auto i1 {prim->indices[3u * i + 1u]};
                const auto i2 {prim->indices[3u * i + 2u]};

                const auto p0_world {glm::vec4 {prim->positions[i0], 1.0f}};
                const auto p1_world {glm::vec4 {prim->positions[i1], 1.0f}};
                const auto p2_world {glm::vec4 {prim->positions[i2], 1.0f}};

                // -- Apply View and Projection Transformation --

                const auto p0_clip {transformation * p0_world};
                const auto p1_clip {transformation * p1_world};
                const auto p2_clip {transformation * p2_world};

                // -- Transform to Viewport Coordinates --

                const auto p0 {viewport * p0_clip};
                const auto p1 {viewport * p1_clip};
                const auto p2 {viewport * p2_clip};

                // -- Backface Culling --

                auto M {vertex_matrix(p0, p1, p2)};

                // If determinant is (close to) zero, the triangle doesn't have visible surface area, so we skip it.
                // If determinant is negative, the triangle is back-facing, so we skip it.
                if (const auto det {M[0].z * p0.w + M[1].z * p1.w + M[2].z * p2.w}; det < 1e-6) {
                  continue;
                }

                // -- Clip Triangle --

                const auto result {clip(p0_clip, p1_clip, p2_clip)};
                if (!result.has_value()) {
                  continue;
                }

                // -- Calculate Screen Extent --

                const auto extent {result.value()};
                const auto x_min {static_cast<u32>(glm::max(0.0f, 0.5f * width * (extent.min.x + 1.0f)))};
                const auto x_max {static_cast<u32>(glm::min(static_cast<f32>(width), 0.5f * width * (extent.max.x + 1.0f) + 1.0f))};
                const auto y_min {static_cast<u32>(glm::max(0.0f, 0.5f * height * (extent.min.y + 1.0f)))};
                const auto y_max {static_cast<u32>(glm::min(static_cast<f32>(height), 0.5f * height * (extent.max.y + 1.0f) + 1.0f))};

                // -- Add Processed Triangle --

                const auto v0 {Vertex {
                    .position = p0,
                    .position_world = p0_world,
                    .normal = prim->normals[i0],
                    .tangent = prim->tangents[i0],
                    .edge = M[0],
                    .uv = prim->texcoords[i0],
                }};
                const auto v1 {Vertex {
                    .position = p1,
                    .position_world = p1_world,
                    .normal = prim->normals[i1],
                    .tangent = prim->tangents[i1],
                    .edge = M[1],
                    .uv = prim->texcoords[i1],
                }};
                const auto v2 {Vertex {
                    .position = p2,
                    .position_world = p2_world,
                    .normal = prim->normals[i2],
                    .tangent = prim->tangents[i2],
                    .edge = M[2],
                    .uv = prim->texcoords[i2],
                }};

                triangles_.add({
                    .v0 = v0,
                    .v1 = v1,
                    .v2 = v2,
                    .min = {x_min, y_min},
                    .max = {x_max, y_max},
                    .material = prim->material,
                });
              }
            },
            false);

        begin = end;
      }
    }
  }

  threads_.notify();
  threads_.sync();
}

auto Rasterizer::bin_triangles() -> void {
  const auto triangles_total {triangles_.size()};
  const auto triangles_per_thread {triangles_total / threads_.capacity()};
  const auto remainder {triangles_total % threads_.capacity()};

  bins_.reserve(triangles_total);
  tiles_.resize(triangles_total);

  auto begin {0u};
  std::atomic<u32> count {0u};

  for (auto i {0u}; i < threads_.capacity(); ++i) {
    auto end {begin + triangles_per_thread + (i < remainder ? 1u : 0u)};

    threads_.schedule(
        [&, begin, end] {
          // -- Count Triangles per Tile --

          for (auto idx {begin}; idx < end; ++idx) {
            tiles_[idx] = overlapping_tiles(triangles_[idx], tile_size_);
            const auto range {tiles_[idx]};

            for (auto ty {range.min.y}; ty <= range.max.y; ++ty) {
              for (auto tx {range.min.x}; tx <= range.max.x; ++tx) {
                bins_.count(tx, ty);
              }
            }
          }

          // --  Allocate Memory for Binning --

          threads_.barrier();

          if (count.fetch_add(1) == 0) {
            bins_.allocate();
          }

          threads_.barrier();

          // --  Bin Triangles --

          for (auto idx {begin}; idx < end; ++idx) {
            const auto range {tiles_[idx]};

            for (auto ty {range.min.y}; ty <= range.max.y; ++ty) {
              for (auto tx {range.min.x}; tx <= range.max.x; ++tx) {
                bins_.add(tx, ty, idx);
              }
            }
          }
        },
        false);

    begin = end;
  }

  threads_.notify();
  threads_.sync();
}

auto Rasterizer::rasterize_tiles(buffer::FramebufferView<u32> output, const model::Lighting& lighting, const glm::vec3& camera_position) -> void {
  const auto tiles_x {bins_.tiles_x()};
  const auto tiles_y {bins_.tiles_y()};
  const auto stride {threads_.capacity()};

  for (auto i {0u}; i < stride; ++i) {
    threads_.schedule(
        [&, i, stride, tiles_x, tiles_y] {
          for (auto tile {i}; tile < tiles_x * tiles_y; tile += stride) {
            const auto tx {tile % tiles_x};
            const auto ty {tile / tiles_x};

            const auto x_min {tx * tile_size_};
            const auto x_max {std::min(x_min + tile_size_, output.width())};
            const auto y_min {ty * tile_size_};
            const auto y_max {std::min(y_min + tile_size_, output.height())};

            for (auto idx : bins_.bin(tx, ty)) {
              rasterize_triangle(output, depth_.view(), triangles_[idx], {.min = {x_min, y_min}, .max = {x_max, y_max}}, lighting, camera_position);
            }
          }
        },
        false);
  }

  threads_.notify();
  threads_.sync();
}

} // namespace rasterizer::rasterization
