// Simple test to verify display rendering works
// This creates a minimal GBA program that sets DISPCNT and draws colors

#include "emulator.h"
#include <stdio.h>
#include <string.h>

void test_display_init(Emulator *emu) {
    printf("Testing display initialization...\n");
    
    // Manually set DISPCNT to mode 3 (240x160, 16-bit bitmap)
    emu->memory.io_regs[0] = 0x03;  // MODE 3
    emu->memory.io_regs[1] = 0x04;  // BG2 enabled
    
    printf("DISPCNT set to: 0x%04X\n", 
           emu->memory.io_regs[0] | (emu->memory.io_regs[1] << 8));
    
    // Fill VRAM with a test pattern
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x++) {
            int offset = (y * 240 + x) * 2;
            if (offset < VRAM_SIZE) {
                // Create RGB gradient
                u16 color = ((x / 8) << 10) | ((y / 5) << 5) | ((x + y) / 10);
                emu->memory.vram[offset] = color & 0xFF;
                emu->memory.vram[offset + 1] = (color >> 8) & 0xFF;
            }
        }
    }
    
    printf("VRAM filled with test pattern\n");
    printf("First few VRAM bytes: %02X %02X %02X %02X\n",
           emu->memory.vram[0], emu->memory.vram[1],
           emu->memory.vram[2], emu->memory.vram[3]);
}
