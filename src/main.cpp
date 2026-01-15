#include <cstdio>
#include <cstring>
#include <pch.h>

#include "common/engine.h"
#include "common/program.h"

#include <SDL3/SDL_video.h>

#include <apps/grass/grass.h>
#include <apps/pbr/pbr_app.h>
#include <common/logger.h>

namespace {
constexpr i32 SCREEN_WIDTH = 1600;
constexpr i32 SCREEN_HEIGHT = 900;
}

int
main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s [app name]\n", argv[0]);
    return 1;
  }

  Logger::Init();
  LOG_INFO("Starting..");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    LOG_ERROR("Couldn't initialize SDL: {}", SDL_GetError());
    return 1;
  }

  LOG_DEBUG("Initialized SDL");

  auto Device =
    SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                          SDL_GPU_SHADERFORMAT_MSL,
                        true,
                        NULL);

  if (Device == NULL) {
    LOG_ERROR("Couldn't create GPU device: {}", SDL_GetError());
    return 1;
  }
  LOG_DEBUG("Created Device");

  auto Window =
    SDL_CreateWindow("Cool program", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
  if (Window == NULL) {
    LOG_ERROR("Couldn't create SDL window: {}", SDL_GetError());
    return -1;
  }
  LOG_DEBUG("Created Window");

  if (!SDL_ClaimWindowForGPUDevice(Device, Window)) {
    LOG_ERROR("Couldn't claim window: {}", SDL_GetError());
    return -1;
  }
  LOG_DEBUG("GPU claimed Window");

  { // app lifecycle
    Engine engine{ Device, Window };
    if (!engine.Init()) {
      LOG_CRITICAL("Couldn't initialize engine");
      return -1;
    }

    Program* app{ nullptr };
    if (strcmp(argv[1], "grass") == 0) {
      app = new grass::GrassProgram{
        Device, Window, &engine, SCREEN_WIDTH, SCREEN_HEIGHT
      };
    } else if (strcmp(argv[1], "pbr") == 0) {
      app =
        new CubeProgram{ Device, Window, &engine, SCREEN_WIDTH, SCREEN_HEIGHT };
    } else {
      LOG_CRITICAL("{}: invalid application name", argv[1]);
      return 1;
    }

    if (!app->Init()) {
      LOG_CRITICAL("Couldn't init app.");
    } else {
      LOG_INFO("Entering main loop");
      while (!app->ShouldQuit()) {
        if (!app->Poll()) {
          LOG_CRITICAL("App failed to Poll");
          break;
        }
        app->UpdateTime();
        if (app->ShouldQuit()) {
          LOG_INFO("App recieved quit event. exiting..");
          break;
        }
        if (!app->Draw()) {
          LOG_CRITICAL("App failed to draw");
          break;
        };
      }
    }
    delete app;
  }

  LOG_INFO("Releasing window and gpu context");
  SDL_ReleaseWindowFromGPUDevice(Device, Window);
  SDL_DestroyGPUDevice(Device);
  SDL_DestroyWindow(Window);

  LOG_INFO("Cleanup done");
  return 0;
}
