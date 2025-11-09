#pragma once

// Wether image pixel data represents a 2D texture or a cubemap
enum class ImageType : u8
{
  DIMENSIONS_2D,
  DIMENSIONS_3D
};

enum class ImagePixelFormat : u8
{
  PIXELFORMAT_UINT,
  PIXELFORMAT_FLOAT,
};

struct LoadedImage
{
  // NOTE: Setting req_comp to 4 will force padding of missing alpha in case of
  // 3 channels. Forced it to 4 because there's no SDL equivalent to
  // VK_FORMAT_R8G8B8_UNORM as support is meh accross drivers it seems.
  i32 nrChannels;
  i32 w, h;
  void* data{ nullptr };
  ImageType image_type;
  ImagePixelFormat pixel_format;

  ~LoadedImage();

  u64 DataSize();
  u8 BytesPerPixel();
};

class ImageLoader
{
public:
  static bool Load(
    LoadedImage& out,
    std::filesystem::path path,
    ImageType img_type = ImageType::DIMENSIONS_2D,
    ImagePixelFormat pixel_fmt = ImagePixelFormat::PIXELFORMAT_UINT);

  static bool Load(
    LoadedImage& out,
    const u8* data,
    i32 sz,
    ImageType img_type = ImageType::DIMENSIONS_2D,
    ImagePixelFormat pixel_fmt = ImagePixelFormat::PIXELFORMAT_UINT);
};
