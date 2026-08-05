#include "model/loading.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
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

auto load_file(const std::filesystem::path& filepath) -> fastgltf::Asset {
  if (!std::filesystem::exists(filepath)) {
    throw std::runtime_error(std::format("File does not exist: {}", filepath.string()));
  }

  auto filebuffer {fastgltf::GltfDataBuffer::FromPath(filepath)};
  if (!filebuffer) {
    throw std::runtime_error(std::format("File '{}' could not be read", filepath.filename().string()));
  }

  auto parser {fastgltf::Parser {}};
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

    return {
        stbi_load_from_memory(data, size, &width, &height, &channels, 4),
        deleter,
        static_cast<u32>(width),
        static_cast<u32>(height),
        static_cast<u32>(channels),
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

  material_.base.x = material.pbrData.baseColorFactor.x();
  material_.base.y = material.pbrData.baseColorFactor.y();
  material_.base.z = material.pbrData.baseColorFactor.z();
  material_.base.w = material.pbrData.baseColorFactor.w();

  // For now, we only support albedo textures
  if (const auto& idx {material.pbrData.baseColorTexture}; idx.has_value()) {
    material_.albedo = &textures[idx.value().textureIndex];
  }

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
    Primitive primitive_;

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
        }
      }
    });
  }

  const auto primitives {model.meshes | std::views::transform(&Mesh::primitives) | std::views::join};
  const auto primitives_count {std::ranges::distance(primitives)};
  const auto vertices_count {std::ranges::distance(primitives | std::views::transform(&Primitive::indices) | std::views::join)};

  std::println("Model '{}' successfully loaded:", filepath.filename().string());
  std::println("  Number of meshes: {}", model.meshes.size());
  std::println("  Number of primitives: {}", primitives_count);
  std::println("  Number of vertices: {}", vertices_count);
  std::println("  Number of materials: {}", model.materials.size());
  std::println("  Number of textures: {}", model.textures.size());

  return model;
}

} // namespace rasterizer::model
