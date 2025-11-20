# Building the Custom Emulator

## Quick Start (Windows with vcpkg)

```powershell
# Navigate to emulator directory
cd E:\pokeplayer\source\pokeemerald-master\tools\custom_emulator

# Create build directory
mkdir build
cd build

# Configure with vcpkg
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . --config Release

# Run
.\Release\pokemon_emu.exe ..\..\pokeemerald.gba
```

## What You'll See

Currently, the emulator:
- ✅ Loads the ROM file (`pokeemerald.gba`)
- ✅ Parses and displays ROM header information
- ✅ Opens a 480x320 window (2x scale)
- ✅ Renders a test pattern (gradient)
- ✅ Accepts keyboard input mapped to GBA buttons
- ✅ Writes AI input to `gAiInputState` (0x0203cf64)
- ✅ Runs at 60 FPS

## Keyboard Controls

- **Z** = A button
- **X** = B button
- **Arrow Keys** = D-Pad
- **Enter** = Start
- **Right Shift** = Select
- **ESC** = Quit

## Current Limitations

⚠️ **CPU Emulation Not Implemented**

The CPU core is a stub. To actually run Pokemon Emerald, you need to:

### Option 1: Use Existing ARM7TDMI Core (Recommended)

Extract the CPU emulator from an existing project:
- **mGBA** - Best accuracy, modern codebase
- **VisualBoyAdvance** - Classic, well-tested
- **SkyEmu** - Lightweight, easy to integrate

### Option 2: Implement Your Own

See `cpu_core.c` for the structure. You need to implement:
1. ARM instruction decoder (~50 most common instructions)
2. Thumb instruction decoder (~30 most common)
3. Condition code evaluation
4. Barrel shifter operations
5. Load/Store with addressing modes
6. Branch and link instructions
7. Interrupt handling

### Option 3: Use Dynamic Recompilation (JIT)

For maximum speed (1000x real-time):
- **Dynarmic** - Modern ARM JIT compiler
- **Lightning** - Fast portable JIT
- Custom JIT with basic block caching

## Integration with RL Training

Even without CPU emulation, you can:

1. **Test the infrastructure:**
   ```c
   // Write AI input
   mem_set_ai_input(&memory, KEY_A | KEY_UP);
   
   // Read back
   u8 input = mem_get_ai_input(&memory);
   ```

2. **Build Python bridge:**
   ```python
   import ctypes
   
   lib = ctypes.CDLL("pokemon_emu_lib.dll")
   lib.mem_set_ai_input(mem_ptr, 0x01)
   ```

3. **Prepare Gym environment:**
   ```python
   class PokemonEnv(gym.Env):
       def __init__(self):
           self.emu = PokemonEmulator()
       
       def step(self, action):
           self.emu.set_input(action)
           self.emu.frame()
           return self.emu.get_state()
   ```

## Next Steps

### To Get Full Emulation Working:

1. **Integrate mGBA's CPU core** (easiest path):
   ```bash
   git clone https://github.com/mgba-emu/mgba
   # Extract arm/cpu.c and dependencies
   # Add to CMakeLists.txt
   ```

2. **Or implement minimal CPU** (educational):
   - Start with the 20 most common ARM instructions
   - Pokemon Emerald uses mostly Thumb mode
   - Focus on: B, BL, LDR, STR, ADD, SUB, MOV, CMP, BX

3. **Test with simple ROM first**:
   - Use a Hello World GBA ROM
   - Verify CPU executes correctly
   - Then try Pokemon Emerald

### Performance Optimization:

Once CPU works:
- Add JIT compilation for speed
- Implement save states properly
- Add headless mode (no rendering)
- Optimize memory access patterns
- Cache decoded instructions

## Resources

- **GBATek**: https://problemkaputt.de/gbatek.htm (Hardware reference)
- **GBADEV**: https://gbadev.net/ (Homebrew community)
- **mGBA Source**: https://github.com/mgba-emu/mgba
- **ARM7TDMI Manual**: Official ARM documentation

## Project Status

| Component | Status | Notes |
|-----------|--------|-------|
| ROM Loader | ✅ Complete | Loads and verifies pokeemerald.gba |
| Memory System | ✅ Complete | Full GBA memory map implemented |
| **CPU Core** | ❌ Stub Only | **Needs implementation** |
| Graphics | ⚠️ Partial | Test pattern works, tile rendering needed |
| Input | ✅ Complete | Keyboard + AI input working |
| Save States | ❌ Stub | Easy to add once CPU works |
| Python Bridge | ❌ Stub | Add ctypes exports |

## Estimated Effort

- **Using existing CPU core**: 1-2 days
- **Implementing minimal CPU**: 1-2 weeks
- **Full accuracy CPU**: 1-2 months
- **With JIT compiler**: 2-3 months

## Why Build This?

Even without full CPU emulation, this framework provides:
- ✅ Direct memory access to game state
- ✅ AI input hook at correct address
- ✅ Foundation for fast training
- ✅ Learning exercise for emulator development
- ✅ Custom instrumentation and debugging

For production RL training, I recommend using **mGBA with Lua scripts** or **BizHawk** until the CPU core is complete.
