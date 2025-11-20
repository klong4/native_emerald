#include "memory.h"
#include "interrupts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int warning_count = 0;
static const int MAX_WARNINGS = 10;

void mem_init(Memory *mem) {
    if (!mem) return;
    
    mem->rom = NULL;
    mem->rom_size = 0;
    mem->interrupts = NULL;
    
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

void mem_set_interrupts(Memory *mem, InterruptState *interrupts) {
    mem->interrupts = interrupts;
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
        u32 offset = addr - ADDR_IO_START;
        
        // Return live interrupt register values
        if (mem->interrupts) {
            if (offset == REG_IE || offset == REG_IE + 1) {
                return (offset == REG_IE) ? (mem->interrupts->ie & 0xFF) : ((mem->interrupts->ie >> 8) & 0xFF);
            }
            if (offset == REG_IF || offset == REG_IF + 1) {
                return (offset == REG_IF) ? (mem->interrupts->if_flag & 0xFF) : ((mem->interrupts->if_flag >> 8) & 0xFF);
            }
            if (offset == REG_IME || offset == REG_IME + 1) {
                return (offset == REG_IME) ? (mem->interrupts->ime & 0xFF) : ((mem->interrupts->ime >> 8) & 0xFF);
            }
            if (offset == 0x04 || offset == 0x05) {
                return (offset == 0x04) ? (mem->interrupts->dispstat & 0xFF) : ((mem->interrupts->dispstat >> 8) & 0xFF);
            }
            if (offset == 0x06 || offset == 0x07) {
                return (offset == 0x06) ? (mem->interrupts->vcount & 0xFF) : ((mem->interrupts->vcount >> 8) & 0xFF);
            }
        }
        
        return mem->io_regs[offset];
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
        u32 offset = addr - ADDR_IO_START;
        
        // Handle interrupt registers specially
        if (mem->interrupts) {
            if (offset == REG_IE || offset == REG_IE + 1) {
                // IE register write
                u16 ie = mem_read16(mem, ADDR_IO_START + REG_IE);
                if (offset == REG_IE) {
                    ie = (ie & 0xFF00) | value;
                } else {
                    ie = (ie & 0x00FF) | (value << 8);
                }
                mem->interrupts->ie = ie;
                mem->io_regs[REG_IE] = ie & 0xFF;
                mem->io_regs[REG_IE + 1] = (ie >> 8) & 0xFF;
                return;
            }
            if (offset == REG_IF || offset == REG_IF + 1) {
                // IF register write (acknowledge interrupts)
                u16 if_val = mem_read16(mem, ADDR_IO_START + REG_IF);
                if (offset == REG_IF) {
                    interrupt_acknowledge(mem->interrupts, value);
                } else {
                    interrupt_acknowledge(mem->interrupts, value << 8);
                }
                mem->io_regs[REG_IF] = mem->interrupts->if_flag & 0xFF;
                mem->io_regs[REG_IF + 1] = (mem->interrupts->if_flag >> 8) & 0xFF;
                return;
            }
            if (offset == REG_IME || offset == REG_IME + 1) {
                // IME register write
                u16 ime = mem_read16(mem, ADDR_IO_START + REG_IME);
                if (offset == REG_IME) {
                    ime = (ime & 0xFF00) | value;
                } else {
                    ime = (ime & 0x00FF) | (value << 8);
                }
                mem->interrupts->ime = ime;
                mem->io_regs[REG_IME] = ime & 0xFF;
                mem->io_regs[REG_IME + 1] = (ime >> 8) & 0xFF;
                return;
            }
            if (offset == 0x04 || offset == 0x05) {
                // DISPSTAT register
                u16 dispstat = mem_read16(mem, ADDR_IO_START + 0x04);
                if (offset == 0x04) {
                    dispstat = (dispstat & 0xFF00) | value;
                } else {
                    dispstat = (dispstat & 0x00FF) | (value << 8);
                }
                mem->interrupts->dispstat = dispstat;
                mem->io_regs[0x04] = dispstat & 0xFF;
                mem->io_regs[0x05] = (dispstat >> 8) & 0xFF;
                return;
            }
        }
        
        mem->io_regs[offset] = value;
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
