#pragma once

#include "common/util.h"

#include <SDL3/SDL_gpu.h>

class Skybox
{
public:
  explicit Skybox(const char* dir, SDL_Window* window, SDL_GPUDevice* device);
  explicit Skybox(const char* dir,
                  const char* vert_path,
                  const char* frag_path,
                  SDL_Window* window,
                  SDL_GPUDevice* device);
  ~Skybox();

  bool IsLoaded() const { return loaded_; }
  void Draw(SDL_GPURenderPass* pass) const;

public:
  const char* VertPath = "resources/shaders/compiled/skybox.vert.spv";
  const char* FragPath = "resources/shaders/compiled/skybox.frag.spv";
  SDL_GPUTexture* faces[6]{};
  SDL_GPUTexture* Cubemap{};
  SDL_GPUSampler* CubemapSampler{};
  SDL_GPUBuffer* VertexBuffer{};
  SDL_GPUBuffer* IndexBuffer{};
  SDL_GPUGraphicsPipeline* Pipeline{ nullptr };

private:
  bool Init();
  bool CreatePipeline();
  bool SendVertexData() const;
  bool LoadTextures();

private:
  const char* dir_{};
  SDL_GPUDevice* device_{}; // needed for dtor
  SDL_Window* window_{};    // needed swapchain format
  bool loaded_{ false };
  const char* paths[6]{ "left.jpg",   "right.jpg", "top.jpg",
                        "bottom.jpg", "back.jpg",  "front.jpg" };

  // Vertex data
  // clang-format off
  static constexpr Uint8 VERT_COUNT = 24;
  static constexpr PosVertex verts_uvs[VERT_COUNT] = {
		{ {-1.f, -1.f, 1.f} },
		{ { 1.f, -1.f, 1.f} },
		{ { 1.f,  1.f, 1.f} },
		{ {-1.f,  1.f, 1.f} },

		{ { 1.f, -1.f, -1.f} },
		{ {-1.f, -1.f, -1.f} },
		{ {-1.f,  1.f, -1.f} },
		{ { 1.f,  1.f, -1.f} },

		{ {-1.f, -1.f, -1.f} },
		{ {-1.f, -1.f,  1.f} },
    { {-1.f,  1.f,  1.f} },
		{ {-1.f,  1.f, -1.f} },

    { {1.f, -1.f, -1.f} },
    { {1.f, -1.f,  1.f} },
    { {1.f,  1.f,  1.f} },
    { {1.f,  1.f, -1.f} },

    { {-1.f, 1.f, -1.f} },
    { { 1.f, 1.f, -1.f} },
    { { 1.f, 1.f,  1.f} },
    { {-1.f, 1.f,  1.f} },

    { { 1.f, -1.f, -1.f} },
    { {-1.f, -1.f, -1.f} },
    { {-1.f, -1.f,  1.f} },
    { { 1.f, -1.f,  1.f} },
  };

  static constexpr Uint8 INDEX_COUNT = 36;
  static constexpr Uint16 indices[INDEX_COUNT] = {
    0,  1,  2,  0,  2,  3,
    4,  5,  6,  4,  6,  7,
    8,  9,  10, 8,  10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23
  };
  // clang-format on
};
