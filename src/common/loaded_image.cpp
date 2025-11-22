#include "loaded_image.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb/stb_image.h>

LoadedImage::~LoadedImage()
{
  if (data != nullptr) {
    stbi_image_free(data);
  }
}

u8
LoadedImage::BytesPerPixel()
{
  // Always assume 4 channels
  if (pixel_format == ImagePixelFormat::PIXELFORMAT_UINT) {
    // return 1 * nrChannels;
    return 1 * 4;
  }
  // return 4 * nrChannels;
  return 4 * 4;
}

u64
LoadedImage::DataSize()
{
  return BytesPerPixel() * w * h;
}

bool
ImageLoader::Load(LoadedImage& out,
                  std::filesystem::path path,
                  ImageType img_type,
                  ImagePixelFormat pixel_fmt)
{
  out.pixel_format = pixel_fmt;
  out.image_type = img_type;
  switch (pixel_fmt) {
    case ImagePixelFormat::PIXELFORMAT_UINT:
      out.data = stbi_load(path.c_str(), &out.w, &out.h, &out.nrChannels, 4);
      break;
    case ImagePixelFormat::PIXELFORMAT_FLOAT:
      out.data = stbi_loadf(path.c_str(), &out.w, &out.h, &out.nrChannels, 4);
      break;
  }

  return out.data != nullptr;
}

bool
ImageLoader::Load(LoadedImage& out,
                  const u8* data,
                  i32 sz,
                  ImageType img_type,
                  ImagePixelFormat pixel_fmt)
{
  out.pixel_format = pixel_fmt;
  out.image_type = img_type;
  switch (pixel_fmt) {
    case ImagePixelFormat::PIXELFORMAT_UINT:
      out.data = stbi_load_from_memory(
        (stbi_uc*)data, sz, &out.w, &out.h, &out.nrChannels, 4);
      break;
    case ImagePixelFormat::PIXELFORMAT_FLOAT:
      out.data = stbi_loadf_from_memory(
        (stbi_uc*)data, sz, &out.w, &out.h, &out.nrChannels, 4);
      break;
  }

  return out.data != nullptr;
}
