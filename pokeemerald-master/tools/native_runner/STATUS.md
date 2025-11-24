# Native x86 Runner - Project Status

## What Works Currently

### ✅ Infrastructure Complete
- **Native Windows x86 executable** compiles and runs
- **SDL2 window** opens at 240x160 with proper aspect ratio
- **60fps rendering** with VBlank timing
- **Test pattern renders** (animated green gradient confirming rendering pipeline)
- **GBA compatibility shims** for MSVC compilation
- **AI integration hooks** ready for policy execution
- **Fixed-point MLP inference** runtime implemented

### ✅ Development Environment
- CMake 4.2.0 configured
- Visual Studio Build Tools 2022 (MSVC 19.44)
- SDL2 2.32.10 installed via vcpkg
- MSVC compatibility layer (`gba_compat.h`) with forced-include

### ✅ Architecture Designed
```
[SDL2 Event Loop] → native_main.c
    ↓
[game_init() / game_update() / game_render()] → Integration Layer
    ↓
[Real Game Functions] (InitHeap, ResetBgs, RunTasks, etc.)
    ↓
[500+ Stub Functions] → game_stubs.c
    ↓
[GBA Shims] (VBlankIntrWait, ReadKeyInput)
    ↓
[SDL Framebuffer] → Screen Output

[policy_tick()] → AI System
    ↓
[mlp_predict()] → Fixed-Point Inference
```

## What Doesn't Work

### ❌ Full Game Compilation Blocked
**Problem**: The game uses GCC-specific build tools that don't work with MSVC:

1. **Text macros** - `_("text")` macro for internationalization generates compile-time strings (not MSVC compatible)
2. **INCBIN macros** - `INCBIN_U8/U16/U32` embed binary data at compile time (GCC-only)
3. **Generated headers** - Missing `constants/map_event_ids.h` and other build-generated files
4. **Void pointer arithmetic** - Code uses `void *` arithmetic (C99 GCC extension, not standard C)
5. **Designated initializers with function calls** - MSVC C mode requires constant initializers

**Error Examples**:
```
error C2099: initializer is not a constant
  const u8 *text = _("Ability text");  // Macro call not allowed in initializer

error C1083: Cannot open include file: 'constants/map_event_ids.h'
  // Build tool should generate this from map data

error C2036: 'void *': unknown size
  ptr + 10;  // void pointer arithmetic
```

### Current Approach Limitations

#### Attempted: Compile Full Game
- Added ~90 game source files to CMakeLists.txt
- Result: **Hundreds of MSVC incompatibility errors**
- Files affected: `abilities.h`, `item_descriptions.h`, `battle_message.c`, `intro.c`, etc.

#### Why Full Compilation Won't Work:
1. Game designed for **GCC + custom preprocessor tools**
2. Uses `preproc`, `gfx`, `jsonproc` build tools to generate C code
3. Text system relies on compile-time macro expansion
4. Graphics data embedded via INCBIN (binary includes)
5. Map constants generated from JSON/event data

## Recommended Path Forward

### Option 1: Hybrid Approach (Recommended)
**Use emulator for game logic, native runner for AI/rendering:**

1. **Keep working native runner** with test pattern
2. **Run actual game in emulator** (mGBA/VBA-M)
3. **Bridge via memory sharing:**
   - Emulator exposes RAM via shared memory
   - Native runner reads game state
   - AI makes decisions
   - Actions injected back to emulator

**Advantages:**
- Game runs with full compatibility
- AI runs natively (fast inference)
- No need to port entire codebase
- Already have `tools/rl_agent/emulator_bridge_lua.lua` template

**Files to Use:**
- `tools/rl_agent/gym_env.py` - RL environment
- `tools/rl_agent/train.py` - Training harness
- `tools/rl_agent/emulator_bridge_lua.lua` - Lua bridge for VBA-M
- `tools/native_runner/policy_example.c` - AI policy (can still use this for standalone testing)

### Option 2: Minimal Native Core
**Port only essential systems needed for training:**

1. **Keep:** Battle system logic (damage calc, type effectiveness)
2. **Keep:** Pokemon stats/moves data structures
3. **Skip:** Graphics, sound, overworld, menus, text
4. **Create:** Minimal battle simulator in pure C

**Estimated Effort:** Medium - requires extracting and adapting battle logic

### Option 3: Build Tool Chain
**Set up full pokeemerald build environment:**

1. Install WSL2 with Ubuntu
2. Follow `INSTALL.md` to set up GCC ARM toolchain
3. Build ROM normally
4. Extract needed .c files after preprocessing
5. Attempt recompilation with MSVC

**Estimated Effort:** High - complex build dependencies

## Current File Structure

```
tools/
├── native_runner/          # Native x86 Windows runner
│   ├── native_main.c       # SDL loop, working ✅
│   ├── game_init.c         # Integration layer ✅
│   ├── gba_shim.c/h        # VBlank, input shims ✅
│   ├── gba_compat.h        # MSVC compatibility ✅
│   ├── game_stubs.c        # 500+ stub functions ✅
│   ├── policy_example.c    # AI hooks ✅
│   ├── mlp_inference.c     # Fixed-point inference ✅
│   ├── export_mlp_to_c.py  # Model exporter ✅
│   ├── CMakeLists.txt      # Build config ✅
│   └── STATUS.md           # This file
│
├── rl_agent/              # RL training infrastructure
│   ├── gym_env.py          # Gym environment ✅
│   ├── train.py            # Training harness ✅
│   └── emulator_bridge_lua.lua  # VBA-M bridge ✅
│
├── include/ai_input.h      # ROM-side AI hooks (unused in native)
└── src/ai_input.c          # (unused in native)
```

## Next Steps (Choose One)

### A. Emulator + Native AI (Fastest to Results)
1. Install VBA-M or mGBA with Lua support
2. Implement `emulator_bridge_lua.lua` to read RAM
3. Set up shared memory communication
4. Train RL agent with `gym_env.py` + `train.py`
5. Use native `mlp_inference.c` for fast inference

### B. Minimal Battle Simulator
1. Extract Pokemon data structures
2. Port battle damage calculation
3. Create headless battle simulator
4. Train on battle decisions only
5. Expand to full game later

### C. Full WSL Build (Longest Path)
1. Set up WSL2 + pokeemerald build tools
2. Compile ROM to verify
3. Preprocess source files
4. Attempt MSVC port of preprocessed code
5. Debug thousands of errors

## Key Learnings

1. **GBA homebrew ≠ portable C** - Heavy reliance on GCC extensions and custom tools
2. **MSVC C mode is strict** - No compound literals, designated initializers must be constant
3. **Build infrastructure matters** - Game needs `preproc`, `gfx`, `jsonproc` tools
4. **Emulator bridge is pragmatic** - Don't reinvent wheels that already turn
5. **AI can still be native** - Policy inference doesn't need full game port

## Working Demo

Current executable location:
```
tools/native_runner/build/Release/native_runner.exe  (if SDL2/vcpkg was configured)
```

What it does:
- Opens 240x160 SDL window
- Renders animated green gradient at 60fps
- Calls `game_init()` which invokes real game functions
- Executes `policy_tick()` for AI integration
- Waits for VBlank (16ms delays)
- Handles SDL events

## Resources

- **pokeemerald Documentation**: `INSTALL.md`, `README.md`
- **RL Training Guide**: `tools/rl_agent/README.md` (if created)
- **Integration Guide**: `tools/native_runner/ADDING_GAME_CODE.md`
- **Compatibility Layer**: `tools/native_runner/gba_compat.h`

---

**Last Updated**: 2025-01-20  
**Status**: Infrastructure complete, full game compilation blocked by toolchain incompatibility  
**Recommendation**: Use emulator bridge approach for fastest RL training results
