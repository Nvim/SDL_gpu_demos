#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

// Program uses SDL GPU and Window to draw, but isn't responsible for
// creating/releasing them.
class Program
{
public:
  SDL_GPUDevice* Device;
  SDL_Window* Window;
  float DeltaTime{ 0.0f };
  float lastTime{ 0.0f };
  // static inline std::shared_ptr<spdlog::logger> s_app_logger{};

public:
  Program(SDL_GPUDevice* device, SDL_Window* window)
    : Device{ device }
    , Window{ window } {};
  ~Program() {};

  virtual bool Init() = 0;
  virtual bool Poll() = 0;
  virtual bool Draw() = 0;
  virtual bool ShouldQuit() = 0;
  void UpdateTime()
  {
    float newTime = SDL_GetTicks() / 1000.0f;
    DeltaTime = newTime - lastTime;
    lastTime = newTime;
  }
};
