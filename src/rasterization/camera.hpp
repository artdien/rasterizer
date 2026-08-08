#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "platform/types.hpp"

namespace rasterizer::rasterization {

class Camera {
public:
  /// @brief Constructs a camera with the specified viewport dimensions and position.
  ///
  /// @param width    Width of the viewport.
  /// @param height   Height of the viewport.
  /// @param position Initial position of the camera in world space.
  Camera(u32 width, u32 height, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));

  /// @brief Computes and returns the camera view matrix.
  ///
  /// @return The 4x4 view transformation matrix.
  auto view_matrix() const -> glm::mat4;

  /// @brief Computes and returns the camera projection matrix.
  ///
  /// @return The 4x4 projection transformation matrix.
  auto projection_matrix() const -> glm::mat4;

  /// @brief Returns the current camera position in world space.
  ///
  /// @return The position as 3D vector.
  auto position() const -> glm::vec3;

  /// @brief Moves the camera forward along its local Z-axis.
  ///
  /// @param dt Delta time for smooth movement.
  auto move_forward(f32 dt) -> void;

  /// @brief Moves the camera backward along its local Z-axis.
  ///
  /// @param dt Delta time for smooth movement.
  auto move_backward(f32 dt) -> void;

  /// @brief Moves the camera left along its local X-axis.
  ///
  /// @param dt Delta time for smooth movement.
  auto move_left(f32 dt) -> void;

  /// @brief Moves the camera right along its local X-axis.
  ///
  /// @param dt Delta time for smooth movement.
  auto move_right(f32 dt) -> void;

  /// @brief Moves the camera up along its local up axis.
  ///
  /// @param dt Delta time for smooth movement.
  auto move_up(f32 dt) -> void;

  /// @brief Moves the camera down along its local up axis.
  ///
  /// @param dt Delta time for smooth movement.
  auto move_down(f32 dt) -> void;

  /// @brief Rotates the camera view to the left (yaw).
  ///
  /// @param dt Delta time for smooth rotation.
  auto look_left(f32 dt) -> void;

  /// @brief Rotates the camera view to the right (yaw).
  ///
  /// @param dt Delta time for smooth rotation.
  auto look_right(f32 dt) -> void;

  /// @brief Rotates the camera view upwards (pitch).
  ///
  /// @param dt Delta time for smooth rotation.
  auto look_up(f32 dt) -> void;

  /// @brief Rotates the camera view downwards (pitch).
  ///
  /// @param dt Delta time for smooth rotation.
  auto look_down(f32 dt) -> void;

private:
  glm::vec3 position_;
  glm::vec3 front_;
  glm::vec3 up_;
  glm::vec3 right_;

  f32 yaw_;
  f32 pitch_;

  f32 aspect_ratio_;

  void update();
  void rotate_yaw(f32 dt, f32 direction);
  void rotate_pitch(f32 dt, f32 direction);
};

} // namespace rasterizer::rasterization
