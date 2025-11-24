// Minimal game initialization for native runner
// This provides a simplified entry point that calls into actual game code

#include "global.h"
#include "gba_shim.h"
#include "main.h"
#include "task.h"
#include "overworld.h"
#include "malloc.h"
#include "bg.h"
#include "window.h"
#include "text.h"
#include "gpu_regs.h"
#include "palette.h"
#include "sprite.h"
#include <string.h>

extern u8 gHeap[];
#define HEAP_SIZE 0x1C000

static u8 sGameInitialized = FALSE;

// Minimal game initialization - calls real game init functions
void game_init(void) {
    if (sGameInitialized)
        return;
        
    // Initialize heap
    InitHeap(gHeap, HEAP_SIZE);
    
    // Initialize graphics systems
    InitGpuRegManager();
    ResetBgs();
    
    // Initialize text system
    SetDefaultFontsPointer();
    
    // Initialize task system
    ResetTasks();
    
    // Initialize sprite system
    ResetSpriteData();
    
    sGameInitialized = TRUE;
}

// Per-frame game update - calls real game loop
void game_update(void) {
    if (!sGameInitialized)
        return;
        
    // Run all active tasks
    RunTasks();
}

// Render game graphics to framebuffer
void game_render(u16 *framebuffer) {
    if (!sGameInitialized) {
        // Not initialized - clear to black
        memset(framebuffer, 0, 240 * 160 * 2);
        return;
    }
    
    // Copy buffered GPU register values
    CopyBufferedValuesToGpuRegs();
    
    // For now, just show a gradient to indicate it's working
    // TODO: Render actual BG layers and sprites to framebuffer
    // This requires implementing a software renderer for the GBA tile/sprite system
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x++) {
            // Green gradient to show the system is alive
            u16 r = 0;
            u16 g = ((x + y) * 31) / (240 + 160);
            u16 b = 0;
            framebuffer[y * 240 + x] = (b << 10) | (g << 5) | r;
        }
    }
}
