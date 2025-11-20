#include "gfx_renderer.h"
#include <string.h>

static SDL_Texture *s_texture = NULL;

// I/O register offsets
#define REG_DISPCNT   0x00
#define REG_BG0CNT    0x08
#define REG_BG0HOFS   0x10
#define REG_BG0VOFS   0x12

// DISPCNT bits
#define DISPCNT_MODE_MASK  0x07
#define DISPCNT_BG0_ON     0x0100
#define DISPCNT_BG1_ON     0x0200
#define DISPCNT_BG2_ON     0x0400
#define DISPCNT_BG3_ON     0x0800
#define DISPCNT_OBJ_ON     0x1000
#define DISPCNT_OBJ_1D     0x0040

// Convert GBA BGR555 to SDL RGB565
static inline u16 convert_color(u16 bgr555) {
    u8 r = (bgr555 & 0x1F);
    u8 g = ((bgr555 >> 5) & 0x1F);
    u8 b = ((bgr555 >> 10) & 0x1F);
    return (r << 11) | (g << 6) | (b);
}

// Render text mode background
static void render_text_bg(GFXState *gfx, Memory *mem, int bg_num, u16 bg_cnt, u16 h_ofs, u16 v_ofs) {
    u32 screen_base = ((bg_cnt >> 8) & 0x1F) * 0x800;
    u32 char_base = ((bg_cnt >> 2) & 0x3) * 0x4000;
    bool use_8bpp = bg_cnt & 0x80;
    u32 screen_size = bg_cnt >> 14;
    
    u32 map_w = (screen_size & 1) ? 512 : 256;
    u32 map_h = (screen_size & 2) ? 512 : 256;
    
    for (int sy = 0; sy < GBA_SCREEN_HEIGHT; sy++) {
        for (int sx = 0; sx < GBA_SCREEN_WIDTH; sx++) {
            u32 mx = (sx + h_ofs) % map_w;
            u32 my = (sy + v_ofs) % map_h;
            
            u32 screen_ofs = 0;
            if (mx >= 256) { screen_ofs += 0x800; mx -= 256; }
            if (my >= 256) { screen_ofs += (map_w == 512) ? 0x800 : 0x1000; my -= 256; }
            
            u32 tx = mx / 8, ty = my / 8;
            u32 px = mx % 8, py = my % 8;
            
            u32 se_addr = 0x06000000 + screen_base + screen_ofs + (ty * 32 + tx) * 2;
            u16 se = mem_read16(mem, se_addr);
            
            u32 tile_num = se & 0x3FF;
            bool h_flip = se & 0x400;
            bool v_flip = se & 0x800;
            u32 pal_bank = (se >> 12) & 0xF;
            
            u32 fpx = h_flip ? (7 - px) : px;
            u32 fpy = v_flip ? (7 - py) : py;
            
            u8 col_idx;
            if (use_8bpp) {
                u32 addr = 0x06000000 + char_base + tile_num * 64 + fpy * 8 + fpx;
                col_idx = mem_read8(mem, addr);
            } else {
                u32 addr = 0x06000000 + char_base + tile_num * 32 + fpy * 4 + fpx / 2;
                u8 data = mem_read8(mem, addr);
                col_idx = (fpx & 1) ? (data >> 4) : (data & 0xF);
                if (col_idx != 0) col_idx += pal_bank * 16;
            }
            
            if (col_idx == 0) continue;
            
            u16 color = mem_read16(mem, 0x05000000 + col_idx * 2);
            gfx->framebuffer[sy * GBA_SCREEN_WIDTH + sx] = convert_color(color);
        }
    }
}

// Render sprites
static void render_sprites(GFXState *gfx, Memory *mem, bool obj_1d) {
    static const u8 obj_sizes[4][4][2] = {
        {{8,8}, {16,16}, {32,32}, {64,64}},
        {{16,8}, {32,8}, {32,16}, {64,32}},
        {{8,16}, {8,32}, {16,32}, {32,64}},
        {{0,0}, {0,0}, {0,0}, {0,0}}
    };
    
    for (int obj = 0; obj < 128; obj++) {
        u32 oam_base = 0x07000000 + obj * 8;
        u16 attr0 = mem_read16(mem, oam_base + 0);
        u16 attr1 = mem_read16(mem, oam_base + 2);
        u16 attr2 = mem_read16(mem, oam_base + 4);
        
        if (((attr0 >> 8) & 0x3) == 2) continue;
        
        s16 y = attr0 & 0xFF;
        s16 x = attr1 & 0x1FF;
        if (x >= 240) x -= 512;
        if (y >= 160) y -= 256;
        
        u8 shape = (attr0 >> 14) & 0x3;
        u8 size = (attr1 >> 14) & 0x3;
        u8 w = obj_sizes[shape][size][0];
        u8 h = obj_sizes[shape][size][1];
        if (w == 0) continue;
        
        u32 tile_num = attr2 & 0x3FF;
        u8 pal_bank = (attr2 >> 12) & 0xF;
        bool use_8bpp = attr0 & 0x2000;
        bool h_flip = attr1 & 0x1000;
        bool v_flip = attr1 & 0x2000;
        
        for (int sy = 0; sy < h; sy++) {
            s16 screen_y = y + sy;
            if (screen_y < 0 || screen_y >= GBA_SCREEN_HEIGHT) continue;
            
            for (int sx = 0; sx < w; sx++) {
                s16 screen_x = x + sx;
                if (screen_x < 0 || screen_x >= GBA_SCREEN_WIDTH) continue;
                
                u8 px = h_flip ? (w - 1 - sx) : sx;
                u8 py = v_flip ? (h - 1 - sy) : sy;
                
                u32 tile_addr;
                if (obj_1d) {
                    u32 ofs = (py / 8) * (w / 8) + (px / 8);
                    tile_addr = 0x06010000 + (tile_num + (use_8bpp ? ofs * 2 : ofs)) * 32;
                } else {
                    u32 ofs = (py / 8) * 32 + (px / 8);
                    tile_addr = 0x06010000 + (tile_num + ofs) * 32;
                }
                
                u8 pxt = px % 8, pyt = py % 8;
                u8 col_idx;
                if (use_8bpp) {
                    col_idx = mem_read8(mem, tile_addr + pyt * 8 + pxt);
                } else {
                    u8 data = mem_read8(mem, tile_addr + pyt * 4 + pxt / 2);
                    col_idx = (pxt & 1) ? (data >> 4) : (data & 0xF);
                    if (col_idx != 0) col_idx += pal_bank * 16;
                }
                
                if (col_idx == 0) continue;
                
                u16 color = mem_read16(mem, 0x05000200 + col_idx * 2);
                gfx->framebuffer[screen_y * GBA_SCREEN_WIDTH + screen_x] = convert_color(color);
            }
        }
    }
}

void gfx_init(GFXState *gfx) {
    if (!gfx) return;
    memset(gfx->framebuffer, 0, sizeof(gfx->framebuffer));
    gfx->dirty = true;
}

void gfx_render_frame(GFXState *gfx, Memory *mem) {
    if (!gfx || !mem) return;
    
    // Get backdrop color
    u16 backdrop = mem_read16(mem, 0x05000000);
    u16 backdrop_rgb = convert_color(backdrop);
    for (int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        gfx->framebuffer[i] = backdrop_rgb;
    }
    
    // Read DISPCNT
    u16 dispcnt = mem_read16(mem, 0x04000000 + REG_DISPCNT);
    u8 mode = dispcnt & DISPCNT_MODE_MASK;
    
    // Render text modes (0-2)
    if (mode <= 2) {
        for (int i = 3; i >= 0; i--) {
            if (!(dispcnt & (DISPCNT_BG0_ON << i))) continue;
            
            u16 bg_cnt = mem_read16(mem, 0x04000000 + REG_BG0CNT + i * 2);
            u16 h_ofs = mem_read16(mem, 0x04000000 + REG_BG0HOFS + i * 4);
            u16 v_ofs = mem_read16(mem, 0x04000000 + REG_BG0VOFS + i * 4);
            
            render_text_bg(gfx, mem, i, bg_cnt, h_ofs, v_ofs);
        }
        
        if (dispcnt & DISPCNT_OBJ_ON) {
            render_sprites(gfx, mem, dispcnt & DISPCNT_OBJ_1D);
        }
    }
    
    gfx->dirty = true;
}

void gfx_present(GFXState *gfx, SDL_Renderer *renderer) {
    if (!gfx || !renderer) return;
    
    if (!s_texture) {
        s_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT);
    }
    
    if (s_texture && gfx->dirty) {
        SDL_UpdateTexture(s_texture, NULL, gfx->framebuffer,
                         GBA_SCREEN_WIDTH * sizeof(u16));
        gfx->dirty = false;
    }
    
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}
