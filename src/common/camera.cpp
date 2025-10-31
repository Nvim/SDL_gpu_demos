#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
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
Camera::Update(f32 delta)
{
  handle_input(delta);
  if (!Moved && !Rotated) {
    return;
  }

  if (Moved) {
    model_ = glm::translate(glm::mat4{ 1.f }, Position);
    Moved = false;
  }

  if (Rotated) {
    glm::quat pitchRotation =
      glm::angleAxis(glm::radians(Pitch), glm::vec3{ 1.f, 0.f, 0.f });
    glm::quat yawRotation =
      glm::angleAxis(glm::radians(Yaw), glm::vec3{ 0.f, -1.f, 0.f });
    rotation_ = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
    Rotated = false;
  }

  view_ = glm::inverse(model_ * rotation_);
}

void
Camera::handle_input(f32 delta)
{
  delta /= 1000.f;
  f32 speed = 8.f * delta;
  f32 sens = 80.f * delta;
  const auto* state = SDL_GetKeyboardState(nullptr);
  glm::vec3 velocity;

  // Movement
  if (state[SDL_SCANCODE_W]) {
    // Position += (lookat_ * speed);
    velocity.z = -1;
    Moved = true;
  }
  if (state[SDL_SCANCODE_S]) {
    // Position -= (lookat_ * speed);
    velocity.z = 1;
    Moved = true;
  }
  if (state[SDL_SCANCODE_A]) {
    velocity.x = -1;
    Moved = true;
  }
  if (state[SDL_SCANCODE_D]) {
    // Position += (right_ * speed);
    velocity.x = 1;
    Moved = true;
  }

  // Camera
  if (state[SDL_SCANCODE_UP]) {
    Pitch = std::min(Pitch + sens, 89.0f);
    Rotated = true;
  }
  if (state[SDL_SCANCODE_DOWN]) {
    Pitch = std::max(Pitch - sens, -89.0f);
    Rotated = true;
  }
  if (state[SDL_SCANCODE_LEFT]) {
    Yaw -= sens;
    Rotated = true;
  }
  if (state[SDL_SCANCODE_RIGHT]) {
    Yaw += sens;
    Rotated = true;
  }

  if (Moved) {
    Position += glm::vec3(rotation_ * glm::vec4(velocity * 0.5f, 0.f));
  }
}
