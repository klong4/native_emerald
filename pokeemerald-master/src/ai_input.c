// AI input implementation for ROM builds. This file is NOT compiled into the
// native runner; native runner provides its own `ai_input.c` implementation.

#include "global.h"
#include "ai_input.h"

// Expose a simple byte in WRAM that external tools (or emulator bridges)
// can write to in order to control inputs for one frame.
// Place in a commonsymbol region used by the build system.

EWRAM_DATA volatile u8 gAiInputState = 0;

// Default no-op setter for ROM builds; tools should write to `gAiInputState`
// directly instead of calling this function.
void AI_SetButtons(u8 mask)
{
    gAiInputState = mask;
}

u8 AI_GetButtons(void)
{
    return gAiInputState;
}
