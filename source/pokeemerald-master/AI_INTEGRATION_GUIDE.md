# AI Integration Guide for Pokemon Emerald

## Quick Start: Testing the ROM

### Option 1: WSL with mGBA
```bash
# Install mGBA
sudo apt install mgba-qt

# Run the ROM
cd /mnt/e/pokeplayer/source/pokeemerald-master
mgba-qt pokeemerald.gba
```

### Option 2: Windows with mGBA
1. Download from https://mgba.io/downloads.html
2. Extract to a folder
3. Run `mGBA.exe`
4. File → Load ROM → Select `E:\pokeplayer\source\pokeemerald-master\pokeemerald.gba`

---

## AI Input Hook Overview

### Memory Layout

The ROM exposes a single byte in EWRAM for AI control:

```c
// In EWRAM (External Working RAM)
EWRAM_DATA volatile u8 gAiInputState = 0;
```

**What this means:**
- External tools (Python scripts, Lua scripts, native runner) can write to this memory location
- The game reads this byte each frame to get AI-controlled button inputs
- Button mask format matches GBA hardware button layout

### Button Mask Format

```c
// Buttons (bit flags)
#define A_BUTTON       0x01  // 0000 0001
#define B_BUTTON       0x02  // 0000 0010
#define SELECT_BUTTON  0x04  // 0000 0100
#define START_BUTTON   0x08  // 0000 1000
#define DPAD_RIGHT     0x10  // 0001 0000
#define DPAD_LEFT      0x20  // 0010 0000
#define DPAD_UP        0x40  // 0100 0000
#define DPAD_DOWN      0x80  // 1000 0000
#define R_BUTTON       0x100 // (requires u16)
#define L_BUTTON       0x200 // (requires u16)
```

**Example Usage:**
```python
# Press A button
gAiInputState = 0x01

# Press A + B simultaneously
gAiInputState = 0x03  # 0x01 | 0x02

# Press Up + A (walk forward and interact)
gAiInputState = 0x41  # 0x40 | 0x01

# No input (release all buttons)
gAiInputState = 0x00
```

---

## Integration Approaches

### Approach 1: mGBA Lua Scripting (Recommended for RL)

**Advantages:**
- ✅ Easy to set up
- ✅ Full memory access
- ✅ No compilation needed
- ✅ Perfect for Gym environment

**Setup:**
```lua
-- example_ai.lua
local EWRAM_BASE = 0x02000000
local AI_INPUT_OFFSET = ???  -- Need to find this address

function on_frame()
    -- Read game state
    local player_x = emu:read8(EWRAM_BASE + 0x1234)  -- Example
    local player_y = emu:read8(EWRAM_BASE + 0x1235)
    
    -- AI decision making
    local action = get_ai_action(player_x, player_y)
    
    -- Write button input
    emu:write8(EWRAM_BASE + AI_INPUT_OFFSET, action)
end

callbacks:add("frame", on_frame)
```

**Finding `gAiInputState` Address:**
```bash
# In WSL, build with symbols
make clean
make DINFO=1

# Check the map file for address
grep gAiInputState pokeemerald.map
```

### Approach 2: BizHawk Emulator (Best for Debugging)

**Advantages:**
- ✅ Powerful Lua API
- ✅ TAS (Tool-Assisted Speedrun) features
- ✅ Save states for training
- ✅ Frame advance for precise control

**Python Integration:**
```python
import socket
import struct

class BizHawkBridge:
    def __init__(self, host='localhost', port=43884):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
    
    def read_byte(self, address):
        cmd = f"ReadByte|{address}\n"
        self.sock.send(cmd.encode())
        return int(self.sock.recv(1024))
    
    def write_byte(self, address, value):
        cmd = f"WriteByte|{address}|{value}\n"
        self.sock.send(cmd.encode())
    
    def set_ai_input(self, buttons):
        self.write_byte(AI_INPUT_ADDRESS, buttons)
```

### Approach 3: Native Runner (Advanced)

**Status:** ⚠️ Partially implemented in `tools/native_runner/`

**Advantages:**
- ✅ Direct Python integration
- ✅ No emulator overhead
- ✅ Fast training iterations

**Blockers:**
- ❌ GBA graphics rendering not complete
- ❌ Full game code not yet integrated
- ❌ MSVC compatibility issues

**See:** `tools/native_runner/STATUS.md` for details

---

## Creating a Gym Environment

### Minimal Example

```python
import gymnasium as gym
import numpy as np

class PokemonEmeraldEnv(gym.Env):
    def __init__(self, rom_path):
        super().__init__()
        
        # Action space: 8 buttons
        self.action_space = gym.spaces.Discrete(8)
        
        # Observation space: example (adjust based on what you extract)
        self.observation_space = gym.spaces.Box(
            low=0, high=255,
            shape=(240, 160, 3),  # GBA screen size
            dtype=np.uint8
        )
        
        # Initialize emulator bridge
        self.emu = self._init_emulator(rom_path)
        self.ai_input_address = self._find_ai_input_address()
    
    def reset(self, seed=None, options=None):
        super().reset(seed=seed)
        self.emu.load_state("start_state.state")
        obs = self._get_observation()
        return obs, {}
    
    def step(self, action):
        # Map action to button mask
        button_map = {
            0: 0x01,  # A
            1: 0x02,  # B
            2: 0x40,  # UP
            3: 0x80,  # DOWN
            4: 0x20,  # LEFT
            5: 0x10,  # RIGHT
            6: 0x08,  # START
            7: 0x04,  # SELECT
        }
        
        # Write AI input
        self.emu.write_byte(self.ai_input_address, button_map[action])
        
        # Advance frame
        self.emu.frame_advance()
        
        # Get new state
        obs = self._get_observation()
        reward = self._calculate_reward()
        terminated = self._check_terminated()
        truncated = False
        info = self._get_info()
        
        return obs, reward, terminated, truncated, info
    
    def _get_observation(self):
        # Read screen or game state from memory
        return self.emu.get_screen()
    
    def _calculate_reward(self):
        # Example reward function
        reward = 0
        
        # Reward for catching Pokemon
        party_size = self.emu.read_byte(PARTY_SIZE_ADDRESS)
        reward += (party_size - self.last_party_size) * 100
        
        # Reward for defeating trainers
        # Reward for exploration
        # Penalty for death
        
        return reward
```

---

## Memory Addresses to Find

Use the `.map` file or a memory viewer to locate:

### Critical Addresses
```
gAiInputState          - AI input byte (EWRAM, our addition)
gSaveBlock1Ptr         - Player data, party, items
gSaveBlock2Ptr         - Options, gameplay info
gPlayerParty           - Current party Pokemon
gEnemyParty            - Opponent's party
gBattleControllerFuncs - Battle state
gMapHeader             - Current map info
```

### How to Find Addresses

**Method 1: Symbol Map**
```bash
grep "gPlayerParty" pokeemerald.map
# Output: 0x02024029 gPlayerParty
```

**Method 2: Cheat Engine / Memory Viewer**
1. Load ROM in mGBA
2. Tools → Memory Viewer
3. Search for known values (HP, level, etc.)
4. Narrow down until you find the structure

**Method 3: Source Code**
```bash
# Find where it's defined
grep -r "gPlayerParty" src/
```

---

## State Extraction Examples

### Reading Player Position
```python
def get_player_position(emu):
    # Addresses from map file (example, need to verify)
    x = emu.read16(0x02037000)  # gPlayerX
    y = emu.read16(0x02037002)  # gPlayerY
    map_id = emu.read8(0x02037004)  # gCurrentMap
    return (x, y, map_id)
```

### Reading Party Pokemon
```python
def get_party_info(emu):
    party = []
    party_count = emu.read8(0x02024029)  # Example address
    
    for i in range(party_count):
        base = 0x02024030 + (i * 100)  # Pokemon struct size
        species = emu.read16(base + 0)
        hp = emu.read16(base + 86)
        max_hp = emu.read16(base + 88)
        level = emu.read8(base + 54)
        
        party.append({
            'species': species,
            'hp': hp,
            'max_hp': max_hp,
            'level': level
        })
    
    return party
```

### Reading Battle State
```python
def get_battle_state(emu):
    in_battle = emu.read8(0x02022D00)  # Example
    if not in_battle:
        return None
    
    return {
        'turn': emu.read8(0x02022D04),
        'player_hp': emu.read16(0x02022D10),
        'enemy_hp': emu.read16(0x02022D20),
        # ... more battle info
    }
```

---

## Reward Function Design

### Example Reward Structure

```python
class RewardCalculator:
    def __init__(self):
        self.last_position = (0, 0, 0)
        self.visited_tiles = set()
        self.last_party_size = 1
        self.last_level_sum = 5
    
    def calculate(self, emu):
        reward = 0
        
        # 1. Exploration reward
        pos = get_player_position(emu)
        if pos not in self.visited_tiles:
            reward += 1
            self.visited_tiles.add(pos)
        
        # 2. Pokemon progression
        party = get_party_info(emu)
        new_party_size = len(party)
        if new_party_size > self.last_party_size:
            reward += 50  # Caught new Pokemon
        
        # 3. Level up reward
        level_sum = sum(p['level'] for p in party)
        if level_sum > self.last_level_sum:
            reward += (level_sum - self.last_level_sum) * 5
        
        # 4. Battle rewards
        battle = get_battle_state(emu)
        if battle and battle['enemy_hp'] == 0:
            reward += 25  # Won battle
        
        # 5. Penalties
        if party[0]['hp'] == 0:
            reward -= 50  # Fainted
        
        # Update state
        self.last_position = pos
        self.last_party_size = new_party_size
        self.last_level_sum = level_sum
        
        return reward
```

---

## Training Setup

### Using Stable-Baselines3

```python
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import SubprocVecEnv

# Create vectorized environment
def make_env():
    def _init():
        return PokemonEmeraldEnv("pokeemerald.gba")
    return _init

env = SubprocVecEnv([make_env() for i in range(4)])

# Create PPO agent
model = PPO(
    "CnnPolicy",
    env,
    verbose=1,
    tensorboard_log="./pokemon_tb/",
    learning_rate=0.0003,
    n_steps=2048,
    batch_size=64,
)

# Train
model.learn(total_timesteps=1_000_000)

# Save
model.save("pokemon_emerald_ppo")
```

---

## Debugging Tips

### Enable Verbose Logging
```python
import logging
logging.basicConfig(level=logging.DEBUG)
```

### Save States for Reproducibility
```python
# Save critical checkpoints
emu.save_state("after_starter.state")
emu.save_state("before_first_gym.state")

# Load for training
env.emu.load_state("after_starter.state")
```

### Monitor Key Metrics
```python
from stable_baselines3.common.callbacks import BaseCallback

class MetricsCallback(BaseCallback):
    def _on_step(self):
        if self.n_calls % 1000 == 0:
            # Log custom metrics
            info = self.locals['infos'][0]
            self.logger.record('pokemon/party_size', info['party_size'])
            self.logger.record('pokemon/level_sum', info['level_sum'])
        return True
```

---

## Resources

### Documentation
- **pokeemerald:** https://github.com/pret/pokeemerald
- **mGBA Scripting:** https://mgba.io/docs/scripting.html
- **Stable-Baselines3:** https://stable-baselines3.readthedocs.io/

### Community
- **pret Discord:** https://discord.gg/d5dubZ3
- **/r/PokemonROMhacks:** https://reddit.com/r/PokemonROMhacks
- **GBATek:** https://problemkaputt.de/gbatek.htm (Hardware specs)

### Tools
- **mGBA:** https://mgba.io/
- **BizHawk:** https://tasvideos.org/BizHawk
- **VBA-M:** https://github.com/visualboyadvance-m/visualboyadvance-m
- **Cheat Engine:** https://www.cheatengine.org/

---

## Next Steps

1. ✅ **Test the ROM** - Verify it works in an emulator
2. 🔍 **Find `gAiInputState` address** - Check the map file
3. 🎮 **Create simple Lua script** - Test button control
4. 🐍 **Build Gym environment** - Wrap emulator in Python
5. 🧠 **Train baseline agent** - Simple PPO on first route
6. 📊 **Analyze performance** - Tensorboard metrics
7. 🚀 **Scale up** - More complex rewards, longer training

Good luck with your AI Pokemon trainer! 🎮🤖
