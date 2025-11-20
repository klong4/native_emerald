#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int warning_count = 0;
static const int MAX_WARNINGS = 10;

void mem_init(Memory *mem) {
    if (!mem) return;
    
    mem->rom = NULL;
    mem->rom_size = 0;
    
    memset(mem->ewram, 0, EWRAM_SIZE);
    memset(mem->iwram, 0, IWRAM_SIZE);
    memset(mem->vram, 0, VRAM_SIZE);
    memset(mem->oam, 0, OAM_SIZE);
    memset(mem->palette, 0, PALETTE_SIZE);
    memset(mem->io_regs, 0, IO_SIZE);
}

void mem_cleanup(Memory *mem) {
    // ROM is owned by caller, don't free it here
    mem->rom = NULL;
    mem->rom_size = 0;
}

void mem_set_rom(Memory *mem, u8 *rom, u32 size) {
    mem->rom = rom;
    mem->rom_size = size;
}

u8 mem_read8(Memory *mem, u32 addr) {
    // EWRAM: 0x02000000 - 0x0203FFFF (256KB)
    if (addr >= ADDR_EWRAM_START && addr < ADDR_EWRAM_START + EWRAM_SIZE) {
        return mem->ewram[addr - ADDR_EWRAM_START];
    }
    
    // IWRAM: 0x03000000 - 0x03007FFF (32KB)
    if (addr >= ADDR_IWRAM_START && addr < ADDR_IWRAM_START + IWRAM_SIZE) {
        return mem->iwram[addr - ADDR_IWRAM_START];
    }
    
    // I/O Registers: 0x04000000 - 0x040003FF (1KB)
    if (addr >= ADDR_IO_START && addr < ADDR_IO_START + IO_SIZE) {
        return mem->io_regs[addr - ADDR_IO_START];
    }
    
    // Palette RAM: 0x05000000 - 0x050003FF (1KB)
    if (addr >= ADDR_PALETTE_START && addr < ADDR_PALETTE_START + PALETTE_SIZE) {
        return mem->palette[addr - ADDR_PALETTE_START];
    }
    
    // VRAM: 0x06000000 - 0x06017FFF (96KB)
    if (addr >= ADDR_VRAM_START && addr < ADDR_VRAM_START + VRAM_SIZE) {
        return mem->vram[addr - ADDR_VRAM_START];
    }
    
    // OAM: 0x07000000 - 0x070003FF (1KB)
    if (addr >= ADDR_OAM_START && addr < ADDR_OAM_START + OAM_SIZE) {
        return mem->oam[addr - ADDR_OAM_START];
    }
    
    // ROM: 0x08000000 - 0x09FFFFFF (max 32MB)
    // Also mirrored at 0x0A000000 and 0x0C000000
    if (addr >= ADDR_ROM_START && addr < ADDR_ROM_START + ROM_SIZE) {
        u32 offset = (addr - ADDR_ROM_START) % mem->rom_size;
        return mem->rom[offset];
    }
    
    // BIOS: 0x00000000 - 0x00003FFF
    // For now, just return 0 (we'll stub BIOS calls)
    if (addr < 0x00004000) {
        return 0;
    }
    
    // Unmapped memory
    if (warning_count < MAX_WARNINGS) {
        fprintf(stderr, "Warning: Read from unmapped address 0x%08X\n", addr);
        warning_count++;
        if (warning_count == MAX_WARNINGS) {
            fprintf(stderr, "(Suppressing further memory warnings...)\n");
        }
    }
    return 0;
}

u16 mem_read16(Memory *mem, u32 addr) {
    // GBA is little-endian
    u8 low = mem_read8(mem, addr);
    u8 high = mem_read8(mem, addr + 1);
    return (u16)(low | (high << 8));
}

u32 mem_read32(Memory *mem, u32 addr) {
    u16 low = mem_read16(mem, addr);
    u16 high = mem_read16(mem, addr + 2);
    return (u32)(low | (high << 16));
}

void mem_write8(Memory *mem, u32 addr, u8 value) {
    // EWRAM: 0x02000000 - 0x0203FFFF
    if (addr >= ADDR_EWRAM_START && addr < ADDR_EWRAM_START + EWRAM_SIZE) {
        mem->ewram[addr - ADDR_EWRAM_START] = value;
        return;
    }
    
    // IWRAM: 0x03000000 - 0x03007FFF
    if (addr >= ADDR_IWRAM_START && addr < ADDR_IWRAM_START + IWRAM_SIZE) {
        mem->iwram[addr - ADDR_IWRAM_START] = value;
        return;
    }
    
    // I/O Registers: 0x04000000 - 0x040003FF
    if (addr >= ADDR_IO_START && addr < ADDR_IO_START + IO_SIZE) {
        mem->io_regs[addr - ADDR_IO_START] = value;
        return;
    }
    
    // Palette RAM: 0x05000000 - 0x050003FF
    if (addr >= ADDR_PALETTE_START && addr < ADDR_PALETTE_START + PALETTE_SIZE) {
        mem->palette[addr - ADDR_PALETTE_START] = value;
        return;
    }
    
    // VRAM: 0x06000000 - 0x06017FFF
    if (addr >= ADDR_VRAM_START && addr < ADDR_VRAM_START + VRAM_SIZE) {
        mem->vram[addr - ADDR_VRAM_START] = value;
        return;
    }
    
    // OAM: 0x07000000 - 0x070003FF
    if (addr >= ADDR_OAM_START && addr < ADDR_OAM_START + OAM_SIZE) {
        mem->oam[addr - ADDR_OAM_START] = value;
        return;
    }
    
    // ROM is read-only
    if (addr >= ADDR_ROM_START && addr < ADDR_ROM_START + ROM_SIZE) {
        fprintf(stderr, "Warning: Attempted write to ROM at 0x%08X\n", addr);
        return;
    }
    
    fprintf(stderr, "Warning: Write to unmapped address 0x%08X = 0x%02X\n", addr, value);
}

void mem_write16(Memory *mem, u32 addr, u16 value) {
    mem_write8(mem, addr, (u8)(value & 0xFF));
    mem_write8(mem, addr + 1, (u8)((value >> 8) & 0xFF));
}

void mem_write32(Memory *mem, u32 addr, u32 value) {
    mem_write16(mem, addr, (u16)(value & 0xFFFF));
    mem_write16(mem, addr + 2, (u16)((value >> 16) & 0xFFFF));
}

u8 mem_get_ai_input(Memory *mem) {
    // gAiInputState is at 0x0203cf64
    // Offset from EWRAM base: 0x3cf64
    return mem->ewram[0x3cf64];
}

void mem_set_ai_input(Memory *mem, u8 value) {
    mem->ewram[0x3cf64] = value;
}
