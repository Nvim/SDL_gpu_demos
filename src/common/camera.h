#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
  // clang-format off
  Camera(
    float fov=60.0f,
    float aspect=4/3.f,
    float near=.1f,
    float far=100.f
  );
  // clang-format on
  void Update(f32 delta);
  const glm::mat4& Projection() const { return proj_; }
  const glm::mat4& View() const { return view_; }
  const glm::mat4& Model() const { return model_; }
  const glm::mat4& Rotation() const { return rotation_; }

public:
  glm::vec3 Position{ 0.f, 0.f, 4.f };
  float Pitch{ 0.f };
  float Yaw{ 0.f };
  bool Moved{ true };
  bool Rotated{ true };

private:
  void handle_input(f32 delta);

private:
  glm::mat4 proj_;
  glm::mat4 view_;
  glm::mat4 model_;
  glm::mat4 rotation_;

  // glm::vec3 lookat_{ 0.0f, 0.0f, 0.0f }; // direction camera is looking at
  // glm::vec3 up_{ 0.0f, -1.0f, 0.0f };
  // glm::vec3 right_{ 1.0f, 0.0f, 0.0f };
  // TODO: storing these for GUI config
  float fov_;
  [[maybe_unused]] float aspect_;
  [[maybe_unused]] float near_;
  [[maybe_unused]] float far_;
};
