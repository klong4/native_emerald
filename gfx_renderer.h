#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include "types.h"
#include "memory.h"
#include <SDL.h>

typedef struct CPU CPU;
typedef struct InterruptState InterruptState;

typedef struct {
    u16 framebuffer[GBA_FRAMEBUFFER_SIZE];
    bool dirty;
    bool show_debug;
} GFXState;

void gfx_init(GFXState *gfx);
void gfx_render_frame(GFXState *gfx, Memory *mem);
void gfx_present(GFXState *gfx, SDL_Renderer *renderer);
void gfx_draw_debug_info(GFXState *gfx, Memory *mem, u32 pc, u32 sp, u32 lr, u32 cpsr, bool thumb, 
                         u16 ie, u16 if_flag, u16 ime, u64 frame_count);

#endif // GFX_RENDERER_H
