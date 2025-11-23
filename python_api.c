#include "python_api.h"
#include "types.h"
#include "rom_loader.h"
#include "memory.h"
#include "cpu_core.h"
#include "gfx_renderer.h"
#include "input.h"
#include "interrupts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal emulator state (not exposed to Python)
typedef struct {
    ARM7TDMI cpu;
    Memory memory;
    GFXState gfx;
    InputState input;
    InterruptState interrupts;
    u8 *rom_data;
    u32 rom_size;
    u64 frame_count;
} EmulatorState;

EmuHandle emu_init(const char *rom_path) {
    if (!rom_path) return NULL;
    
    // Allocate emulator state
    EmulatorState *emu = (EmulatorState*)malloc(sizeof(EmulatorState));
    if (!emu) return NULL;
    
    memset(emu, 0, sizeof(EmulatorState));
    
    // Load ROM
    if (!load_rom(rom_path, &emu->rom_data, &emu->rom_size)) {
        free(emu);
        return NULL;
    }
    
    // Initialize subsystems
    cpu_init(&emu->cpu);
    mem_init(&emu->memory);
    mem_set_rom(&emu->memory, emu->rom_data, emu->rom_size);
    interrupt_init(&emu->interrupts);
    mem_set_interrupts(&emu->memory, &emu->interrupts);
    gfx_init(&emu->gfx);
    input_init(&emu->input);
    
    cpu_reset(&emu->cpu);
    
    emu->frame_count = 0;
    
    printf("Python API: Emulator initialized (ROM: %u bytes)\n", emu->rom_size);
    
    return (EmuHandle)emu;
}

void emu_step(EmuHandle handle, u8 buttons) {
    if (!handle) return;
    
    EmulatorState *emu = (EmulatorState*)handle;
    
    // Set button input
    mem_set_ai_input(&emu->memory, buttons);
    input_update(&emu->input, &emu->memory);
    
    // Clear VBlank at start of frame (back to scanline 0)
    interrupt_update_vcount(&emu->interrupts, 0);
    
    // Execute one frame (game code runs)
    cpu_execute_frame(&emu->cpu, &emu->memory, &emu->interrupts);
    
    // Trigger VBlank interrupt at END of frame (scanline 160)
    // This will be processed at the START of next frame's execution
    interrupt_update_vcount(&emu->interrupts, 160);
    
    // Render graphics
    gfx_render_frame(&emu->gfx, &emu->memory);
    
    emu->frame_count++;
}

void emu_get_screen(EmuHandle handle, u8 *buffer) {
    if (!handle || !buffer) return;
    
    EmulatorState *emu = (EmulatorState*)handle;
    
    // Convert RGB565 framebuffer to RGB888
    u16 *fb = emu->gfx.framebuffer;
    
    for (int y = 0; y < GBA_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
            u16 color = fb[y * GBA_SCREEN_WIDTH + x];
            
            // RGB565 -> RGB888
            u8 r = ((color >> 11) & 0x1F) << 3;
            u8 g = ((color >> 5) & 0x3F) << 2;
            u8 b = (color & 0x1F) << 3;
            
            int idx = (y * GBA_SCREEN_WIDTH + x) * 3;
            buffer[idx + 0] = r;
            buffer[idx + 1] = g;
            buffer[idx + 2] = b;
        }
    }
}

void emu_reset(EmuHandle handle) {
    if (!handle) return;
    
    EmulatorState *emu = (EmulatorState*)handle;
    
    // Reset CPU
    cpu_reset(&emu->cpu);
    
    // Clear memory (except ROM)
    memset(emu->memory.ewram, 0, EWRAM_SIZE);
    memset(emu->memory.iwram, 0, IWRAM_SIZE);
    memset(emu->memory.io_regs, 0, IO_SIZE);
    memset(emu->memory.palette, 0, PALETTE_SIZE);
    memset(emu->memory.vram, 0, VRAM_SIZE);
    memset(emu->memory.oam, 0, OAM_SIZE);
    
    // Reset interrupts
    interrupt_init(&emu->interrupts);
    
    // Reset graphics
    memset(emu->gfx.framebuffer, 0, sizeof(emu->gfx.framebuffer));
    
    emu->frame_count = 0;
    
    printf("Python API: Emulator reset\n");
}

void emu_cleanup(EmuHandle handle) {
    if (!handle) return;
    
    EmulatorState *emu = (EmulatorState*)handle;
    
    // Free ROM data
    if (emu->rom_data) {
        free(emu->rom_data);
    }
    
    // Cleanup memory system
    mem_cleanup(&emu->memory);
    
    // Free emulator state
    free(emu);
    
    printf("Python API: Emulator cleaned up\n");
}

u8 emu_read_memory(EmuHandle handle, u32 addr) {
    if (!handle) return 0;
    
    EmulatorState *emu = (EmulatorState*)handle;
    return mem_read8(&emu->memory, addr);
}

void emu_write_memory(EmuHandle handle, u32 addr, u8 value) {
    if (!handle) return;
    
    EmulatorState *emu = (EmulatorState*)handle;
    mem_write8(&emu->memory, addr, value);
}

u32 emu_get_frame_count(EmuHandle handle) {
    if (!handle) return 0;
    
    EmulatorState *emu = (EmulatorState*)handle;
    return (u32)emu->frame_count;
}

u32 emu_get_cpu_cycles(EmuHandle handle) {
    if (!handle) return 0;
    
    EmulatorState *emu = (EmulatorState*)handle;
    return (u32)emu->cpu.cycles;
}

void emu_save_state(EmuHandle handle, const char *filename) {
    if (!handle || !filename) return;
    
    // TODO: Implement save state functionality
    printf("Python API: Save state not yet implemented\n");
}

void emu_load_state(EmuHandle handle, const char *filename) {
    if (!handle || !filename) return;
    
    // TODO: Implement load state functionality
    printf("Python API: Load state not yet implemented\n");
}
