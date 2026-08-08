#include "rasterization/camera.hpp"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

namespace rasterizer::rasterization {

namespace {

constexpr auto WORLD_UP {glm::vec3 {0.0f, 1.0f, 0.0f}}; // glTF specification assumes positive Y-axis as world up vector
constexpr auto VERTICAL_FIELD_OF_VIEW {60.0f};          // in degrees
constexpr auto NEAR_PLANE {0.1f};                       // in world units
constexpr auto FAR_PLANE {100.0f};                      // in world units
constexpr auto MOVEMENT_SPEED {3.0f};                   // world units per second
constexpr auto ROTATION_SPEED {45.0f};                  // degrees per second

} // namespace

Camera::Camera(u32 width, u32 height, glm::vec3 position)
    : position_ {position}, yaw_ {-90.0f}, pitch_ {0.0f}, aspect_ratio_ {static_cast<f32>(width) / static_cast<f32>(height)} {
  update();
}

auto Camera::view_matrix() const -> glm::mat4 {
  return glm::lookAt(position_, position_ + front_, up_);
}

auto Camera::projection_matrix() const -> glm::mat4 {
  return glm::mat4 {glm::perspective(glm::radians(VERTICAL_FIELD_OF_VIEW), aspect_ratio_, NEAR_PLANE, FAR_PLANE)};
}

auto Camera::position() const -> glm::vec3 {
  return position_;
}

auto Camera::move_forward(f32 dt) -> void {
  position_ += front_ * MOVEMENT_SPEED * dt;
}

auto Camera::move_backward(f32 dt) -> void {
  position_ -= front_ * MOVEMENT_SPEED * dt;
}

auto Camera::move_left(f32 dt) -> void {
  position_ -= right_ * MOVEMENT_SPEED * dt;
}

auto Camera::move_right(f32 dt) -> void {
  position_ += right_ * MOVEMENT_SPEED * dt;
}

auto Camera::move_up(f32 dt) -> void {
  position_ += up_ * MOVEMENT_SPEED * dt;
}

auto Camera::move_down(f32 dt) -> void {
  position_ -= up_ * MOVEMENT_SPEED * dt;
}

auto Camera::look_left(f32 dt) -> void {
  rotate_yaw(dt, -1.0f);
}

auto Camera::look_right(f32 dt) -> void {
  rotate_yaw(dt, 1.0f);
}

auto Camera::look_up(f32 dt) -> void {
  rotate_pitch(dt, 1.0f);
}

auto Camera::look_down(f32 dt) -> void {
  rotate_pitch(dt, -1.0f);
}

auto Camera::update() -> void {
  front_ = glm::normalize(glm::vec3 {glm::cos(glm::radians(yaw_)) * glm::cos(glm::radians(pitch_)), //
                                     glm::sin(glm::radians(pitch_)),                                //
                                     glm::sin(glm::radians(yaw_)) * glm::cos(glm::radians(pitch_))});
  right_ = glm::normalize(glm::cross(front_, WORLD_UP));
  up_ = glm::normalize(glm::cross(right_, front_));
}

auto Camera::rotate_yaw(f32 dt, f32 direction) -> void {
  yaw_ += direction * ROTATION_SPEED * dt;
  update();
}

auto Camera::rotate_pitch(f32 dt, f32 direction) -> void {
  // Clamp pitch to prevent gimbal lock
  pitch_ = glm::clamp(pitch_ + direction * ROTATION_SPEED * dt, -89.0f, 89.0f);
  update();
}

} // namespace rasterizer::rasterization
