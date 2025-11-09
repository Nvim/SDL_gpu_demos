#pragma once

namespace UnitCube {
// clang-format off
static constexpr u8 VertCount = 24;
static constexpr PosVertex Verts[VertCount] = {
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

static constexpr u16 IndexCount = 36;
static constexpr u16 Indices[IndexCount] = {
  0,  1,  2,  0,  2,  3,
  4,  5,  6,  4,  6,  7,
  8,  9,  10, 8,  10, 11,
  12, 13, 14, 12, 14, 15,
  16, 17, 18, 16, 18, 19,
  20, 21, 22, 20, 22, 23
};
// clang-format on

}
