#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
  // clang-format off
  Camera(
    float fov=glm::radians(60.f),
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

  void SetFov(f32 fov_radians);
  void SetAspect(f32 aspect);
  void SetNearFar(f32 near, f32 far);

private:
  void handle_input(f32 delta);

private:
  glm::mat4 proj_;
  glm::mat4 view_;
  glm::mat4 model_;
  glm::mat4 rotation_;

  // TODO: storing these for GUI config
  float fov_;
  float aspect_;
  float near_;
  float far_;
};
