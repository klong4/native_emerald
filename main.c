#include <stdio.h>
#include <SDL.h>
#include "types.h"
#include "rom_loader.h"
#include "memory.h"
#include "cpu_core.h"
#include "gfx_renderer.h"
#include "input.h"
#include "stubs.h"
#include "interrupts.h"
#include "debug_trace.h"
#include "timer.h"
#include "dma.h"
#include "rtc.h"

typedef struct {
    ARM7TDMI cpu;
    Memory memory;
    GFXState gfx;
    InputState input;
    InterruptState interrupts;
    TimerState timers;
    DMAState dma;
    RTCState rtc;
    u64 frame_count;
    bool running;
    u32 vram_writes;
    u32 oam_writes;
    u32 interrupts_fired;
} EmulatorState;

static void emu_init(EmulatorState *emu, u8 *rom, u32 rom_size) {
    printf("[INIT] Starting initialization...\n");
    fflush(stdout);
    
    printf("[INIT] Initializing CPU...\n");
    cpu_init(&emu->cpu);
    
    printf("[INIT] Initializing memory...\n");
    mem_init(&emu->memory);
    
    printf("[INIT] Setting ROM...\n");
    mem_set_rom(&emu->memory, rom, rom_size);
    
    printf("[INIT] Initializing interrupts...\n");
    interrupt_init(&emu->interrupts);
    mem_set_interrupts(&emu->memory, &emu->interrupts);
    
    printf("[INIT] Initializing timers...\n");
    timer_init(&emu->timers);
    
    printf("[INIT] Initializing DMA...\n");
    dma_init(&emu->dma);
    
    printf("[INIT] Initializing RTC...\n");
    rtc_init(&emu->rtc);
    
    printf("[INIT] Setting subsystems...\n");
    mem_set_timers(&emu->memory, &emu->timers);
    mem_set_dma(&emu->memory, &emu->dma);
    mem_set_rtc(&emu->memory, &emu->rtc);
    
    printf("[INIT] Initializing graphics...\n");
    gfx_init(&emu->gfx);
    
    printf("[INIT] Initializing input...\n");
    input_init(&emu->input);
    
    printf("[INIT] Initializing audio...\n");
    // Initialize audio
    AudioState audio;
    audio_init(&audio);
    
    printf("[INIT] Resetting CPU...\n");
    cpu_reset(&emu->cpu);
    
    printf("[INIT] Setting emulator state...\n");
    emu->frame_count = 0;
    emu->running = true;
    emu->vram_writes = 0;
    emu->oam_writes = 0;
    emu->interrupts_fired = 0;
    
    printf("[INIT] Reading first instruction...\n");
    fflush(stdout);
    
    printf("Emulator initialized!\n");
    printf("Entry point: 0x%08X\n", emu->cpu.r[15]);
    printf("CPU Mode: %s\n", emu->cpu.thumb_mode ? "Thumb" : "ARM");
    
    // TEMP: Comment out to test if this causes unmapped reads
    // u32 pc_value = emu->cpu.r[15];
    // printf("[DEBUG] About to read from address: 0x%08X\n", pc_value);
    // fflush(stdout);
    // u32 first_instruction = mem_read32(&emu->memory, pc_value);
    // printf("First instruction: 0x%08X\n", first_instruction);
    printf("First instruction diagnostic disabled for testing\n");
}

static void emu_frame(EmulatorState *emu) {
    // Update input from AI or keyboard
    input_update(&emu->input, &emu->memory);
    
    // Track VRAM/OAM writes before frame
    u32 vram_before = emu->vram_writes;
    u32 oam_before = emu->oam_writes;
    
    // TEMP: Track if PC is making progress
    static u32 pc_history[10] = {0};
    static int pc_index = 0;
    static int unique_count = 0;
    
    if (emu->frame_count % 60 == 0) {
        pc_history[pc_index] = emu->cpu.r[15];
        pc_index = (pc_index + 1) % 10;
        
        // Count unique PCs in history
        unique_count = 0;
        for (int i = 0; i < 10; i++) {
            bool unique = true;
            for (int j = 0; j < i; j++) {
                if (pc_history[i] == pc_history[j]) {
                    unique = false;
                    break;
                }
            }
            if (unique && pc_history[i] != 0) unique_count++;
        }
    }
    
    // TEMP: If stuck at same PC for too long, help it along by enabling VBlank interrupt
    static u32 stuck_count = 0;
    static u32 last_pc = 0xFFFFFFFF;
    u32 current_pc = emu->cpu.r[15];
    
    if (current_pc == last_pc && current_pc >= 0x08000000 && current_pc < 0x0A000000) {
        stuck_count++;
        if (stuck_count == 60) {
            printf("\n=== STUCK DETECTED at PC=0x%08X (stuck for %d frames) ===\n", current_pc, stuck_count);
            
            // Check if interrupt handler pointer is set
            u32 handler_ptr = mem_read32(&emu->memory, 0x03007FFC);
            printf("Interrupt handler at [0x03007FFC] = 0x%08X\n", handler_ptr);
            if (handler_ptr == 0 || handler_ptr < 0x02000000 || handler_ptr >= 0x0A000000) {
                printf("WARNING: Invalid interrupt handler pointer!\n");
                printf("Game may not have initialized interrupts correctly.\n");
            } else {
                printf("Handler pointer looks valid (ROM/RAM address)\n");
                // Read first instruction at handler
                u32 first_inst = mem_read32(&emu->memory, handler_ptr & ~1); // Clear Thumb bit
                printf("First instruction at handler: 0x%08X\n", first_inst);
            }
            
            printf("Current CPSR: 0x%08X (Mode=%d, I=%d)\n", emu->cpu.cpsr, emu->cpu.cpsr & 0x1F, (emu->cpu.cpsr & 0x80) ? 1 : 0);
            
            printf("Enabling instruction trace for next frame...\n");
            debug_trace_enabled = true;
            debug_trace_init();
        }
        if (stuck_count == 61 && emu->interrupts.ie == 0) {
            // DISABLED TEMP FIX - let game enable interrupts naturally
            // printf("\n=== TEMP FIX: Enabling VBlank interrupt ===\n");
            // u32 handler = mem_read32(&emu->memory, 0x03007FFC);
            // printf("Handler at 0x03007FFC before enable: 0x%08X\n", handler);
            // emu->interrupts.ie = 0x0001;  // Enable VBlank
            // emu->interrupts.ime = 1;       // Enable interrupts globally
            // emu->memory.io_regs[0x200] = 0x01;  // IE low byte
            // emu->memory.io_regs[0x208] = 0x01;  // IME
            // printf("IE=0x%04X IME=%d\n", emu->interrupts.ie, emu->interrupts.ime);
        }
    } else {
        stuck_count = 0;
    }
    last_pc = current_pc;
    
    // Simulate all 228 scanlines (0-227) per frame
    // Scanlines 0-159: Visible area
    // Scanlines 160-227: VBlank period
    // Each frame takes about 280,000 cycles at 60 FPS
    const u32 CYCLES_PER_SCANLINE = 1232; // ~280000 / 228
    
    for (int scanline = 0; scanline < 228; scanline++) {
        // Update DISPSTAT for this scanline
        interrupt_update_vcount(&emu->interrupts, scanline);
        
        // Trigger DMA at VBlank start
        if (scanline == 160) {
            dma_trigger(&emu->dma, &emu->memory, 1); // VBlank DMA
        }
        
        // Trigger DMA at HBlank (simplified - once per scanline)
        if (scanline < 160) {
            dma_trigger(&emu->dma, &emu->memory, 2); // HBlank DMA
        }
        
        // Execute CPU for this scanline
        u32 cycles_left = CYCLES_PER_SCANLINE;
        while (cycles_left > 0 && !emu->cpu.halted) {
            u32 cycles = cpu_step(&emu->cpu, &emu->memory);
            cycles_left = (cycles > cycles_left) ? 0 : (cycles_left - cycles);
            
            // Update timers
            timer_update(&emu->timers, cycles, &emu->interrupts);
            
            // Check for interrupts
            if (interrupt_check(&emu->interrupts) && !(emu->cpu.cpsr & 0x80)) {
                cpu_handle_interrupt(&emu->cpu, &emu->memory);
            }
        }
        
        // Track VBlank interrupts
        if (scanline == 160 && (emu->interrupts.if_flag & INT_VBLANK)) {
            emu->interrupts_fired++;
        }
    }
    
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
    
    printf("SDL Window and Renderer created successfully!\n");
    printf("Window size: %dx%d\n", GBA_SCREEN_WIDTH * 2, GBA_SCREEN_HEIGHT * 2);
    printf("Graphics should be visible now.\n");
    
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
                // Toggle debug display with F1
                if (event.key.keysym.sym == SDLK_F1) {
                    emu.gfx.show_debug = !emu.gfx.show_debug;
                    printf("Debug display: %s\n", emu.gfx.show_debug ? "ON" : "OFF");
                }
            }
        }
        
        // Map current keyboard state to GBA buttons (checked every frame)
        const u8 *keys = SDL_GetKeyboardState(NULL);
        u8 ai_input = 0;
        if (keys[SDL_SCANCODE_Z]) ai_input |= KEY_A;
        if (keys[SDL_SCANCODE_X]) ai_input |= KEY_B;
        if (keys[SDL_SCANCODE_UP]) ai_input |= KEY_UP;
        if (keys[SDL_SCANCODE_DOWN]) ai_input |= KEY_DOWN;
        if (keys[SDL_SCANCODE_LEFT]) ai_input |= KEY_LEFT;
        if (keys[SDL_SCANCODE_RIGHT]) ai_input |= KEY_RIGHT;
        if (keys[SDL_SCANCODE_RETURN]) ai_input |= KEY_START;
        if (keys[SDL_SCANCODE_RSHIFT]) ai_input |= KEY_SELECT;
        
        mem_set_ai_input(&emu.memory, ai_input);
        
        // Run one frame
        emu_frame(&emu);
        
        // Draw debug overlay
        gfx_draw_debug_info(&emu.gfx, &emu.memory, emu.cpu.r[15], emu.cpu.r[13], emu.cpu.r[14], 
                            emu.cpu.cpsr, emu.cpu.thumb_mode,
                            emu.interrupts.ie, emu.interrupts.if_flag, emu.interrupts.ime,
                            emu.frame_count);
        
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
            u16 dispcnt = mem_read16(&emu.memory, 0x04000000);
            
            // Count unique PCs for progress tracking
            static u32 pc_hist[10] = {0};
            static int pc_idx = 0;
            pc_hist[pc_idx] = emu.cpu.r[15];
            pc_idx = (pc_idx + 1) % 10;
            int uniq = 0;
            for (int i = 0; i < 10; i++) {
                bool is_uniq = true;
                for (int j = 0; j < i; j++) {
                    if (pc_hist[i] == pc_hist[j]) { is_uniq = false; break; }
                }
                if (is_uniq && pc_hist[i] != 0) uniq++;
            }
            
            // Debug: print PC history after frame 120
            if (emu.frame_count >= 120 && emu.frame_count <= 240) {
                printf("PC history: ");
                for (int i = 0; i < 10; i++) {
                    printf("0x%08X ", pc_hist[i]);
                }
                printf("\n");
            }
            
            printf("Frame %llu | PC=0x%08X | DISPCNT=0x%04X | IE=0x%04X IF=0x%04X IME=%d | Ints=%u | CPSR=0x%08X (I=%d) | Input=0x%02X | UniqPC=%d\n",
                   (unsigned long long)emu.frame_count,
                   emu.cpu.r[15],
                   dispcnt,
                   emu.interrupts.ie,
                   emu.interrupts.if_flag,
                   emu.interrupts.ime,
                   emu.frame_count,
                   emu.cpu.cpsr,
                   (emu.cpu.cpsr & 0x80) ? 1 : 0,
                   mem_get_ai_input(&emu.memory),
                   uniq);
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
