#include "pch.h"
#include "transform.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

const glm::mat4&
Transform::Matrix()
{
  if (Touched) {
    matrix_ = glm::translate(glm::mat4{ 1.f }, translation_);
    matrix_ = glm::rotate(matrix_, rotation_.y, { 0.f, 1.f, 0.f });
    matrix_ = glm::rotate(matrix_, rotation_.x, { 1.f, 0.f, 0.f });
    matrix_ = glm::rotate(matrix_, rotation_.z, { 0.f, 0.f, 1.f });
    matrix_ = glm::scale(matrix_, scale_);
    Touched = false;
  }

  return matrix_;
}
