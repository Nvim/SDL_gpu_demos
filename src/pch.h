#pragma once

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
// #include <cstddef>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "common/logger.h"
#include "common/types.h"
#include "common/util.h"
