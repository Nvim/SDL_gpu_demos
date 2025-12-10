#include <pch.h>

#include "common/engine.h"

#include <SDL3/SDL_video.h>

#include <apps/grass/grass.h>
// #include <apps/pbr/pbr_app.h>
#include <common/logger.h>

int
main()
{
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

  auto Window = SDL_CreateWindow("Cool program", 1200, 900, 0);
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
    grass::GrassProgram app{ Device, Window, &engine, 1200, 900 };
    // CubeProgram app{ Device, Window, &engine, 1600, 1200 };

    if (!app.Init()) {
      LOG_CRITICAL("Couldn't init app.");
    } else {
      LOG_INFO("Entering main loop");
      while (!app.ShouldQuit()) {
        if (!app.Poll()) {
          LOG_CRITICAL("App failed to Poll");
          break;
        }
        app.UpdateTime();
        if (app.ShouldQuit()) {
          LOG_INFO("App recieved quit event. exiting..");
          break;
        }
        if (!app.Draw()) {
          LOG_CRITICAL("App failed to draw");
          break;
        };
      }
    }
  }

  LOG_INFO("Releasing window and gpu context");
  SDL_ReleaseWindowFromGPUDevice(Device, Window);
  SDL_DestroyGPUDevice(Device);
  SDL_DestroyWindow(Window);

  LOG_INFO("Cleanup done");
  return 0;
}
