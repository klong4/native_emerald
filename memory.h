#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

typedef struct {
    u8 *rom;              // ROM data (loaded from file)
    u32 rom_size;         // Actual ROM size
    u8 ewram[EWRAM_SIZE]; // External WRAM (256KB)
    u8 iwram[IWRAM_SIZE]; // Internal WRAM (32KB)
    u8 vram[VRAM_SIZE];   // Video RAM (96KB)
    u8 oam[OAM_SIZE];     // Object Attribute Memory (1KB)
    u8 palette[PALETTE_SIZE]; // Palette RAM (1KB)
    u8 io_regs[IO_SIZE];  // I/O Registers (1KB)
} Memory;

// Initialize memory subsystem
void mem_init(Memory *mem);
void mem_cleanup(Memory *mem);
void mem_set_rom(Memory *mem, u8 *rom, u32 size);

// Memory access functions
u32 mem_read32(Memory *mem, u32 addr);
u16 mem_read16(Memory *mem, u32 addr);
u8  mem_read8(Memory *mem, u32 addr);

void mem_write32(Memory *mem, u32 addr, u32 value);
void mem_write16(Memory *mem, u32 addr, u16 value);
void mem_write8(Memory *mem, u32 addr, u8 value);

// Helper to get AI input state
u8 mem_get_ai_input(Memory *mem);
void mem_set_ai_input(Memory *mem, u8 value);

#endif // MEMORY_H
