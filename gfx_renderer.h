#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include "types.h"
#include "memory.h"
#include <SDL.h>

typedef struct {
    u16 framebuffer[GBA_FRAMEBUFFER_SIZE];
    bool dirty;
} GFXState;

void gfx_init(GFXState *gfx);
void gfx_render_frame(GFXState *gfx, Memory *mem);
void gfx_present(GFXState *gfx, SDL_Renderer *renderer);

#endif // GFX_RENDERER_H
