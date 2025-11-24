#include "gba_shim.h"
#include <SDL.h>

static u16 s_keyState = 0;
static u16 s_framebuffer[240 * 160];

// Helper texture cache
static SDL_Texture* s_fbTexture = NULL;

void VBlankIntrWait(void)
{
    // approximate a small wait to simulate frame timing
    SDL_Delay(16);
}

u16 ReadKeyInput(void)
{
    // TODO: translate SDL key state into GBA key bitmask if needed
    return s_keyState;
}

void gba_set_key_state(u16 keys)
{
    s_keyState = keys;
}

u16* gba_get_framebuffer(void)
{
    return s_framebuffer;
}

void gba_render_framebuffer(SDL_Renderer* ren)
{
    if (!ren) return;
    if (!s_fbTexture) {
        s_fbTexture = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 240, 160);
    }
    if (!s_fbTexture) return;

    // Update texture with raw RGB565 framebuffer
    SDL_UpdateTexture(s_fbTexture, NULL, s_framebuffer, 240 * sizeof(u16));
    // Scale to window: assume window larger than 240x160
    SDL_Rect dst = {0, 0, 240 * 2, 160 * 2};
    SDL_RenderCopy(ren, s_fbTexture, NULL, &dst);
}
