#include "gfx_renderer.h"
#include "cpu_core.h"
#include "interrupts.h"
#include <string.h>
#include <stdio.h>

static SDL_Texture *s_texture = NULL;

// PPU Layer types
typedef enum {
    LAYER_BG0 = 0,
    LAYER_BG1 = 1,
    LAYER_BG2 = 2,
    LAYER_BG3 = 3,
    LAYER_OBJ = 4,
    LAYER_BACKDROP = 5
} LayerType;

typedef struct {
    u16 color;
    u8 priority;
    u8 layer;
    bool transparent;
} Pixel;

// Rendering context for a scanline
typedef struct {
    Pixel pixels[GBA_SCREEN_WIDTH];
    u16 dispcnt;
    u16 bg_cnt[4];
    u16 bg_hofs[4];
    u16 bg_vofs[4];
    s16 bg_pa[2], bg_pb[2], bg_pc[2], bg_pd[2];
    s32 bg_x[2], bg_y[2];
    u16 bldcnt;
    u16 bldalpha;
    u16 bldy;
    u16 mosaic;
    u16 win0h, win1h, win0v, win1v;
    u16 winin, winout;
} ScanlineContext;

// Simple 3x5 font for debug text  
static const u8 font_3x5[][5] = {
    {0x7,0x5,0x5,0x5,0x7}, // 0
    {0x2,0x2,0x2,0x2,0x2}, // 1
    {0x7,0x1,0x7,0x4,0x7}, // 2
    {0x7,0x1,0x7,0x1,0x7}, // 3
    {0x5,0x5,0x7,0x1,0x1}, // 4
    {0x7,0x4,0x7,0x1,0x7}, // 5
    {0x7,0x4,0x7,0x5,0x7}, // 6
    {0x7,0x1,0x1,0x1,0x1}, // 7
    {0x7,0x5,0x7,0x5,0x7}, // 8
    {0x7,0x5,0x7,0x1,0x7}, // 9
    {0x7,0x5,0x7,0x5,0x5}, // A
    {0x6,0x5,0x6,0x5,0x6}, // B
    {0x7,0x4,0x4,0x4,0x7}, // C
    {0x6,0x5,0x5,0x5,0x6}, // D
    {0x7,0x4,0x7,0x4,0x7}, // E
    {0x7,0x4,0x7,0x4,0x4}, // F
};

static void draw_char(GFXState *gfx, int x, int y, char c, u16 color) {
    if (x < 0 || y < 0 || x >= GBA_SCREEN_WIDTH - 3 || y >= GBA_SCREEN_HEIGHT - 5) return;
    
    int idx = -1;
    if (c >= '0' && c <= '9') idx = c - '0';
    else if (c >= 'A' && c <= 'F') idx = 10 + (c - 'A');
    else if (c >= 'a' && c <= 'f') idx = 10 + (c - 'a');
    else return;
    
    for (int py = 0; py < 5; py++) {
        for (int px = 0; px < 3; px++) {
            if (font_3x5[idx][py] & (1 << (2 - px))) {
                int sx = x + px;
                int sy = y + py;
                if (sx >= 0 && sx < GBA_SCREEN_WIDTH && sy >= 0 && sy < GBA_SCREEN_HEIGHT) {
                    gfx->framebuffer[sy * GBA_SCREEN_WIDTH + sx] = color;
                }
            }
        }
    }
}

static void draw_text(GFXState *gfx, int x, int y, const char *text, u16 color) {
    int cx = x;
    while (*text) {
        if (*text == ' ') {
            cx += 4;
        } else {
            draw_char(gfx, cx, y, *text, color);
            cx += 4;
        }
        text++;
    }
}

static void draw_box(GFXState *gfx, int x, int y, int w, int h, u16 color) {
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int sx = x + px;
            int sy = y + py;
            if (sx >= 0 && sx < GBA_SCREEN_WIDTH && sy >= 0 && sy < GBA_SCREEN_HEIGHT) {
                if (px == 0 || px == w-1 || py == 0 || py == h-1) {
                    gfx->framebuffer[sy * GBA_SCREEN_WIDTH + sx] = color;
                } else {
                    // Semi-transparent background (darken)
                    u16 orig = gfx->framebuffer[sy * GBA_SCREEN_WIDTH + sx];
                    gfx->framebuffer[sy * GBA_SCREEN_WIDTH + sx] = ((orig >> 1) & 0x7BEF);
                }
            }
        }
    }
}

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
    // Expand 5-bit to proper RGB565
    return ((r << 11) | (g << 6) | b);
}

// Convert BGR555 to RGB888 components for blending
static inline void bgr555_to_rgb(u16 bgr555, u8 *r, u8 *g, u8 *b) {
    *r = (bgr555 & 0x1F) << 3;
    *g = ((bgr555 >> 5) & 0x1F) << 3;
    *b = ((bgr555 >> 10) & 0x1F) << 3;
}

// Convert RGB888 back to BGR555
static inline u16 rgb_to_bgr555(u8 r, u8 g, u8 b) {
    return ((r >> 3) & 0x1F) | (((g >> 3) & 0x1F) << 5) | (((b >> 3) & 0x1F) << 10);
}

// Alpha blend two colors
static inline u16 alpha_blend(u16 top, u16 bottom, u8 eva, u8 evb) {
    u8 r1, g1, b1, r2, g2, b2;
    bgr555_to_rgb(top, &r1, &g1, &b1);
    bgr555_to_rgb(bottom, &r2, &g2, &b2);
    
    u8 r = ((r1 * eva) + (r2 * evb)) >> 4;
    u8 g = ((g1 * eva) + (g2 * evb)) >> 4;
    u8 b = ((b1 * eva) + (b2 * evb)) >> 4;
    
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    
    return rgb_to_bgr555(r, g, b);
}

// Brightness increase/decrease
static inline u16 brightness_adjust(u16 color, u8 evy, bool increase) {
    u8 r, g, b;
    bgr555_to_rgb(color, &r, &g, &b);
    
    if (increase) {
        r = r + (((255 - r) * evy) >> 4);
        g = g + (((255 - g) * evy) >> 4);
        b = b + (((255 - b) * evy) >> 4);
    } else {
        r = r - ((r * evy) >> 4);
        g = g - ((g * evy) >> 4);
        b = b - ((b * evy) >> 4);
    }
    
    return rgb_to_bgr555(r, g, b);
}

// Render text mode background scanline
static void render_text_bg_scanline(ScanlineContext *ctx, Memory *mem, int bg_num, int scanline, Pixel *line) {
    u16 bg_cnt = ctx->bg_cnt[bg_num];
    u16 h_ofs = ctx->bg_hofs[bg_num];
    u16 v_ofs = ctx->bg_vofs[bg_num];
    
    u8 priority = bg_cnt & 0x3;
    u32 char_base = ((bg_cnt >> 2) & 0x3) * 0x4000;
    u32 screen_base = ((bg_cnt >> 8) & 0x1F) * 0x800;
    bool use_8bpp = bg_cnt & 0x80;
    u32 screen_size = bg_cnt >> 14;
    
    u32 map_w = (screen_size & 1) ? 512 : 256;
    u32 map_h = (screen_size & 2) ? 512 : 256;
    
    u32 my = (scanline + v_ofs) % map_h;
    
    for (int sx = 0; sx < GBA_SCREEN_WIDTH; sx++) {
        u32 mx = (sx + h_ofs) % map_w;
        
        // Calculate which screen block we're in
        u32 screen_ofs = 0;
        u32 adj_mx = mx;
        u32 adj_my = my;
        
        if (adj_mx >= 256) { 
            screen_ofs += 0x800; 
            adj_mx -= 256; 
        }
        if (adj_my >= 256) { 
            screen_ofs += (map_w == 512) ? 0x800 : 0x1000; 
            adj_my -= 256; 
        }
        
        u32 tx = adj_mx / 8, ty = adj_my / 8;
        u32 px = adj_mx % 8, py = adj_my % 8;
        
        // Read screen entry
        u32 se_addr = 0x06000000 + screen_base + screen_ofs + (ty * 32 + tx) * 2;
        u16 se = mem_read16(mem, se_addr);
        
        u32 tile_num = se & 0x3FF;
        bool h_flip = se & 0x400;
        bool v_flip = se & 0x800;
        u32 pal_bank = (se >> 12) & 0xF;
        
        u32 fpx = h_flip ? (7 - px) : px;
        u32 fpy = v_flip ? (7 - py) : py;
        
        // Read pixel color index
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
        
        if (col_idx == 0) {
            line[sx].transparent = true;
            continue;
        }
        
        // Read palette color
        u16 color = mem_read16(mem, 0x05000000 + col_idx * 2);
        line[sx].color = color;
        line[sx].priority = priority;
        line[sx].layer = LAYER_BG0 + bg_num;
        line[sx].transparent = false;
    }
}

// Render affine background scanline (Mode 1-2: BG2/BG3)
static void render_affine_bg_scanline(ScanlineContext *ctx, Memory *mem, int bg_num, int scanline, Pixel *line) {
    u16 bg_cnt = ctx->bg_cnt[bg_num];
    u8 priority = bg_cnt & 0x3;
    u32 char_base = ((bg_cnt >> 2) & 0x3) * 0x4000;
    u32 screen_base = ((bg_cnt >> 8) & 0x1F) * 0x800;
    u32 screen_size = (bg_cnt >> 14) & 0x3;
    bool wraparound = bg_cnt & 0x2000;
    
    u32 map_size = 128 << screen_size; // 128, 256, 512, or 1024
    
    int idx = bg_num - 2; // BG2=0, BG3=1
    s32 x = ctx->bg_x[idx];
    s32 y = ctx->bg_y[idx];
    s16 pa = ctx->bg_pa[idx];
    s16 pb = ctx->bg_pb[idx];
    
    for (int sx = 0; sx < GBA_SCREEN_WIDTH; sx++) {
        s32 tex_x = x >> 8;
        s32 tex_y = y >> 8;
        
        // Handle wraparound
        if (!wraparound) {
            if (tex_x < 0 || tex_x >= (s32)map_size || tex_y < 0 || tex_y >= (s32)map_size) {
                line[sx].transparent = true;
                x += pa;
                y += pb;
                continue;
            }
        } else {
            tex_x &= (map_size - 1);
            tex_y &= (map_size - 1);
        }
        
        u32 tx = tex_x / 8;
        u32 ty = tex_y / 8;
        u32 px = tex_x % 8;
        u32 py = tex_y % 8;
        
        u32 se_addr = 0x06000000 + screen_base + (ty * (map_size / 8) + tx);
        u8 tile_num = mem_read8(mem, se_addr);
        
        u32 addr = 0x06000000 + char_base + tile_num * 64 + py * 8 + px;
        u8 col_idx = mem_read8(mem, addr);
        
        if (col_idx == 0) {
            line[sx].transparent = true;
        } else {
            u16 color = mem_read16(mem, 0x05000000 + col_idx * 2);
            line[sx].color = color;
            line[sx].priority = priority;
            line[sx].layer = LAYER_BG0 + bg_num;
            line[sx].transparent = false;
        }
        
        x += pa;
        y += pb;
    }
}

// Render bitmap background scanline (Mode 3-5)
static void render_bitmap_bg_scanline(ScanlineContext *ctx, Memory *mem, int mode, int scanline, Pixel *line) {
    for (int sx = 0; sx < GBA_SCREEN_WIDTH; sx++) {
        u16 color = 0;
        
        if (mode == 3) {
            // Mode 3: 240x160, 16bpp direct color
            u32 addr = 0x06000000 + (scanline * 240 + sx) * 2;
            color = mem_read16(mem, addr);
        } else if (mode == 4) {
            // Mode 4: 240x160, 8bpp indexed
            u32 frame_offset = (ctx->dispcnt & 0x10) ? 0xA000 : 0;
            u32 addr = 0x06000000 + frame_offset + scanline * 240 + sx;
            u8 idx = mem_read8(mem, addr);
            color = mem_read16(mem, 0x05000000 + idx * 2);
        } else if (mode == 5) {
            // Mode 5: 160x128, 16bpp direct color
            if (sx < 160 && scanline < 128) {
                u32 frame_offset = (ctx->dispcnt & 0x10) ? 0xA000 : 0;
                u32 addr = 0x06000000 + frame_offset + (scanline * 160 + sx) * 2;
                color = mem_read16(mem, addr);
            }
        }
        
        line[sx].color = color;
        line[sx].priority = 0;
        line[sx].layer = LAYER_BG2;
        line[sx].transparent = false;
    }
}
// Render sprites scanline
static void render_sprites_scanline(ScanlineContext *ctx, Memory *mem, int scanline, Pixel obj_line[4][GBA_SCREEN_WIDTH]) {
    static const u8 obj_sizes[4][4][2] = {
        {{8,8}, {16,16}, {32,32}, {64,64}},
        {{16,8}, {32,8}, {32,16}, {64,32}},
        {{8,16}, {8,32}, {16,32}, {32,64}},
        {{0,0}, {0,0}, {0,0}, {0,0}}
    };
    
    bool obj_1d = ctx->dispcnt & DISPCNT_OBJ_1D;
    
    // Initialize sprite lines
    for (int p = 0; p < 4; p++) {
        for (int sx = 0; sx < GBA_SCREEN_WIDTH; sx++) {
            obj_line[p][sx].transparent = true;
        }
    }
    
    // Render sprites back to front (127 to 0)
    for (int obj = 127; obj >= 0; obj--) {
        u32 oam_base = 0x07000000 + obj * 8;
        u16 attr0 = mem_read16(mem, oam_base + 0);
        u16 attr1 = mem_read16(mem, oam_base + 2);
        u16 attr2 = mem_read16(mem, oam_base + 4);
        
        // Check if sprite is disabled
        u8 obj_mode = (attr0 >> 8) & 0x3;
        if (obj_mode == 2) continue; // Disabled
        
        // Get sprite position
        s16 y = attr0 & 0xFF;
        s16 x = attr1 & 0x1FF;
        if (x >= 240) x -= 512;
        
        // Get sprite size
        u8 shape = (attr0 >> 14) & 0x3;
        u8 size = (attr1 >> 14) & 0x3;
        u8 w = obj_sizes[shape][size][0];
        u8 h = obj_sizes[shape][size][1];
        if (w == 0) continue;
        
        // Check if sprite is on this scanline
        if (y > 160) y -= 256;
        if (scanline < y || scanline >= y + h) continue;
        
        u32 tile_num = attr2 & 0x3FF;
        u8 pal_bank = (attr2 >> 12) & 0xF;
        u8 priority = (attr2 >> 10) & 0x3;
        bool use_8bpp = attr0 & 0x2000;
        bool h_flip = attr1 & 0x1000;
        bool v_flip = attr1 & 0x2000;
        bool semitransparent = (obj_mode == 1);
        
        int sy = scanline - y;
        int py = v_flip ? (h - 1 - sy) : sy;
        
        for (int sx = 0; sx < w; sx++) {
            s16 screen_x = x + sx;
            if (screen_x < 0 || screen_x >= GBA_SCREEN_WIDTH) continue;
            
            int px = h_flip ? (w - 1 - sx) : sx;
            
            // Calculate tile address
            u32 tile_addr;
            if (obj_1d) {
                // 1D mapping
                u32 tile_row = py / 8;
                u32 tile_col = px / 8;
                u32 tile_offset = tile_row * (w / 8) + tile_col;
                if (use_8bpp) tile_offset *= 2;
                tile_addr = 0x06010000 + (tile_num + tile_offset) * 32;
            } else {
                // 2D mapping
                u32 tile_row = py / 8;
                u32 tile_col = px / 8;
                u32 tile_offset = tile_row * 32 + tile_col;
                tile_addr = 0x06010000 + (tile_num + tile_offset) * 32;
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
            
            if (col_idx == 0) continue; // Transparent
            
            u16 color = mem_read16(mem, 0x05000200 + col_idx * 2);
            
            obj_line[priority][screen_x].color = color;
            obj_line[priority][screen_x].priority = priority;
            obj_line[priority][screen_x].layer = LAYER_OBJ;
            obj_line[priority][screen_x].transparent = false;
        }
    }
}

// Compose layers with priority and effects
static void compose_scanline(ScanlineContext *ctx, Pixel bg_lines[4][GBA_SCREEN_WIDTH], 
                             Pixel obj_line[4][GBA_SCREEN_WIDTH], u16 backdrop, u16 *output) {
    u16 bldcnt = ctx->bldcnt;
    u8 blend_mode = (bldcnt >> 6) & 0x3;
    u8 eva = ctx->bldalpha & 0x1F;
    u8 evb = (ctx->bldalpha >> 8) & 0x1F;
    u8 evy = ctx->bldy & 0x1F;
    
    if (eva > 16) eva = 16;
    if (evb > 16) evb = 16;
    if (evy > 16) evy = 16;
    
    // Blend target masks
    bool target1[6] = {false};
    bool target2[6] = {false};
    for (int i = 0; i < 5; i++) {
        target1[i] = (bldcnt & (1 << i)) != 0;
        target2[i] = (bldcnt & (1 << (8 + i))) != 0;
    }
    target1[5] = (bldcnt & 0x20) != 0; // Backdrop
    target2[5] = (bldcnt & 0x2000) != 0;
    
    for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
        // Collect all visible pixels at this position, sorted by priority
        typedef struct { u16 color; u8 layer; u8 priority; } LayerPixel;
        LayerPixel pixels[9]; // Max 4 BGs + 4 OBJ priorities + 1 backdrop
        int pixel_count = 0;
        
        // Add background layers
        for (int bg = 0; bg < 4; bg++) {
            if (!bg_lines[bg][x].transparent) {
                pixels[pixel_count].color = bg_lines[bg][x].color;
                pixels[pixel_count].layer = bg_lines[bg][x].layer;
                pixels[pixel_count].priority = bg_lines[bg][x].priority;
                pixel_count++;
            }
        }
        
        // Add sprite layers
        for (int p = 0; p < 4; p++) {
            if (!obj_line[p][x].transparent) {
                pixels[pixel_count].color = obj_line[p][x].color;
                pixels[pixel_count].layer = LAYER_OBJ;
                pixels[pixel_count].priority = p;
                pixel_count++;
            }
        }
        
        // Sort by priority (lower number = higher priority), then by layer
        for (int i = 0; i < pixel_count - 1; i++) {
            for (int j = i + 1; j < pixel_count; j++) {
                bool swap = false;
                if (pixels[i].priority > pixels[j].priority) {
                    swap = true;
                } else if (pixels[i].priority == pixels[j].priority) {
                    // Within same priority, OBJ > BG0 > BG1 > BG2 > BG3
                    if (pixels[i].layer == LAYER_OBJ && pixels[j].layer != LAYER_OBJ) {
                        swap = false;
                    } else if (pixels[i].layer != LAYER_OBJ && pixels[j].layer == LAYER_OBJ) {
                        swap = true;
                    } else if (pixels[i].layer > pixels[j].layer) {
                        swap = true;
                    }
                }
                
                if (swap) {
                    LayerPixel temp = pixels[i];
                    pixels[i] = pixels[j];
                    pixels[j] = temp;
                }
            }
        }
        
        // Get top pixel
        u16 final_color;
        if (pixel_count == 0) {
            final_color = backdrop;
        } else {
            final_color = pixels[0].color;
            
            // Apply blending effects
            if (blend_mode == 1 && pixel_count >= 2) {
                // Alpha blend
                if (target1[pixels[0].layer] && target2[pixels[1].layer]) {
                    final_color = alpha_blend(pixels[0].color, pixels[1].color, eva, evb);
                }
            } else if (blend_mode == 2 && target1[pixels[0].layer]) {
                // Brightness increase
                final_color = brightness_adjust(pixels[0].color, evy, true);
            } else if (blend_mode == 3 && target1[pixels[0].layer]) {
                // Brightness decrease
                final_color = brightness_adjust(pixels[0].color, evy, false);
            }
        }
        
        output[x] = convert_color(final_color);
    }
}

void gfx_init(GFXState *gfx) {
    if (!gfx) return;
    memset(gfx->framebuffer, 0, sizeof(gfx->framebuffer));
    gfx->dirty = true;
    gfx->show_debug = true;
}

void gfx_render_frame(GFXState *gfx, Memory *mem) {
    if (!gfx || !mem) return;
    
    // Read display control registers
    ScanlineContext ctx;
    ctx.dispcnt = mem_read16(mem, 0x04000000);
    ctx.bldcnt = mem_read16(mem, 0x04000050);
    ctx.bldalpha = mem_read16(mem, 0x04000052);
    ctx.bldy = mem_read16(mem, 0x04000054);
    
    // If display is disabled (DISPCNT=0), show a test pattern with message
    if (ctx.dispcnt == 0) {
        u16 backdrop = mem_read16(mem, 0x05000000);
        // Convert BGR555 to RGB565
        u8 r = ((backdrop >> 0) & 0x1F) << 3;
        u8 g = ((backdrop >> 5) & 0x1F) << 3;
        u8 b = ((backdrop >> 10) & 0x1F) << 3;
        u16 rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        
        // Add a simple grid pattern so user can see the display is working
        for (int y = 0; y < GBA_SCREEN_HEIGHT; y++) {
            for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
                u16 color = rgb565;
                // Draw a border and grid pattern
                if (x == 0 || x == GBA_SCREEN_WIDTH-1 || y == 0 || y == GBA_SCREEN_HEIGHT-1) {
                    color = 0xF800; // Red border
                } else if ((x % 40 == 0) || (y % 40 == 0)) {
                    color = 0x001F; // Blue grid
                }
                gfx->framebuffer[y * GBA_SCREEN_WIDTH + x] = color;
            }
        }
        gfx->dirty = true;
        return;
    }
    
    // If forced blank mode (bit 7 set), render white screen with message
    if (ctx.dispcnt & 0x80) {
        // Fill with white for forced blank
        for (int i = 0; i < GBA_FRAMEBUFFER_SIZE; i++) {
            gfx->framebuffer[i] = 0xFFFF; // White
        }
        gfx->dirty = true;
        return;
    }
    
    u8 mode = ctx.dispcnt & 0x7;
    
    // Read BG control registers
    for (int i = 0; i < 4; i++) {
        ctx.bg_cnt[i] = mem_read16(mem, 0x04000008 + i * 2);
        ctx.bg_hofs[i] = mem_read16(mem, 0x04000010 + i * 4) & 0x1FF;
        ctx.bg_vofs[i] = mem_read16(mem, 0x04000012 + i * 4) & 0x1FF;
    }
    
    // Read affine parameters for BG2/BG3
    ctx.bg_pa[0] = mem_read16(mem, 0x04000020);
    ctx.bg_pb[0] = mem_read16(mem, 0x04000022);
    ctx.bg_pc[0] = mem_read16(mem, 0x04000024);
    ctx.bg_pd[0] = mem_read16(mem, 0x04000026);
    ctx.bg_x[0] = (s32)(mem_read32(mem, 0x04000028) << 4) >> 4; // Sign extend 28-bit
    ctx.bg_y[0] = (s32)(mem_read32(mem, 0x0400002C) << 4) >> 4;
    
    ctx.bg_pa[1] = mem_read16(mem, 0x04000030);
    ctx.bg_pb[1] = mem_read16(mem, 0x04000032);
    ctx.bg_pc[1] = mem_read16(mem, 0x04000034);
    ctx.bg_pd[1] = mem_read16(mem, 0x04000036);
    ctx.bg_x[1] = (s32)(mem_read32(mem, 0x04000038) << 4) >> 4;
    ctx.bg_y[1] = (s32)(mem_read32(mem, 0x0400003C) << 4) >> 4;
    
    // Get backdrop color
    u16 backdrop = mem_read16(mem, 0x05000000);
    
    // Render each scanline
    for (int scanline = 0; scanline < GBA_SCREEN_HEIGHT; scanline++) {
        Pixel bg_lines[4][GBA_SCREEN_WIDTH];
        Pixel obj_line[4][GBA_SCREEN_WIDTH];
        
        // Initialize all pixels as transparent
        for (int bg = 0; bg < 4; bg++) {
            for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
                bg_lines[bg][x].transparent = true;
            }
        }
        
        // Render based on mode
        if (mode == 0) {
            // Mode 0: Text BG0-3
            for (int bg = 0; bg < 4; bg++) {
                if (ctx.dispcnt & (DISPCNT_BG0_ON << bg)) {
                    render_text_bg_scanline(&ctx, mem, bg, scanline, bg_lines[bg]);
                }
            }
        } else if (mode == 1) {
            // Mode 1: Text BG0-1, Affine BG2
            if (ctx.dispcnt & DISPCNT_BG0_ON) {
                render_text_bg_scanline(&ctx, mem, 0, scanline, bg_lines[0]);
            }
            if (ctx.dispcnt & DISPCNT_BG1_ON) {
                render_text_bg_scanline(&ctx, mem, 1, scanline, bg_lines[1]);
            }
            if (ctx.dispcnt & DISPCNT_BG2_ON) {
                render_affine_bg_scanline(&ctx, mem, 2, scanline, bg_lines[2]);
            }
        } else if (mode == 2) {
            // Mode 2: Affine BG2-3
            if (ctx.dispcnt & DISPCNT_BG2_ON) {
                render_affine_bg_scanline(&ctx, mem, 2, scanline, bg_lines[2]);
            }
            if (ctx.dispcnt & DISPCNT_BG3_ON) {
                render_affine_bg_scanline(&ctx, mem, 3, scanline, bg_lines[3]);
            }
        } else if (mode == 3 || mode == 4 || mode == 5) {
            // Mode 3-5: Bitmap modes (BG2 only)
            if (ctx.dispcnt & DISPCNT_BG2_ON) {
                render_bitmap_bg_scanline(&ctx, mem, mode, scanline, bg_lines[2]);
            }
        }
        
        // Render sprites
        if (ctx.dispcnt & DISPCNT_OBJ_ON) {
            render_sprites_scanline(&ctx, mem, scanline, obj_line);
        }
        
        // Compose final scanline with blending
        u16 *output = &gfx->framebuffer[scanline * GBA_SCREEN_WIDTH];
        compose_scanline(&ctx, bg_lines, obj_line, backdrop, output);
        
        // Update affine background reference points for next scanline
        ctx.bg_x[0] += ctx.bg_pc[0];
        ctx.bg_y[0] += ctx.bg_pd[0];
        ctx.bg_x[1] += ctx.bg_pc[1];
        ctx.bg_y[1] += ctx.bg_pd[1];
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

void gfx_draw_debug_info(GFXState *gfx, Memory *mem, u32 pc, u32 sp, u32 lr, u32 cpsr, bool thumb,
                         u16 ie, u16 if_flag, u16 ime, u64 frame_count) {
    if (!gfx || !mem || !gfx->show_debug) return;
    
    u16 white = 0xFFFF;
    u16 yellow = 0xFFE0;
    u16 green = 0x07E0;
    u16 red = 0xF800;
    u16 cyan = 0x07FF;
    
    char buf[64];
    
    // Top-left: Frame count and LCD status
    draw_box(gfx, 0, 0, 110, 50, white);
    snprintf(buf, sizeof(buf), "F %06llu", (unsigned long long)(frame_count % 1000000));
    draw_text(gfx, 2, 2, buf, yellow);
    
    u16 dispcnt = mem_read16(mem, 0x04000000);
    snprintf(buf, sizeof(buf), "LCD %04X", dispcnt);
    draw_text(gfx, 2, 8, buf, dispcnt ? green : red);
    
    snprintf(buf, sizeof(buf), "MODE %d", dispcnt & 7);
    draw_text(gfx, 2, 14, buf, white);
    
    u8 bg_on = 0;
    if (dispcnt & 0x0100) bg_on |= 1;
    if (dispcnt & 0x0200) bg_on |= 2;
    if (dispcnt & 0x0400) bg_on |= 4;
    if (dispcnt & 0x0800) bg_on |= 8;
    snprintf(buf, sizeof(buf), "BG %d%d%d%d", 
             (bg_on & 1) ? 0 : 0,
             (bg_on & 2) ? 1 : 0, 
             (bg_on & 4) ? 2 : 0,
             (bg_on & 8) ? 3 : 0);
    draw_text(gfx, 2, 20, buf, bg_on ? green : red);
    
    snprintf(buf, sizeof(buf), "OBJ %s", (dispcnt & 0x1000) ? "ON" : "OFF");
    draw_text(gfx, 2, 26, buf, (dispcnt & 0x1000) ? green : white);
    
    // Top-right: CPU state
    draw_box(gfx, 130, 0, 110, 56, white);
    snprintf(buf, sizeof(buf), "PC %08X", pc);
    draw_text(gfx, 132, 2, buf, cyan);
    
    snprintf(buf, sizeof(buf), "SP %08X", sp);
    draw_text(gfx, 132, 8, buf, white);
    
    snprintf(buf, sizeof(buf), "LR %08X", lr);
    draw_text(gfx, 132, 14, buf, white);
    
    snprintf(buf, sizeof(buf), "%s %s%s%s%s",
             thumb ? "THM" : "ARM",
             (cpsr & 0x80000000) ? "N" : "",
             (cpsr & 0x40000000) ? "Z" : "",
             (cpsr & 0x20000000) ? "C" : "",
             (cpsr & 0x10000000) ? "V" : "");
    draw_text(gfx, 132, 20, buf, yellow);
    
    const char *mode_names[] = {"USR", "FIQ", "IRQ", "SVC", "ABT", "UND", "SYS"};
    int mode_idx = cpsr & 0x1F;
    if (mode_idx == 0x10) mode_idx = 0;
    else if (mode_idx == 0x11) mode_idx = 1;
    else if (mode_idx == 0x12) mode_idx = 2;
    else if (mode_idx == 0x13) mode_idx = 3;
    else if (mode_idx == 0x17) mode_idx = 4;
    else if (mode_idx == 0x1B) mode_idx = 5;
    else if (mode_idx == 0x1F) mode_idx = 6;
    else mode_idx = 0;
    
    snprintf(buf, sizeof(buf), "MODE %s", mode_names[mode_idx]);
    draw_text(gfx, 132, 26, buf, white);
    
    // Interrupt status
    snprintf(buf, sizeof(buf), "IE %04X", ie);
    draw_text(gfx, 132, 32, buf, ie ? green : white);
    
    snprintf(buf, sizeof(buf), "IF %04X", if_flag);
    draw_text(gfx, 132, 38, buf, if_flag ? red : white);
    
    snprintf(buf, sizeof(buf), "IME %d", ime);
    draw_text(gfx, 132, 44, buf, ime ? green : white);
    
    // Bottom-left: Memory and input
    draw_box(gfx, 0, GBA_SCREEN_HEIGHT - 26, 120, 26, white);
    
    snprintf(buf, sizeof(buf), "VRAM %08X", 0x06000000);
    draw_text(gfx, 2, GBA_SCREEN_HEIGHT - 24, buf, white);
    
    u16 vcount = mem_read16(mem, 0x04000006);
    snprintf(buf, sizeof(buf), "VCOUNT %03d", vcount);
    draw_text(gfx, 2, GBA_SCREEN_HEIGHT - 18, buf, cyan);
    
    u8 ai_input = mem_get_ai_input(mem);
    snprintf(buf, sizeof(buf), "INPUT %02X", ai_input);
    draw_text(gfx, 2, GBA_SCREEN_HEIGHT - 12, buf, ai_input ? yellow : white);
    
    snprintf(buf, sizeof(buf), "AI %08X", 0x0203CF64);
    draw_text(gfx, 2, GBA_SCREEN_HEIGHT - 6, buf, white);
}
