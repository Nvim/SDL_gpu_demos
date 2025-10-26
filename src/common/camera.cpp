#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/trigonometric.hpp>
#include <pch.h>

#include "camera.h"

Camera::Camera(float fov, float aspect, float near, float far)
  : fov_{ fov }
  , aspect_{ aspect }
  , near_{ near }
  , far_{ far }
{
  proj_ = glm::perspective(fov, aspect, near, far);
}

void
Camera::Update()
{
  if (!Moved && !Rotated) {
    return;
  }

  if (Moved) {
    Position += glm::vec3(rotation_ * glm::vec4(velocity_ * 0.5f, 0.f));
    model_ = glm::translate(glm::mat4{ 1.f }, Position);
    Moved = false;
  }

  if (Rotated) {
    glm::quat pitchRotation = glm::angleAxis(glm::radians(Pitch), glm::vec3{ 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(glm::radians(Yaw), glm::vec3{ 0.f, -1.f, 0.f });
    rotation_ = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
    Rotated = false;
  }

  view_ = glm::inverse(model_ * rotation_);
}

void
Camera::Poll([[maybe_unused]] SDL_Event& evt)
{
  // TODO
}
