#ifndef AI_INPUT_H
#define AI_INPUT_H

#include "global.h"

// In-ROM AI input interface.
// The ROM exposes a small RAM-backed byte `gAiInputState` that external tools
// (or the native runner) can set to control button presses for one frame.

// Set the AI button mask (for native hosts that link with the ROM code).
// For ROM builds this may be a no-op; the external tool should write directly
// to the `gAiInputState` variable in WRAM.
void AI_SetButtons(u8 mask);

// Read the current AI button mask (polled by game input handler).
u8 AI_GetButtons(void);

// WRAM-exposed variable name for external tools to write to.
extern volatile u8 gAiInputState;

#endif // AI_INPUT_H
