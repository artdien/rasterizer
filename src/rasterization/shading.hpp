#pragma once

#include <concepts>
#include <numbers>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "model/lighting.hpp"

namespace rasterizer::shading {

namespace constants {

inline const auto WORLD_UP {glm::vec3 {0.0f, 1.0f, 0.0f}};
inline constexpr auto BASE_REFLECTIVITY {0.04f};
inline constexpr auto PI {std::numbers::pi_v<f32>};
inline constexpr auto INV_PI {std::numbers::inv_pi_v<f32>};
inline constexpr auto INV_EIGHT {1.0f / 8.0f};

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

static_assert(Shader<BlinnPhongShader>);

struct CookTorranceShader {
  auto operator()(const ShadingContext& context, const model::Lighting& lighting, glm::vec3 V) const -> glm::vec3 {
    auto result {glm::vec3 {0.0f}};

    const auto N {context.normal};
    const auto metallic {context.metallic};
    const auto roughness {context.roughness};

    // -- F0 (Base Reflectivity)  --

    const auto F0 {glm::mix(glm::vec3 {constants::BASE_REFLECTIVITY}, context.albedo, metallic)};

    // -- Diffuse --

    const auto diffuse {context.albedo * constants::INV_PI * (1.0f - metallic)};

    // -- Ambient Light --

    const auto weight {glm::dot(N, constants::WORLD_UP) * 0.5f + 0.5f};
    result += glm::mix(lighting.hemispherical.ground, lighting.hemispherical.sky, weight) * context.albedo * context.occlusion * (1.0f - metallic);

    const auto evaluate_light = [&, F0](glm::vec3 L, glm::vec3 light_color, f32 attenuation) {
      const auto NdotL {glm::dot(N, L)};
      if (NdotL <= 0.0f) {
        return;
      }

      const auto NdotV {glm::dot(N, V)};
      if (NdotV <= 0.0f) {
        return;
      }

      const auto H {glm::normalize(L + V)};
      const auto NdotH {glm::dot(N, H)};
      const auto NdotV_abs {glm::abs(NdotV)};

      // -- Normal Distribution Function: Trowbridge-Reitz GGX --

      const auto alpha {roughness * roughness};
      const auto alpha_squared {alpha * alpha};
      const auto denominator {NdotH * NdotH * (alpha_squared - 1.0f) + 1.0f};
      const auto D {alpha_squared / (constants::PI * denominator * denominator)};

      // -- Geometry Function: Schlick GGX --

      const auto roughness_plus_one {roughness + 1.0f};
      const auto k {constants::INV_EIGHT * roughness_plus_one * roughness_plus_one};
      const auto k_one_minus {1.0f - k};

      const auto Gv {NdotV_abs / (NdotV_abs * k_one_minus + k)};
      const auto Gl {NdotL / (NdotL * k_one_minus + k)};
      const auto G {Gv * Gl};

      // -- Fresnel Equation: Schlick Approximation --

      const auto pow {1.0f - glm::dot(H, V)};
      const auto F {F0 + (glm::vec3 {1.0f} - F0) * pow * pow * pow * pow * pow};

      // -- Specular Cook-Torrance --

      const auto specular {(D * G * F) / glm::max(4.0f * NdotV_abs * NdotL, 0.001f)};
      result += NdotL * light_color * (diffuse * (glm::vec3 {1.0f} - F) + specular) * attenuation;
    };

    // -- Directional Light --

    const auto L_d {-lighting.directional.direction};
    evaluate_light(L_d, lighting.directional.color, 1.0f);

    // -- Point Lights --

    for (const auto& point : lighting.points) {
      const auto light_to_pos {point.position - context.position};
      const auto distance_squared {glm::dot(light_to_pos, light_to_pos)};

      if (distance_squared < point.range * point.range) {
        const auto distance {glm::max(0.01f, glm::sqrt(distance_squared))};

        const auto L_p {light_to_pos / distance};
        const auto ratio {distance / point.range};
        const auto attenuation {glm::max(glm::min(1.0f - ratio * ratio * ratio * ratio, 1.0f), 0.0f) / (distance * distance)};

        evaluate_light(L_p, point.color, attenuation);
      }
    }

    result += context.emissive;

    return result;
  }
};

static_assert(Shader<CookTorranceShader>);

} // namespace rasterizer::shading
