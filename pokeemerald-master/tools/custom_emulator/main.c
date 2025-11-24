#include <stdio.h>
#include <SDL.h>
#include "types.h"
#include "rom_loader.h"
#include "memory.h"
#include "cpu_core.h"
#include "gfx_renderer.h"
#include "input.h"
#include "stubs.h"

typedef struct {
    ARM7TDMI cpu;
    Memory memory;
    GFXState gfx;
    InputState input;
    u64 frame_count;
    bool running;
} EmulatorState;

static void emu_init(EmulatorState *emu, u8 *rom, u32 rom_size) {
    cpu_init(&emu->cpu);
    mem_init(&emu->memory);
    mem_set_rom(&emu->memory, rom, rom_size);
    gfx_init(&emu->gfx);
    input_init(&emu->input);
    
    // Initialize audio
    AudioState audio;
    audio_init(&audio);
    
    cpu_reset(&emu->cpu);
    
    emu->frame_count = 0;
    emu->running = true;
    
    printf("Emulator initialized!\n");
    printf("Entry point: 0x%08X\n", emu->cpu.r[15]);
    printf("CPU Mode: %s\n", emu->cpu.thumb_mode ? "Thumb" : "ARM");
    printf("First instruction: 0x%08X\n", mem_read32(&emu->memory, emu->cpu.r[15]));
}

static void emu_frame(EmulatorState *emu) {
    // Update input from AI or keyboard
    input_update(&emu->input, &emu->memory);
    
    // Execute one frame worth of CPU instructions
    cpu_execute_frame(&emu->cpu, &emu->memory);
    
    // Render graphics
    gfx_render_frame(&emu->gfx, &emu->memory);
    
    emu->frame_count++;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom_file.gba>\n", argv[0]);
        fprintf(stderr, "Example: %s ../../pokeemerald.gba\n", argv[0]);
        return 1;
    }
    
    const char *rom_path = argv[1];
    
    // Load ROM
    u8 *rom_data = NULL;
    u32 rom_size = 0;
    
    if (!load_rom(rom_path, &rom_data, &rom_size)) {
        return 1;
    }
    
    // Verify ROM header
    if (!verify_rom_header(rom_data)) {
        fprintf(stderr, "Warning: ROM verification failed, continuing anyway...\n");
    }
    
    // Parse and display ROM info
    ROMInfo rom_info;
    parse_rom_header(rom_data, &rom_info);
    print_rom_info(&rom_info);
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        free(rom_data);
        return 1;
    }
    
    // Create window (2x scale)
    SDL_Window *window = SDL_CreateWindow(
        "Pokemon Emerald Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        GBA_SCREEN_WIDTH * 2,
        GBA_SCREEN_HEIGHT * 2,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        free(rom_data);
        return 1;
    }
    
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(rom_data);
        return 1;
    }
    
    // Initialize emulator
    EmulatorState emu;
    emu_init(&emu, rom_data, rom_size);
    
    printf("\nEmulator running! Press ESC to quit.\n");
    printf("CPU: ARM7TDMI interpreter active\n");
    printf("Keyboard: Z=A, X=B, Arrows=D-Pad, Enter=Start\n\n");
    
    // Main loop
    SDL_Event event;
    u32 last_time = SDL_GetTicks();
    
    while (emu.running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                emu.running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    emu.running = false;
                }
                
                // Map keyboard to GBA buttons (for testing)
                u8 ai_input = 0;
                if (event.key.keysym.sym == SDLK_z) ai_input |= KEY_A;
                if (event.key.keysym.sym == SDLK_x) ai_input |= KEY_B;
                if (event.key.keysym.sym == SDLK_UP) ai_input |= KEY_UP;
                if (event.key.keysym.sym == SDLK_DOWN) ai_input |= KEY_DOWN;
                if (event.key.keysym.sym == SDLK_LEFT) ai_input |= KEY_LEFT;
                if (event.key.keysym.sym == SDLK_RIGHT) ai_input |= KEY_RIGHT;
                if (event.key.keysym.sym == SDLK_RETURN) ai_input |= KEY_START;
                if (event.key.keysym.sym == SDLK_RSHIFT) ai_input |= KEY_SELECT;
                
                mem_set_ai_input(&emu.memory, ai_input);
            }
        }
        
        // Run one frame
        emu_frame(&emu);
        
        // Render
        gfx_present(&emu.gfx, renderer);
        
        // Frame timing (60 FPS)
        u32 current_time = SDL_GetTicks();
        u32 elapsed = current_time - last_time;
        if (elapsed < 16) {
            SDL_Delay(16 - elapsed);
        }
        last_time = SDL_GetTicks();
        
        // Print status every 60 frames
        if (emu.frame_count % 60 == 0) {
            printf("Frame %llu, AI Input: 0x%02X\n",
                   (unsigned long long)emu.frame_count,
                   mem_get_ai_input(&emu.memory));
        }
    }
    
    // Cleanup
    printf("\nEmulator shutting down...\n");
    printf("Total frames rendered: %llu\n", (unsigned long long)emu.frame_count);
    
    audio_cleanup();
    mem_cleanup(&emu.memory);
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    free(rom_data);
    
    return 0;
}
