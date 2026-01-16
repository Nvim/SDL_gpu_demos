#ifndef POST_PROCESS_FLAGS_H
#define POST_PROCESS_FLAGS_H

// clang-format off
#define USE_GAMMA_CORRECT          (0x01 << 0x00)
#define TONEMAP_NONE               (0x01 << 0x01)
#define TONEMAP_REINHARD           (0x01 << 0x02)
#define TONEMAP_REINHARD_EXTENDED  (0x01 << 0x03)
#define TONEMAP_ACES               (0x01 << 0x04)
#define TONEMAP_HABLE              (0x01 << 0x05)
#define TONEMAP_FILMIC             (0x01 << 0x06)

#endif // !POST_PROCESS_FLAGS_H
