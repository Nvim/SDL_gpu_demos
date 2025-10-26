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
  void Poll(SDL_Event& evt);
  void Update();
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
  glm::vec3 velocity_{ 0.f, 0.f, 0.f };
  glm::mat4 proj_;
  glm::mat4 view_;
  glm::mat4 model_;
  glm::mat4 rotation_;
  // TODO: storing these for GUI config
  float fov_;
  [[maybe_unused]] float aspect_;
  [[maybe_unused]] float near_;
  [[maybe_unused]] float far_;
  glm::vec3 up_;
};
