#include <stdio.h>
#include <stdint.h>
#include <SDL.h>
#include "gba_shim.h"
#include "ai_input.h"
#include "policy_example.h"
#include "game_init.h"

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("PokeEmerald Native Runner",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       240 * 2, 160 * 2,
                                       SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    // Initialize game systems
    game_init();

    int running = 1;
    uint32_t frame_count = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        // Read current AI buttons (bitmask)
        uint8_t ai_buttons = ai_read_buttons();
        // Map ai_buttons into GBA key state (simple mapping: bit 0 -> A)
        u16 keymask = 0;
        if (ai_buttons & 0x1) keymask |= (1 << 0); // A placeholder bit
        gba_set_key_state(keymask);

        // Update game logic
        game_update();

        // Call AI policy tick (embedded)
        policy_tick();

        // Render game to framebuffer
        u16 *fb = gba_get_framebuffer();
        game_render(fb);
        frame_count++;

        // Render framebuffer from shim if populated
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        gba_render_framebuffer(ren);
        SDL_RenderPresent(ren);

        VBlankIntrWait();
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
