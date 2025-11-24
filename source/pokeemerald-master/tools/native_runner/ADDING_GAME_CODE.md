# Adding Game Code - Incremental Integration Guide

The native runner now has a clean integration point via `game_init.c`. This file acts as a bridge between the native SDL runner and actual game logic.

## Current State

✅ **Working:**
- SDL2 window and rendering
- 60fps game loop with VBlank timing
- AI policy hooks (called every frame)
- 2 battle controller source files compiled
- 500+ stub functions for missing game systems
- Framebuffer rendering pipeline

## How to Add More Game Code

### Step 1: Add Source Files to CMakeLists.txt

Edit `tools/native_runner/CMakeLists.txt` and add files to the `BUILD_GAME_CORE` section:

```cmake
if(BUILD_GAME_CORE)
    target_sources(native_runner PRIVATE
        ${CMAKE_SOURCE_DIR}/../../src/battle_controllers.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_controller_player.c
        # Add new files here:
        # ${CMAKE_SOURCE_DIR}/../../src/graphics.c
        # ${CMAKE_SOURCE_DIR}/../../src/bg.c
        # etc.
    )
endif()
```

### Step 2: Build and Capture Missing Symbols

```powershell
cmake --build . --config Release 2>&1 | Select-String "unresolved external"
```

### Step 3: Add Stubs for Missing Symbols

Add missing functions/globals to `game_stubs.c`:

```c
// Example stub for a missing function
void MissingFunction(int arg) {
    // Stub: no-op
}

// Example stub for a missing global
u32 gMissingGlobal = 0;
```

### Step 4: Implement Game Init/Update/Render

Edit `game_init.c` to call actual game functions:

```c
#include "overworld.h"   // Once you add overworld.c
#include "graphics.h"     // Once you add graphics.c

void game_init(void) {
    // Initialize subsystems as you add them
    // InitGraphics();
    // InitOverworld();
}

void game_update(void) {
    // Call game logic
    // UpdateOverworld();
    // UpdateBattleSystem();
}

void game_render(u16 *framebuffer) {
    // Render game state to framebuffer
    // RenderOverworld(framebuffer);
    // or copy from GBA VRAM if implemented
}
```

## Recommended Addition Order

Add subsystems in this order to minimize dependencies:

1. **Core utilities** (`src/random.c`, `src/util.c`)
2. **Graphics layer** (`src/bg.c`, `src/gpu_regs.c`, `src/graphics.c`)
3. **Input system** (already shimmed in `gba_shim.c`)
4. **Text/menus** (`src/text.c`, `src/menu.c`)
5. **Sprites** (`src/sprite.c`, `src/sprite_anim.c`)
6. **Overworld** (`src/overworld.c`, `src/field_player_avatar.c`)
7. **Battle system** (already have battle_controllers.c)
8. **Pokemon data** (`src/pokemon.c`, `src/pokemon_summary_screen.c`)

## Testing Each Addition

After adding each file:

1. **Build** - Fix compile errors with more stubs
2. **Link** - Add missing symbol stubs to `game_stubs.c`
3. **Run** - Test that the window still opens and renders
4. **Call** - Add function calls to `game_init.c` once it links

## Example: Adding Graphics System

```cmake
# CMakeLists.txt
target_sources(native_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/../../src/bg.c
    ${CMAKE_SOURCE_DIR}/../../src/gpu_regs.c
)
```

```c
// game_init.c
#include "bg.h"
#include "gpu_regs.h"

void game_init(void) {
    InitGpuRegManager();
    ResetBgs();
}

void game_render(u16 *framebuffer) {
    // Copy from GBA BG VRAM to framebuffer
    // (implementation depends on how you shim the BG system)
}
```

## Current Stubs

See `game_stubs.c` - contains 500+ placeholder functions including:
- Battle system (complete)
- Link/multiplayer (all stubbed out)
- Graphics/sprites (basic stubs)
- Sound/music (all stubbed)
- Save system (stubbed)
- Pokemon data (stubbed)

## Running the Native Runner

```powershell
cd tools\native_runner\build
.\Release\native_runner.exe
```

Currently shows a solid blue screen (placeholder from `game_render()`). As you implement more systems, this will show actual game graphics.

## Notes

- The game will NOT fully work without ALL its dependencies compiled
- Start with isolated systems (graphics, input) before complex ones (battle, overworld)
- Many functions expect GBA hardware registers - you'll need to shim or stub these
- Some files pull in TONS of dependencies (e.g., `main.c` needs 40+ subsystems)
- Focus on getting a minimal vertical slice working (e.g., render a single screen) before expanding
