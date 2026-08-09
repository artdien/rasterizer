#pragma once

#include <concepts>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "model/lighting.hpp"

namespace rasterizer::shading {

namespace constants {

inline const auto WORLD_UP {glm::vec3 {0.0f, 1.0f, 0.0f}};
inline constexpr auto BASE_REFLECTIVITY {0.04f};

} // namespace constants

struct ShadingContext {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 albedo;
  glm::vec3 emissive;
  f32 metallic;
  f32 roughness;
  f32 occlusion;
};

template <typename T>
concept Shader = requires(T t, ShadingContext context, model::Lighting lighting, glm::vec3 V) {
  { t(context, lighting, V) } -> std::same_as<glm::vec3>;
};

struct BlinnPhongShader {
  auto operator()(const ShadingContext& context, const model::Lighting& lighting, glm::vec3 V) const -> glm::vec3 {
    auto result {glm::vec3 {0.0f}};

    const auto diffuse_color {context.albedo * (1.0f - context.metallic)};
    const auto specular_color {glm::mix(glm::vec3 {constants::BASE_REFLECTIVITY}, context.albedo, context.metallic)};
    const auto specular_exponent {glm::pow(2.0f, 10.0f * (1.0f - context.roughness)) * 128.0f};

    const auto L_d {-lighting.directional.direction};

    // -- Ambient Light --

    const auto weight {glm::dot(context.normal, constants::WORLD_UP) * 0.5f + 0.5f};
    result += glm::mix(lighting.hemispherical.ground, lighting.hemispherical.sky, weight) * context.albedo * context.occlusion;

    // -- Directional Light --

    const auto H_d {glm::normalize(L_d + V)};
    result += glm::max(0.0f, glm::dot(context.normal, L_d)) * diffuse_color * lighting.directional.color;
    result += glm::pow(glm::max(0.0f, glm::dot(context.normal, H_d)), specular_exponent) * specular_color * lighting.directional.color;

    // -- Point Lights --

    for (const auto& point : lighting.points) {
      const auto light {point.position - context.position};
      const auto distance_squared {glm::dot(light, light)};

      if (distance_squared < point.range * point.range) {
        const auto distance {glm::max(0.01f, glm::sqrt(distance_squared))};

        const auto L_p {light / distance};
        const auto H_p {glm::normalize(L_p + V)};

        const auto ratio {distance / point.range};
        const auto attenuation {glm::max(glm::min(1.0f - ratio * ratio * ratio * ratio, 1.0f), 0.0f) / (distance * distance)};

        result += glm::max(0.0f, glm::dot(context.normal, L_p)) * diffuse_color * point.color * attenuation;
        result += glm::pow(glm::max(0.0f, glm::dot(context.normal, H_p)), specular_exponent) * specular_color * point.color * attenuation;
      }
    }

    result += context.emissive;

    return result;
  }
};

} // namespace rasterizer::shading
