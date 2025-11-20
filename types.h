#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Standard GBA types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

// GBA Constants
#define GBA_SCREEN_WIDTH  240
#define GBA_SCREEN_HEIGHT 160
#define GBA_FRAMEBUFFER_SIZE (GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT)

// Memory sizes
#define ROM_SIZE      (32 * 1024 * 1024)  // 32 MB max
#define EWRAM_SIZE    (256 * 1024)        // 256 KB
#define IWRAM_SIZE    (32 * 1024)         // 32 KB
#define VRAM_SIZE     (96 * 1024)         // 96 KB
#define OAM_SIZE      1024                // 1 KB
#define PALETTE_SIZE  1024                // 1 KB
#define IO_SIZE       1024                // 1 KB

// Memory map addresses
#define ADDR_EWRAM_START   0x02000000
#define ADDR_IWRAM_START   0x03000000
#define ADDR_IO_START      0x04000000
#define ADDR_PALETTE_START 0x05000000
#define ADDR_VRAM_START    0x06000000
#define ADDR_OAM_START     0x07000000
#define ADDR_ROM_START     0x08000000

// AI Input Hook
#define AI_INPUT_ADDRESS   0x0203cf64  // gAiInputState in EWRAM

// GBA Key bits
#define KEY_A       (1 << 0)
#define KEY_B       (1 << 1)
#define KEY_SELECT  (1 << 2)
#define KEY_START   (1 << 3)
#define KEY_RIGHT   (1 << 4)
#define KEY_LEFT    (1 << 5)
#define KEY_UP      (1 << 6)
#define KEY_DOWN    (1 << 7)
#define KEY_R       (1 << 8)
#define KEY_L       (1 << 9)

#endif // TYPES_H
