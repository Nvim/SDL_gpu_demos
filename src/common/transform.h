#pragma once

#include <glm/glm.hpp>

struct Transform
{
  glm::vec3 translation_{};
  glm::vec3 scale_{ 1.f, 1.f, 1.f };
  glm::vec3 rotation_{};
  bool Touched{ true };

  const glm::mat4& Matrix();

private:
  glm::mat4 matrix_{ 1.0f };
};
