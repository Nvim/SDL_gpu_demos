#include <pch.h>

#include "camera.h"

Camera::Camera(float fov, float aspect, float near, float far, glm::vec3 up)
  : fov_{ fov }
  , aspect_{ aspect }
  , near_{ near }
  , far_{ far }
  , up_{ up }
{
  const float tanHalfFovy = tan(fov_ / 2.f);
  proj_ = glm::mat4{ 0.0f };
  proj_[0][0] = 1.f / (aspect * tanHalfFovy);
  proj_[1][1] = 1.f / (tanHalfFovy);
  proj_[2][2] = far / (far - near);
  proj_[2][3] = 1.f;
  proj_[3][2] = -(far * near) / (far - near);
}

void
Camera::setViewTarget()
{
  const glm::vec3 w{ glm::normalize(Target - Position) };
  const glm::vec3 u{ glm::normalize(glm::cross(w, up_)) };
  const glm::vec3 v{ glm::cross(w, u) };

  view_ = glm::mat4{ 1.f };
  view_[0][0] = u.x;
  view_[1][0] = u.y;
  view_[2][0] = u.z;
  view_[0][1] = v.x;
  view_[1][1] = v.y;
  view_[2][1] = v.z;
  view_[0][2] = w.x;
  view_[1][2] = w.y;
  view_[2][2] = w.z;
  view_[3][0] = -glm::dot(u, Position);
  view_[3][1] = -glm::dot(v, Position);
  view_[3][2] = -glm::dot(w, Position);
}
void
Camera::Update()
{
  if (!Touched) {
    return;
  }
  model_ = glm::translate(glm::mat4{ 1.f }, Position);
  setViewTarget();
  Touched = false;
}
