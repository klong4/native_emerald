# API Reference - Native Emerald

Complete API documentation for the Pokemon Emerald emulator and RL environment.

## Table of Contents
- [Python API](#python-api)
- [C API](#c-api)
- [Memory Map](#memory-map)
- [Game State Addresses](#game-state-addresses)

## Python API

### EmeraldEnv Class

```python
from emerald_env import EmeraldEnv

env = EmeraldEnv(rom_path, render_mode='rgb_array')
```

#### Constructor

```python
EmeraldEnv(rom_path: str, render_mode: Optional[str] = None)
```

**Parameters:**
- `rom_path`: Path to `pokeemerald.gba` ROM file
- `render_mode`: `'human'` for window, `'rgb_array'` for numpy array, `None` for headless

**Example:**
```python
env = EmeraldEnv('../pokeemerald.gba', render_mode='human')
```

#### Methods

##### reset()
```python
reset(seed: Optional[int] = None, options: Optional[Dict] = None) -> Tuple[np.ndarray, Dict]
```

Reset environment to initial state.

**Returns:**
- `observation`: (160, 240, 3) numpy array - RGB screen
- `info`: Dict with `{'frame': int, 'episode_reward': float}`

**Example:**
```python
obs, info = env.reset()
print(f"Frame: {info['frame']}, Reward: {info['episode_reward']}")
```

##### step()
```python
step(action: int) -> Tuple[np.ndarray, float, bool, bool, Dict]
```

Execute one frame with given action.

**Parameters:**
- `action`: Integer 0-8 representing button press

**Returns:**
- `observation`: (160, 240, 3) numpy array
- `reward`: Float reward value
- `terminated`: Bool - episode finished naturally
- `truncated`: Bool - episode ended early
- `info`: Dict with metadata

**Example:**
```python
action = env.action_space.sample()
obs, reward, terminated, truncated, info = env.step(action)
```

##### read_memory()
```python
read_memory(addr: int) -> int
```

Read byte from emulator memory.

**Parameters:**
- `addr`: Memory address (0x00000000 - 0xFFFFFFFF)

**Returns:**
- Byte value (0-255)

**Example:**
```python
# Read badge count
badges = env.read_memory(0x0202420C)
badge_count = bin(badges).count('1')
```

##### write_memory()
```python
write_memory(addr: int, value: int)
```

Write byte to emulator memory.

**Parameters:**
- `addr`: Memory address
- `value`: Byte value (0-255)

**Example:**
```python
# Set player money to 999999
env.write_memory(0x02024490, 0x3F)
env.write_memory(0x02024491, 0x42)
env.write_memory(0x02024492, 0x0F)
```

##### get_game_state()
```python
get_game_state() -> Dict[str, Any]
```

Extract current game state from memory.

**Returns:**
- Dict with game information (player position, party, badges, etc.)

**Example:**
```python
state = env.get_game_state()
print(f"Map: {state['map_id']}, Badges: {state['party_count']}")
```

#### Properties

##### action_space
```python
env.action_space  # spaces.Discrete(9)
```

Discrete action space:
- 0: No button
- 1: A
- 2: B
- 3: Up
- 4: Down
- 5: Left
- 6: Right
- 7: Start
- 8: Select

##### observation_space
```python
env.observation_space  # spaces.Box(low=0, high=255, shape=(160, 240, 3), dtype=np.uint8)
```

RGB screen observation (160 height x 240 width x 3 channels).

## C API

### Core Functions

#### emu_init()
```c
EmuHandle emu_init(const char *rom_path);
```

Initialize emulator with ROM.

**Parameters:**
- `rom_path`: Path to ROM file

**Returns:**
- Opaque emulator handle (NULL on failure)

**Example:**
```c
EmuHandle emu = emu_init("pokeemerald.gba");
if (!emu) {
    fprintf(stderr, "Failed to load ROM\n");
    return 1;
}
```

#### emu_step()
```c
void emu_step(EmuHandle handle, u8 buttons);
```

Execute one frame.

**Parameters:**
- `handle`: Emulator handle
- `buttons`: Button bitmask

**Button Bits:**
- 0x01: A
- 0x02: B
- 0x04: Select
- 0x08: Start
- 0x10: Right
- 0x20: Left
- 0x40: Up
- 0x80: Down

**Example:**
```c
// Press A button
emu_step(emu, 0x01);

// Press A + Start
emu_step(emu, 0x01 | 0x08);

// No buttons
emu_step(emu, 0x00);
```

#### emu_get_screen()
```c
void emu_get_screen(EmuHandle handle, u8 *buffer);
```

Get current screen as RGB888.

**Parameters:**
- `handle`: Emulator handle
- `buffer`: Buffer for 240x160x3 bytes (115,200 bytes)

**Example:**
```c
u8 screen[240 * 160 * 3];
emu_get_screen(emu, screen);

// Access pixel at (x=100, y=50)
int idx = (50 * 240 + 100) * 3;
u8 r = screen[idx + 0];
u8 g = screen[idx + 1];
u8 b = screen[idx + 2];
```

#### emu_reset()
```c
void emu_reset(EmuHandle handle);
```

Reset emulator to initial state.

**Example:**
```c
emu_reset(emu);
```

#### emu_cleanup()
```c
void emu_cleanup(EmuHandle handle);
```

Free emulator resources.

**Example:**
```c
emu_cleanup(emu);
```

### Memory Access

#### emu_read_memory()
```c
u8 emu_read_memory(EmuHandle handle, u32 addr);
```

Read byte from memory.

**Example:**
```c
// Read first instruction
u8 byte = emu_read_memory(emu, 0x08000000);
```

#### emu_write_memory()
```c
void emu_write_memory(EmuHandle handle, u32 addr, u8 value);
```

Write byte to memory.

**Example:**
```c
// Write to EWRAM
emu_write_memory(emu, 0x02000000, 0x42);
```

### Statistics

#### emu_get_frame_count()
```c
u32 emu_get_frame_count(EmuHandle handle);
```

Get total frames executed.

#### emu_get_cpu_cycles()
```c
u32 emu_get_cpu_cycles(EmuHandle handle);
```

Get total CPU cycles executed.

## Memory Map

### GBA Memory Layout

```
0x00000000 - 0x00003FFF: BIOS (16KB, read-only)
0x02000000 - 0x0203FFFF: EWRAM (256KB)
0x03000000 - 0x03007FFF: IWRAM (32KB)
0x04000000 - 0x040003FF: I/O Registers (1KB)
0x05000000 - 0x050003FF: Palette RAM (1KB)
0x06000000 - 0x06017FFF: VRAM (96KB)
0x07000000 - 0x070003FF: OAM (1KB)
0x08000000 - 0x09FFFFFF: ROM (max 32MB, mirrored)
```

### I/O Registers

```
0x04000000: DISPCNT - Display Control
0x04000004: DISPSTAT - Display Status
0x04000006: VCOUNT - Vertical Counter
0x04000008-E: BG0-3CNT - Background Control
0x04000200: IE - Interrupt Enable
0x04000202: IF - Interrupt Request
0x04000208: IME - Interrupt Master Enable
```

## Game State Addresses

### Player Information

```c
#define ADDR_PLAYER_NAME      0x02024029  // 7 bytes
#define ADDR_PLAYER_GENDER    0x02024008  // 0=male, 1=female
#define ADDR_PLAYER_ID        0x0202402C  // u32
```

### Location

```c
#define ADDR_MAP_GROUP        0x02036DFC  // u8
#define ADDR_MAP_NUMBER       0x02036DFD  // u8
#define ADDR_PLAYER_X         0x02037340  // u16
#define ADDR_PLAYER_Y         0x02037344  // u16
```

### Progress

```c
#define ADDR_BADGES           0x0202420C  // u8 bitmask
#define ADDR_MONEY            0x02024490  // u32
#define ADDR_COINS            0x02024494  // u16
```

### Party Pokemon

```c
#define ADDR_PARTY_COUNT      0x02024284  // u8 (1-6)
#define ADDR_PARTY_DATA       0x02024284  // 600 bytes total

// Each Pokemon is 100 bytes
#define POKEMON_SIZE          100
#define POKEMON_SPECIES       0x00  // u16
#define POKEMON_HP            0x56  // u16
#define POKEMON_MAX_HP        0x58  // u16
#define POKEMON_LEVEL         0x54  // u8
```

### Example: Reading Party

```python
def get_party_info(env):
    party_count = env.read_memory(0x02024284)
    party = []
    
    for i in range(party_count):
        base = 0x02024284 + 4 + (i * 100)
        
        species = (env.read_memory(base) | 
                   (env.read_memory(base + 1) << 8))
        hp = (env.read_memory(base + 0x56) |
              (env.read_memory(base + 0x57) << 8))
        level = env.read_memory(base + 0x54)
        
        party.append({
            'species': species,
            'hp': hp,
            'level': level
        })
    
    return party
```

## Advanced Usage

### Custom CNN Policy

```python
from stable_baselines3 import PPO
from stable_baselines3.common.torch_layers import BaseFeaturesExtractor
import torch.nn as nn

class CustomCNN(BaseFeaturesExtractor):
    def __init__(self, observation_space, features_dim=256):
        super().__init__(observation_space, features_dim)
        self.cnn = nn.Sequential(
            nn.Conv2d(3, 32, 8, stride=4),
            nn.ReLU(),
            nn.Conv2d(32, 64, 4, stride=2),
            nn.ReLU(),
            nn.Flatten(),
            nn.Linear(64 * 19 * 28, features_dim),
            nn.ReLU()
        )
    
    def forward(self, observations):
        return self.cnn(observations)

policy_kwargs = dict(
    features_extractor_class=CustomCNN,
    features_extractor_kwargs=dict(features_dim=512)
)

model = PPO('CnnPolicy', env, policy_kwargs=policy_kwargs)
```

### Curriculum Learning

```python
class CurriculumEmeraldEnv(EmeraldEnv):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.curriculum_stage = 0
    
    def _calculate_reward(self):
        reward = super()._calculate_reward()
        
        # Stage 0: Just explore
        if self.curriculum_stage == 0:
            if self._new_maps_visited > 5:
                self.curriculum_stage = 1
                print("Advancing to stage 1: Battle training")
        
        # Stage 1: Win battles
        elif self.curriculum_stage == 1:
            if self._battles_won > 10:
                self.curriculum_stage = 2
                print("Advancing to stage 2: Gym challenge")
        
        return reward
```

## Error Codes

| Code | Meaning | Solution |
|------|---------|----------|
| NULL handle | Failed to load ROM | Check ROM path and file size (16MB) |
| Segfault | Invalid memory access | Check address ranges |
| OOM | Out of memory | Reduce batch size or frame stack |

## Performance Benchmarks

| Operation | Time | Notes |
|-----------|------|-------|
| `emu_init()` | ~50ms | One-time initialization |
| `emu_step()` | ~0.3ms | ~3,300 FPS on modern CPU |
| `emu_get_screen()` | ~0.1ms | RGB888 conversion |
| `emu_reset()` | ~5ms | Clears memory |

## See Also

- [README.md](README.md) - Project overview
- [TRAINING_GUIDE.md](TRAINING_GUIDE.md) - Training tips
- [ARCHITECTURE.md](ARCHITECTURE.md) - System design
