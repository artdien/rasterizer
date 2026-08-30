#include "model/loading.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <print>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <stb_image.h>

#include "model/material.hpp"
#include "platform/types.hpp"

namespace rasterizer::model {

namespace {

auto calculate_tangents(Primitive* primitive) -> void {

  // The glTF specification recommends using MikkTSpace to calculate tangents in case they are missing.
  // However, for simplicity we calculate 'linear' tangents here instead.
  // For this, we assume that no vertices need to be pre-split (e.g. for shared vertices at mirror seams).

  primitive->tangents.resize(primitive->positions.size());

  for (usize i = 0; i < primitive->indices.size(); i += 3) {
    // Calculate face tangent
    const auto i0 {primitive->indices[i]};
    const auto i1 {primitive->indices[i + 1]};
    const auto i2 {primitive->indices[i + 2]};

    const auto p0 {primitive->positions[i0]};
    const auto p1 {primitive->positions[i1]};
    const auto p2 {primitive->positions[i2]};

    const auto uv0 {primitive->texcoords[i0]};
    const auto uv1 {primitive->texcoords[i1]};
    const auto uv2 {primitive->texcoords[i2]};

    const auto edge1 {p1 - p0};
    const auto edge2 {p2 - p0};
    const auto duv1 {uv1 - uv0};
    const auto duv2 {uv2 - uv0};

    const auto denominator {duv1.x * duv2.y - duv2.x * duv1.y};
    const auto f {std::abs(denominator) < 1e-6f ? 0.0f : 1.0f / denominator};
    const auto tangent {f * (duv2.y * edge1 - duv1.y * edge2)};
    const auto bitangent {f * (-duv2.x * edge1 + duv1.x * edge2)};

    const auto face_normal = glm::normalize(glm::cross(edge1, edge2));
    const auto sign {(glm::dot(glm::cross(face_normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f};

    // Accumulate and average calculated tangents and signs
    primitive->tangents[i0] += glm::vec4 {tangent, sign};
    primitive->tangents[i1] += glm::vec4 {tangent, sign};
    primitive->tangents[i2] += glm::vec4 {tangent, sign};
  }

  // Gram-Schmidt orthogonalization and normalization
  for (auto i {0u}; i < primitive->tangents.size(); ++i) {
    const auto normal {primitive->normals[i]};
    const auto tangent {glm::vec3 {primitive->tangents[i]}};
    const auto sign {primitive->tangents[i].w >= 0.0f ? 1.0f : -1.0f};

    if (const auto orthogonal_tangent {tangent - normal * glm::dot(normal, tangent)}; glm::length(orthogonal_tangent) > 1e-6f) {
      primitive->tangents[i] = glm::vec4 {glm::normalize(orthogonal_tangent), sign};
    } else {
      auto fallback {glm::vec3 {1.0f, 0.0f, 0.0f}};
      if (std::abs(glm::dot(normal, fallback)) > 0.9f) {
        fallback = {0.0f, 1.0f, 0.0f};
      }

      primitive->tangents[i] = glm::vec4 {glm::normalize(fallback - normal * glm::dot(normal, fallback)), sign};
    }
  }
}

auto load_file(const std::filesystem::path& filepath) -> fastgltf::Asset {
  if (!std::filesystem::exists(filepath)) {
    throw std::runtime_error(std::format("File does not exist: {}", filepath.string()));
  }

  auto filebuffer {fastgltf::GltfDataBuffer::FromPath(filepath)};
  if (!filebuffer) {
    throw std::runtime_error(std::format("File '{}' could not be read", filepath.filename().string()));
  }

  auto parser {fastgltf::Parser {fastgltf::Extensions::KHR_lights_punctual}};
  auto asset {parser.loadGltf(filebuffer.get(), filepath.parent_path(),
                              fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages)};
  if (asset.error() != fastgltf::Error::None) {
    throw std::runtime_error(std::format("File '{}' could not be parsed (error code: {})", filepath.filename().string(), static_cast<u64>(asset.error())));
  }

  if (const auto num_scenes {asset->scenes.size()}; num_scenes != 1) {
    throw std::runtime_error(std::format("File '{}' contains {} scenes, but expected exactly one", filepath.filename().string(), num_scenes));
  }

  return std::move(asset.get());
}

auto load_texture(const fastgltf::Asset& asset, const fastgltf::Texture& texture) -> Texture {
  auto texture_ {Texture {}};

  // -- LOAD IMAGE --

  if (const auto& idx {texture.imageIndex}; !idx.has_value()) {
    throw std::runtime_error(std::format("Texture '{}' is missing image", texture.name));
  }

  const auto deleter {[](uc8* data) { stbi_image_free(data); }};

  const auto load_from_memory {[&](const stbi_uc* data, i32 size) -> Texture {
    auto width {i32 {}};
    auto height {i32 {}};
    auto channels {i32 {}};
    const auto image {stbi_load_from_memory(data, size, &width, &height, &channels, 4)};

    return {
        // pass desired channels here (i.e. 4), not actual channels in data
        image, deleter, static_cast<u32>(width), static_cast<u32>(height), 4u,
    };
  }};

  const auto load_from_array {[&](const std::byte* buffer, usize length) -> Texture {
    const auto data {reinterpret_cast<const stbi_uc*>(buffer)};
    const auto size {static_cast<i32>(length)};
    return load_from_memory(data, size);
  }};

  texture_ = std::visit(fastgltf::visitor {
                            [&](auto& _) { return Texture {}; },
                            [&](const fastgltf::sources::Array& array) { return load_from_array(array.bytes.data(), array.bytes.size()); },
                            [&](const fastgltf::sources::BufferView& view) {
                              auto& buffer_view {asset.bufferViews[view.bufferViewIndex]};
                              auto& buffer {asset.buffers[buffer_view.bufferIndex]};

                              return std::visit(fastgltf::visitor {
                                                    [&](auto _) { return Texture {}; },
                                                    [&](const fastgltf::sources::Array& array) {
                                                      const auto data {array.bytes.data() + buffer_view.byteOffset};
                                                      const auto size {buffer_view.byteLength};

                                                      return load_from_array(data, size);
                                                    },
                                                },
                                                buffer.data);
                            },
                        },
                        asset.images[texture.imageIndex.value()].data);

  // -- LOAD SAMPLER --

  if (const auto& idx {texture.samplerIndex}; !idx.has_value()) {
    throw std::runtime_error(std::format("Texture '{}' is missing sampler", texture.name));
  }

  const auto map_texture_wrap {[](fastgltf::Wrap wrap) {
    switch (wrap) {
      case fastgltf::Wrap::Repeat:
        return TextureWrap::REPEAT;
      case fastgltf::Wrap::MirroredRepeat:
        return TextureWrap::MIRRORED_REPEAT;
      case fastgltf::Wrap::ClampToEdge:
        return TextureWrap::CLAMP_TO_EDGE;
    }
  }};

  texture_.wrap_s = map_texture_wrap(asset.samplers[texture.samplerIndex.value()].wrapS);
  texture_.wrap_t = map_texture_wrap(asset.samplers[texture.samplerIndex.value()].wrapT);

  return texture_;
}

auto load_material(const std::vector<Texture>& textures, const fastgltf::Material& material) -> Material {
  auto material_ {Material {}};

  if (const auto& info {material.pbrData.baseColorTexture}; info.has_value()) {
    material_.base_color = &textures[info.value().textureIndex];
  }

  if (const auto& info {material.pbrData.metallicRoughnessTexture}; info.has_value()) {
    material_.metallic_roughness = &textures[info.value().textureIndex];
  }

  if (const auto& info {material.normalTexture}; info.has_value()) {
    material_.normal = &textures[info.value().textureIndex];
    material_.normal_scale = info.value().scale;
  }

  if (const auto& info {material.occlusionTexture}; info.has_value()) {
    material_.occlusion = &textures[info.value().textureIndex];
    material_.occlusion_strength = info.value().strength;
  }

  if (const auto& info {material.emissiveTexture}; info.has_value()) {
    material_.emissive = &textures[info.value().textureIndex];
  }

  material_.base_color_factor = {
      material.pbrData.baseColorFactor.x(),
      material.pbrData.baseColorFactor.y(),
      material.pbrData.baseColorFactor.z(),
      material.pbrData.baseColorFactor.w(),
  };

  material_.metallic_factor = material.pbrData.metallicFactor;
  material_.roughness_factor = material.pbrData.roughnessFactor;

  material_.emissive_factor = {
      material.emissiveFactor.x(),
      material.emissiveFactor.y(),
      material.emissiveFactor.z(),
  };

  material_.alpha_cutoff = material.alphaCutoff;
  material_.masked = material.alphaMode == fastgltf::AlphaMode::Mask;

  return material_;
}

template <typename T>
auto load_attributes(const fastgltf::Asset& asset, usize accessor_idx, std::vector<T>* attributes) -> void {
  if (const auto& accessor {asset.accessors[accessor_idx]}; accessor.bufferViewIndex.has_value()) {
    attributes->resize(accessor.count);
    fastgltf::iterateAccessorWithIndex<T>(asset, accessor, [&](T attribute, usize idx) { (*attributes)[idx] = attribute; });
  } else {
    throw std::runtime_error(std::format("Accessor {} is missing buffer view", accessor_idx));
  }
}

auto load_mesh(const fastgltf::Asset& asset, const std::vector<Material>& materials, const fastgltf::Mesh& mesh) -> Mesh {
  auto mesh_ {Mesh {}};

  for (const auto& primitive : mesh.primitives) {
    auto primitive_ {Primitive {}};

    auto missing_tangents {false};

    if (const auto* it {primitive.findAttribute("POSITION")}; it != std::end(primitive.attributes)) {
      load_attributes(asset, it->accessorIndex, &primitive_.positions);
    } else {
      throw std::runtime_error(std::format("Mesh '{}' is missing position attributes", mesh.name));
    }

    if (const auto* it {primitive.findAttribute("NORMAL")}; it != std::end(primitive.attributes)) {
      load_attributes(asset, it->accessorIndex, &primitive_.normals);
    } else {
      throw std::runtime_error(std::format("Mesh '{}' is missing normal attributes", mesh.name));
    }

    if (const auto* it {primitive.findAttribute("TANGENT")}; it != std::end(primitive.attributes)) {
      load_attributes(asset, it->accessorIndex, &primitive_.tangents);
    } else {
      missing_tangents = true;
    }

    if (const auto* it {primitive.findAttribute("TEXCOORD_0")}; it != std::end(primitive.attributes)) {
      load_attributes(asset, it->accessorIndex, &primitive_.texcoords);
    } else {
      throw std::runtime_error(std::format("Mesh '{}' is missing texture coordinate attributes", mesh.name));
    }

    // Indices should always exist, since we enabled the option to generate indices when loading the file.
    // Thus, this shouldn't be necessary, but it doesn't hurt to check either.
    if (const auto& idx {primitive.indicesAccessor}; idx.has_value()) {
      load_attributes(asset, idx.value(), &primitive_.indices);
    } else {
      throw std::runtime_error(std::format("Mesh '{}' is missing indices", mesh.name));
    }

    if (const auto& idx {primitive.materialIndex}; idx.has_value()) {
      primitive_.material = &materials[idx.value()];
    } else {
      throw std::runtime_error(std::format("Mesh '{}' is missing material", mesh.name));
    }

    if (missing_tangents) {
      calculate_tangents(&primitive_);
    }

    mesh_.primitives.emplace_back(std::move(primitive_));
  }

  return mesh_;
}

} // namespace

auto load_model(const std::filesystem::path& filepath, bool convert_to_world_coordinates) -> Model {
  const auto asset {load_file(filepath)};

  auto model {Model {}};

  std::ranges::for_each(asset.textures, [&](const auto& texture) { model.textures.emplace_back(load_texture(asset, texture)); });
  std::ranges::for_each(asset.materials, [&](const auto& material) { model.materials.emplace_back(load_material(model.textures, material)); });
  std::ranges::for_each(asset.meshes, [&](const auto& mesh) { model.meshes.emplace_back(load_mesh(asset, model.materials, mesh)); });

  if (convert_to_world_coordinates) {
    fastgltf::iterateSceneNodes(asset, 0u, fastgltf::math::fmat4x4 {}, [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
      if (const auto& idx {node.meshIndex}; idx.has_value()) {
        const auto inv_t {transpose(inverse(fastgltf::math::fmat3x3(matrix)))};
        auto& mesh {model.meshes[idx.value()]};

        for (auto& primitive : mesh.primitives) {
          for (auto& position : primitive.positions) {
            const auto row_1 {matrix.row(0)};
            const auto row_2 {matrix.row(1)};
            const auto row_3 {matrix.row(2)};

            const auto position_x {row_1.x() * position.x + row_1.y() * position.y + row_1.z() * position.z + row_1.w()};
            const auto position_y {row_2.x() * position.x + row_2.y() * position.y + row_2.z() * position.z + row_2.w()};
            const auto position_z {row_3.x() * position.x + row_3.y() * position.y + row_3.z() * position.z + row_3.w()};

            position.x = position_x;
            position.y = position_y;
            position.z = position_z;
          }

          for (auto& normal : primitive.normals) {
            const auto row_1 {inv_t.row(0)};
            const auto row_2 {inv_t.row(1)};
            const auto row_3 {inv_t.row(2)};

            const auto normal_x {row_1.x() * normal.x + row_1.y() * normal.y + row_1.z() * normal.z};
            const auto normal_y {row_2.x() * normal.x + row_2.y() * normal.y + row_2.z() * normal.z};
            const auto normal_z {row_3.x() * normal.x + row_3.y() * normal.y + row_3.z() * normal.z};

            normal.x = normal_x;
            normal.y = normal_y;
            normal.z = normal_z;
          }

          for (auto& tangent : primitive.tangents) {
            const auto row_1 {matrix.row(0)};
            const auto row_2 {matrix.row(1)};
            const auto row_3 {matrix.row(2)};

            const auto tangent_x {row_1.x() * tangent.x + row_1.y() * tangent.y + row_1.z() * tangent.z};
            const auto tangent_y {row_2.x() * tangent.x + row_2.y() * tangent.y + row_2.z() * tangent.z};
            const auto tangent_z {row_3.x() * tangent.x + row_3.y() * tangent.y + row_3.z() * tangent.z};

            tangent.x = tangent_x;
            tangent.y = tangent_y;
            tangent.z = tangent_z;
          }
        }
      }
    });
  }

  return model;
}

auto load_lighting(const std::filesystem::path& filepath) -> Lighting {
  const auto asset {load_file(filepath)};

  auto lighting {Lighting {}};

  fastgltf::iterateSceneNodes(asset, 0u, fastgltf::math::fmat4x4 {}, [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
    if (const auto& idx {node.lightIndex}; idx.has_value()) {
      const auto& light {asset.lights[idx.value()]};

      const auto row_1 {matrix.row(0)};
      const auto row_2 {matrix.row(1)};
      const auto row_3 {matrix.row(2)};

      // For now, we only support exactly one directional light.
      // If more than one directional lights are present, the last one in this iteration wins.
      if (light.type == fastgltf::LightType::Directional) {
        const auto direction {glm::vec3 {0.0f, 0.0f, -1.0f}};

        lighting.directional.direction = glm::normalize(glm::vec3 {
            row_1.x() * direction.x + row_1.y() * direction.y + row_1.z() * direction.z,
            row_2.x() * direction.x + row_2.y() * direction.y + row_2.z() * direction.z,
            row_3.x() * direction.x + row_3.y() * direction.y + row_3.z() * direction.z,
        });
        lighting.directional.color = {light.color.x(), light.color.y(), light.color.z()};
        lighting.directional.intensity = light.intensity;
      } else if (light.type == fastgltf::LightType::Point) {
        // For point lights we only care about the translation component of the transformation matrix,
        // as rotating point lights doesn't make sense.
        const auto position {glm::vec3 {row_1.w(), row_2.w(), row_3.w()}};
        const auto color {glm::vec3 {light.color.x(), light.color.y(), light.color.z()}};

        lighting.points.emplace_back(position, color, light.range.value_or(std::numeric_limits<f32>::infinity()), light.intensity);
      }
    }
  });

  return lighting;
}

auto print_model_info(const Model& model) -> void {
  const auto primitives {model.meshes | std::views::transform(&Mesh::primitives) | std::views::join};
  const auto primitives_count {std::ranges::distance(primitives)};
  const auto vertices_count {std::ranges::distance(primitives | std::views::transform(&Primitive::positions) | std::views::join)};
  const auto indices_count {std::ranges::distance(primitives | std::views::transform(&Primitive::indices) | std::views::join)};

  std::println("MODEL INFO");
  std::println("  Meshes: {}", model.meshes.size());
  std::println("  Primitives: {}", primitives_count);
  std::println("  Vertices: {}", vertices_count);
  std::println("  Indices: {}", indices_count);
  std::println("  Materials: {}", model.materials.size());
  std::println("  Textures: {}", model.textures.size());
}

} // namespace rasterizer::model
