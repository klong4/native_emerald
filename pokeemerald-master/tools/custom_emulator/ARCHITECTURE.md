# Custom Pokemon Emerald Emulator Architecture

## Overview

A specialized GBA emulator designed specifically for Pokemon Emerald AI/RL training. Optimized for fast iteration, easy state extraction, and direct AI integration.

## Design Philosophy

**Purpose-Built, Not General:**
- Only implement features Pokemon Emerald actually uses
- Skip unused GBA capabilities (Mode 7, HBlank effects, etc.)
- Optimize for training speed over accuracy
- Direct memory access for state extraction
- Native AI policy integration

**Hybrid Approach:**
- Load and execute the actual ROM we just built (`pokeemerald.gba`)
- Emulate ARM7TDMI CPU instructions
- Render GBA graphics natively with SDL2
- Expose game state directly to Python/RL framework
- No need to port C code - run the real game!

## Architecture Layers

```
┌─────────────────────────────────────────────────────────┐
│                  Python RL Environment                   │
│              (Stable-Baselines3 / Custom)                │
└────────────────────┬────────────────────────────────────┘
                     │ Shared Memory / IPC
                     ↓
┌─────────────────────────────────────────────────────────┐
│              C/C++ Emulator Core (This!)                 │
├──────────────────┬──────────────────────────────────────┤
│  CPU Emulation   │  Memory Management                    │
│  - ARM7TDMI      │  - ROM (16MB)                        │
│  - Thumb mode    │  - EWRAM (256KB) ← gAiInputState     │
│  - ARM mode      │  - IWRAM (32KB)                       │
│                  │  - IO Registers                       │
├──────────────────┼──────────────────────────────────────┤
│ Graphics Engine  │  Input System                         │
│  - BG layers 0-3 │  - Keyboard mapping                   │
│  - OBJ sprites   │  - AI input injection (0x0203cf64)    │
│  - Palettes      │  - Save states                        │
│  - VRAM mgmt     │                                       │
├──────────────────┼──────────────────────────────────────┤
│  Audio (Stub)    │  Timers & Interrupts                  │
│  - Just enough   │  - VBlank                             │
│    to not crash  │  - HBlank (minimal)                   │
│                  │  - Timer 0-3                          │
└──────────────────┴──────────────────────────────────────┘
         │                          │
         ↓                          ↓
    SDL2 Video                 State Export
    (240x160)                  (JSON/Binary)
```

## Core Components

### 1. CPU Emulator (`cpu_core.c`)
```c
typedef struct {
    uint32_t r[16];      // R0-R15 (R15 is PC)
    uint32_t cpsr;       // Current Program Status Register
    uint32_t spsr;       // Saved PSR
    bool thumb_mode;     // Thumb vs ARM instruction set
    uint64_t cycles;     // Total cycles executed
} ARM7TDMI;

// Main execution loop
void cpu_execute_frame(ARM7TDMI *cpu, Memory *mem);
uint32_t cpu_fetch_decode_execute(ARM7TDMI *cpu, Memory *mem);
```

**What to Implement:**
- ~50 most common ARM/Thumb instructions (not all 200+)
- Branch, load/store, arithmetic, logical operations
- Enough for Pokemon Emerald's compiled code to run

**What to Skip:**
- Coprocessor instructions (CP15) - not used
- Complex SIMD operations - unnecessary
- Perfect timing accuracy - training doesn't need cycle-perfect

### 2. Memory System (`memory.c`)
```c
typedef struct {
    uint8_t *rom;           // 16MB ROM (pokeemerald.gba)
    uint8_t ewram[256*1024]; // External WRAM
    uint8_t iwram[32*1024];  // Internal WRAM
    uint8_t vram[96*1024];   // Video RAM
    uint8_t oam[1024];       // Object Attribute Memory
    uint8_t palette[1024];   // Palette RAM
    uint8_t io_regs[1024];   // I/O Registers
} Memory;

// Memory-mapped I/O
uint32_t mem_read32(Memory *mem, uint32_t addr);
void mem_write32(Memory *mem, uint32_t addr, uint32_t value);
uint8_t mem_read8(Memory *mem, uint32_t addr);
void mem_write8(Memory *mem, uint32_t addr, uint8_t value);
```

**Memory Map (GBA):**
```
0x00000000 - 0x00003FFF : BIOS (16KB) - can stub or use real BIOS
0x02000000 - 0x0203FFFF : EWRAM (256KB) ← gAiInputState @ 0x0203cf64
0x03000000 - 0x03007FFF : IWRAM (32KB)
0x04000000 - 0x040003FF : I/O Registers
0x05000000 - 0x050003FF : Palette RAM (1KB)
0x06000000 - 0x06017FFF : VRAM (96KB)
0x07000000 - 0x070003FF : OAM (1KB)
0x08000000 - 0x09FFFFFF : ROM (32MB max, Emerald uses 16MB)
```

### 3. Graphics Renderer (`gfx_renderer.c`)
```c
typedef struct {
    uint16_t framebuffer[240 * 160]; // RGB555 format
    bool layers_enabled[4];
    bool sprites_enabled;
    uint8_t bg_mode;
} GFXState;

// Rendering pipeline
void gfx_render_frame(GFXState *gfx, Memory *mem, uint16_t *output);
void gfx_render_background(int layer, Memory *mem, uint16_t *fb);
void gfx_render_sprites(Memory *mem, uint16_t *fb);
void gfx_apply_palette(uint8_t pal_index, uint16_t *color);
```

**Pokemon Emerald Uses:**
- BG Mode 0 (tiled backgrounds)
- 4 background layers
- Hardware sprites (OBJ)
- 16-color and 256-color palettes

**Rendering Order:**
1. Clear framebuffer
2. Render BG0-BG3 (back to front based on priority)
3. Render sprites (OBJ) with priority sorting
4. Apply palette lookup
5. Output to SDL2 texture

### 4. Input System (`input.c`)
```c
typedef struct {
    uint16_t current_keys;  // Current frame key state
    uint16_t previous_keys; // Previous frame (for edge detection)
} InputState;

// GBA key bits (active LOW in real hardware, but we'll use active HIGH)
#define KEY_A       (1 << 0)
#define KEY_B       (1 << 1)
#define KEY_SELECT  (1 << 2)
#define KEY_START   (1 << 3)
#define KEY_RIGHT   (1 << 4)
#define KEY_LEFT    (1 << 5)
#define KEY_UP      (1 << 6)
#define KEY_DOWN    (1 << 7)
#define KEY_R       (1 << 8)
#define KEY_L       (1 << 9)

void input_update(InputState *input, uint8_t ai_input);
uint16_t input_get_keys(InputState *input);
```

**AI Integration:**
```c
// Read AI input from gAiInputState in EWRAM
uint8_t ai_buttons = mem->ewram[0x3cf64]; // Offset from 0x02000000
input_state.current_keys = ai_buttons;
```

### 5. State Management (`save_state.c`)
```c
typedef struct {
    ARM7TDMI cpu;
    Memory memory;
    GFXState gfx;
    InputState input;
    uint64_t frame_number;
} EmulatorState;

void save_state(EmulatorState *emu, const char *filename);
void load_state(EmulatorState *emu, const char *filename);
```

**For RL Training:**
- Save state at key checkpoints (start of route, before gym, etc.)
- Load state to reset episodes
- Fast state serialization (memcpy entire struct)

### 6. ROM Loader (`rom_loader.c`)
```c
bool load_rom(Memory *mem, const char *rom_path);
bool verify_rom_header(uint8_t *rom);
void parse_rom_header(uint8_t *rom, ROMInfo *info);
```

**ROM Header (offset 0xA0):**
- Game title: "POKEMON EMER"
- Game code: "BPEE"
- Maker code: "01"
- Checksum validation

## Python Integration

### Shared Memory Approach
```python
import mmap
import struct
import numpy as np

class CustomEmulatorBridge:
    def __init__(self, shared_mem_name="pokemon_emu"):
        self.shm = mmap.mmap(-1, 4096, shared_mem_name)
    
    def read_memory(self, address, size):
        # Request memory read from emulator
        cmd = struct.pack('<BII', 0x01, address, size)
        self.shm.seek(0)
        self.shm.write(cmd)
        # Read response
        response = self.shm.read(size)
        return response
    
    def write_ai_input(self, buttons):
        # Write directly to gAiInputState
        self.write_byte(0x0203cf64, buttons)
    
    def get_framebuffer(self):
        # Read framebuffer from shared memory
        fb = np.frombuffer(self.shm, dtype=np.uint16, count=240*160)
        return fb.reshape(160, 240)
    
    def step(self):
        # Advance one frame
        self.shm.seek(1000)
        self.shm.write(b'\x01')  # Step command
        # Wait for completion
        while self.shm[1001] != b'\x00':
            time.sleep(0.001)
```

### Socket/Pipe Approach (Alternative)
```python
import socket

class EmulatorSocket:
    def __init__(self, host='localhost', port=5555):
        self.sock = socket.socket()
        self.sock.connect((host, port))
    
    def send_command(self, cmd_type, data):
        packet = struct.pack('<B', cmd_type) + data
        self.sock.send(packet)
    
    def recv_response(self):
        return self.sock.recv(4096)
```

## Performance Targets

- **Frame Rate:** 60 FPS (16.67ms per frame) or unlimited for training
- **CPU Instructions:** ~280,000 per frame @ 16.78 MHz
- **Training Speed:** 100-1000x real-time when headless
- **State Save/Load:** < 1ms
- **Memory Footprint:** ~50 MB (ROM + RAM + buffers)

## Build System

### CMakeLists.txt
```cmake
project(pokemon_emerald_emulator C)

add_executable(emu_core
    main.c
    cpu_core.c
    memory.c
    gfx_renderer.c
    input.c
    save_state.c
    rom_loader.c
    python_bridge.c
)

target_link_libraries(emu_core SDL2)

# Optional: Build as shared library for Python ctypes
add_library(emu_lib SHARED ${SOURCES})
```

## Development Phases

### Phase 1: ROM Loading & Basic Execution ⬅️ START HERE
- [x] Design architecture
- [ ] Load pokeemerald.gba into memory
- [ ] Initialize ARM7TDMI CPU state
- [ ] Implement fetch-decode-execute loop (minimal instruction set)
- [ ] Handle ROM reads
- [ ] Get to first instruction

### Phase 2: Memory & I/O
- [ ] Implement EWRAM/IWRAM read/write
- [ ] Stub I/O registers (enough to boot)
- [ ] Handle interrupts (VBlank at minimum)
- [ ] Memory-mapped I/O routing

### Phase 3: Graphics (Minimal)
- [ ] Parse background tiles from VRAM
- [ ] Render BG layer 0 (simplest case)
- [ ] Display to SDL2 window
- [ ] See *something* on screen

### Phase 4: Input & AI Hook
- [ ] Keyboard input mapping
- [ ] Write to gAiInputState (0x0203cf64)
- [ ] Game responds to input
- [ ] Verify AI control works

### Phase 5: Full Graphics
- [ ] All 4 BG layers with priority
- [ ] Sprite rendering (OBJ)
- [ ] Palette effects
- [ ] Windows and blending (if needed)

### Phase 6: Python Bridge
- [ ] Shared memory or socket interface
- [ ] Gym environment wrapper
- [ ] State extraction helpers
- [ ] Save state API

### Phase 7: Training Optimization
- [ ] Headless mode (no rendering)
- [ ] Fast-forward (no frame limiting)
- [ ] Deterministic execution
- [ ] Batch state operations

## Similar Projects (Reference)

- **mGBA** - Modern GBA emulator, reference for accuracy
- **VisualBoyAdvance** - Classic GBA emulator
- **MGBA-http** - mGBA with HTTP API
- **Gymnasium-Retro** - OpenAI's retro game training platform
- **BizHawk** - Multi-system emulator with Lua API

## Why Custom vs Existing Emulator?

| Feature | mGBA/VBA-M | Custom Emulator |
|---------|------------|-----------------|
| **Accuracy** | Cycle-perfect | "Good enough" for Pokemon |
| **Speed** | 60 FPS | 1000+ FPS headless |
| **State Access** | Lua/memory viewer | Direct C structs |
| **AI Integration** | External bridge | Native, zero-copy |
| **Flexibility** | General purpose | Pokemon-optimized |
| **Setup** | Install + scripts | Single binary |
| **Debugging** | GDB, external tools | Custom instrumentation |

## File Structure
```
tools/custom_emulator/
├── ARCHITECTURE.md       # This file
├── CMakeLists.txt        # Build config
├── main.c                # Entry point
├── cpu_core.c/h          # ARM7TDMI emulation
├── memory.c/h            # Memory management
├── gfx_renderer.c/h      # Graphics rendering
├── input.c/h             # Input handling
├── save_state.c/h        # Save state system
├── rom_loader.c/h        # ROM loading
├── python_bridge.c/h     # Python integration
├── timer.c/h             # Timers & interrupts
├── audio_stub.c/h        # Minimal audio (stub)
└── tests/
    ├── test_cpu.c
    ├── test_memory.c
    └── test_rom_load.c
```

## Next Steps

1. **Set up project structure** - Create directories and CMake
2. **Implement ROM loader** - Read pokeemerald.gba, parse header
3. **Minimal CPU core** - Just enough to execute first instruction
4. **Memory system** - Map GBA address space
5. **Test with actual ROM** - See if we can execute the entry point

Let's start building! 🚀
