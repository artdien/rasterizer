#pragma once

#include <filesystem>

#include "model/lighting.hpp"
#include "model/model.hpp"

namespace rasterizer::model {

/// @brief Loads a 3D model from a glTF (.gltf or .glb) file into the internal Model representation.
///
/// This loader performs a strict import and expects all geometry to be fully defined.
/// If any required attributes are missing, an error is thrown.
///
/// To successfully load a model, the glTF file must provide the following for every primitive:
/// - Positions (`POSITION`)
/// - Normals (`NORMAL`)
/// - Texture coordinates (`TEXCOORD_0`).
///
/// @param filepath Absolute or relative path to the .gltf or .glb file.
/// @param convert_to_world_coordinates If true, traverses the scene graph and bakes all node transformations
///                                     directly into the vertex positions, normals, and tangents.
///                                     If false, vertices remain in local object space.
///
/// @return A Model structure containing processed meshes, materials, and textures ready for rendering.
///
/// @throws std::runtime_error if the file is missing, corrupted, or lacks required attributes.
///
/// @note Instancing is currently not supported.
///       Although a file requiring instancing might load without errors being thrown, the result will be incorrect.
auto load_model(const std::filesystem::path& filepath, bool convert_to_world_coordinates = true) -> Model;

/// @brief Loads lighting information from a glTF (.gltf or .glb) file using the KHR_lights_punctual extension.
///
/// Currently, only directional lights and point lights are supported, spot lights are ignored.
/// The hemispherical lighting data remains unpopulated.
///
/// @param filepath Absolute or relative path to the .gltf or .glb file.
///
/// @return A Lighting structure containing the parsed light data.
///
/// @throws std::runtime_error if the file is missing or corrupted.
///
/// @note If multiple directional lights are present, only one will be used.
auto load_lighting(const std::filesystem::path& filepath) -> Lighting;

/// @brief Prints summary information about the given model to standard output.
///
/// Outputs the number of meshes, primitives, vertices, indices, materials, and textures.
///
/// @param model Loaded model to inspect.
auto print_model_info(const Model& model) -> void;

} // namespace rasterizer::model
