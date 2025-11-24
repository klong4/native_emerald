// Game initialization API for native runner
#ifndef GAME_INIT_H
#define GAME_INIT_H

#include "gba_shim.h"

// Initialize game systems
void game_init(void);

// Update game state (call once per frame)
void game_update(void);

// Render game to framebuffer
void game_render(u16 *framebuffer);

#endif // GAME_INIT_H
