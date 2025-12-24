#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_timer.h>

class Engine;

// Program uses SDL GPU and Window to draw, but isn't responsible for
// creating/releasing them.
class Program
{
public:
  SDL_GPUDevice* Device;
  SDL_Window* Window;
  Engine* EnginePtr;
  f32 DeltaTime{ 0.0f };
  f32 lastTime = SDL_GetTicks();
  // static inline std::shared_ptr<spdlog::logger> s_app_logger{};

public:
  Program(SDL_GPUDevice* device, SDL_Window* window, Engine* engine)
    : Device{ device }
    , Window{ window }
    , EnginePtr{ engine } {};
  ~Program() {};

  virtual bool Init() = 0;
  virtual bool Poll() = 0;
  virtual bool Draw() = 0;
  virtual bool ShouldQuit() = 0;
  void UpdateTime()
  {
    f32 newTimeMs = SDL_GetTicks();
    DeltaTime = newTimeMs - lastTime;
    lastTime = newTimeMs;
  }

  // void UpdateTimeDelay() {
  //   static constexpr f64 target_ms = (1 / 60.f) * 1000.f;
  //   f32 newTimeMs = SDL_GetTicks();
  //   f32 delta_ms = newTimeMs - lastTime;
  //   lastTime = newTimeMs;
  //   if (delta_ms < target_ms) {
  //     f64 diff = target_ms - DeltaTime;
  //     SDL_Delay(diff);
  //     LOG_WARN("frame took: {} ms, sleeping for: {} ms", DeltaTime, diff);
  //     DeltaTime = delta_ms;
  //   } else {
  //     DeltaTime = target_ms;
  //   }
  // }
};
