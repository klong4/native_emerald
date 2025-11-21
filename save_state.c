#include "save_state.h"
#include "types.h"
#include "cpu_core.h"
#include "memory.h"
#include "gfx_renderer.h"
#include "input.h"
#include "interrupts.h"
#include "timer.h"
#include "dma.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SAVE_STATE_MAGIC 0x454D4552  // "EMER"
#define SAVE_STATE_VERSION 1

typedef struct {
    u32 magic;
    u32 version;
    u64 frame_count;
    ARM7TDMI cpu;
    u8 ewram[EWRAM_SIZE];
    u8 iwram[IWRAM_SIZE];
    u8 io_regs[IO_SIZE];
    u8 palette[PALETTE_SIZE];
    u8 vram[VRAM_SIZE];
    u8 oam[OAM_SIZE];
    InterruptState interrupts;
    TimerState timers;
    DMAState dma;
    // Note: ROM is not saved, it's loaded separately
} SaveStateData;

// Forward declare EmulatorState structure
struct EmulatorState {
    ARM7TDMI cpu;
    Memory memory;
    GFXState gfx;
    InputState input;
    InterruptState interrupts;
    TimerState timers;
    DMAState dma;
    u8 *rom_data;
    u32 rom_size;
    u64 frame_count;
};

u32 get_save_state_size(void) {
    return sizeof(SaveStateData);
}

bool save_state_to_buffer(struct EmulatorState *emu, u8 *buffer, u32 *size) {
    if (!emu || !buffer || !size) return false;
    
    if (*size < sizeof(SaveStateData)) {
        *size = sizeof(SaveStateData);
        return false;
    }
    
    SaveStateData *save = (SaveStateData*)buffer;
    
    save->magic = SAVE_STATE_MAGIC;
    save->version = SAVE_STATE_VERSION;
    save->frame_count = emu->frame_count;
    save->cpu = emu->cpu;
    
    memcpy(save->ewram, emu->memory.ewram, EWRAM_SIZE);
    memcpy(save->iwram, emu->memory.iwram, IWRAM_SIZE);
    memcpy(save->io_regs, emu->memory.io_regs, IO_SIZE);
    memcpy(save->palette, emu->memory.palette, PALETTE_SIZE);
    memcpy(save->vram, emu->memory.vram, VRAM_SIZE);
    memcpy(save->oam, emu->memory.oam, OAM_SIZE);
    
    save->interrupts = emu->interrupts;
    save->timers = emu->timers;
    save->dma = emu->dma;
    
    *size = sizeof(SaveStateData);
    return true;
}

bool load_state_from_buffer(struct EmulatorState *emu, const u8 *buffer, u32 size) {
    if (!emu || !buffer || size < sizeof(SaveStateData)) return false;
    
    const SaveStateData *save = (const SaveStateData*)buffer;
    
    if (save->magic != SAVE_STATE_MAGIC) {
        fprintf(stderr, "Invalid save state magic\n");
        return false;
    }
    
    if (save->version != SAVE_STATE_VERSION) {
        fprintf(stderr, "Incompatible save state version\n");
        return false;
    }
    
    emu->frame_count = save->frame_count;
    emu->cpu = save->cpu;
    
    memcpy(emu->memory.ewram, save->ewram, EWRAM_SIZE);
    memcpy(emu->memory.iwram, save->iwram, IWRAM_SIZE);
    memcpy(emu->memory.io_regs, save->io_regs, IO_SIZE);
    memcpy(emu->memory.palette, save->palette, PALETTE_SIZE);
    memcpy(emu->memory.vram, save->vram, VRAM_SIZE);
    memcpy(emu->memory.oam, save->oam, OAM_SIZE);
    
    emu->interrupts = save->interrupts;
    emu->timers = save->timers;
    emu->dma = save->dma;
    
    return true;
}

bool save_state_to_file(struct EmulatorState *emu, const char *filename) {
    if (!emu || !filename) return false;
    
    u32 size = sizeof(SaveStateData);
    u8 *buffer = (u8*)malloc(size);
    if (!buffer) return false;
    
    if (!save_state_to_buffer(emu, buffer, &size)) {
        free(buffer);
        return false;
    }
    
    FILE *f = fopen(filename, "wb");
    if (!f) {
        free(buffer);
        return false;
    }
    
    size_t written = fwrite(buffer, 1, size, f);
    fclose(f);
    free(buffer);
    
    return written == size;
}

bool load_state_from_file(struct EmulatorState *emu, const char *filename) {
    if (!emu || !filename) return false;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < (long)sizeof(SaveStateData)) {
        fclose(f);
        return false;
    }
    
    u8 *buffer = (u8*)malloc(file_size);
    if (!buffer) {
        fclose(f);
        return false;
    }
    
    size_t read = fread(buffer, 1, file_size, f);
    fclose(f);
    
    if (read != (size_t)file_size) {
        free(buffer);
        return false;
    }
    
    bool result = load_state_from_buffer(emu, buffer, file_size);
    free(buffer);
    
    return result;
}
