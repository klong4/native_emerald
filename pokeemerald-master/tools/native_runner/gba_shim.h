#ifndef GBA_SHIM_H
#define GBA_SHIM_H

#include <stdint.h>
#include <SDL.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// Minimal shim functions used by some GBA code paths. Expand as porting progresses.
void VBlankIntrWait(void);
u16 ReadKeyInput(void);

// Set key state driven by SDL keyboard mapping or AI input.
void gba_set_key_state(u16 keys);

// Simple framebuffer (RGB565) exposed for native rendering.
// Returns pointer to a 240x160 array of u16 pixels (row-major)
u16* gba_get_framebuffer(void);
// Render the framebuffer to an SDL renderer (helper)
void gba_render_framebuffer(SDL_Renderer* ren);

#endif // GBA_SHIM_H
