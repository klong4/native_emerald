# Pokemon Emerald AI Quick Reference

## Critical Memory Addresses

### AI Input Hook
```
Address: 0x0203cf64 (EWRAM)
Variable: gAiInputState
Type: u8 (1 byte)
Purpose: AI-controlled button input mask
```

## Button Masks (OR together for combinations)

```c
A_BUTTON      = 0x01  // 0000 0001
B_BUTTON      = 0x02  // 0000 0010
SELECT_BUTTON = 0x04  // 0000 0100
START_BUTTON  = 0x08  // 0000 1000
DPAD_RIGHT    = 0x10  // 0001 0000
DPAD_LEFT     = 0x20  // 0010 0000
DPAD_UP       = 0x40  // 0100 0000
DPAD_DOWN     = 0x80  // 1000 0000
```

## Example: mGBA Lua Script

```lua
-- ai_control.lua
local AI_INPUT = 0x0203cf64

function press_button(button)
    emu:write8(AI_INPUT, button)
end

function release_all()
    emu:write8(AI_INPUT, 0x00)
end

-- Example: Walk forward and press A
function on_frame()
    local frame_count = emu:frameCount()
    
    if frame_count % 60 < 30 then
        -- Walk up for 30 frames
        press_button(0x40)
    elseif frame_count % 60 == 30 then
        -- Press A on frame 30
        press_button(0x01)
    else
        -- Release for remaining frames
        release_all()
    end
end

callbacks:add("frame", on_frame)
```

## Example: Python with Memory Access

```python
# Assuming you have an emulator bridge
class AIController:
    AI_INPUT_ADDRESS = 0x0203cf64
    
    def __init__(self, emu_bridge):
        self.emu = emu_bridge
    
    def press_a(self):
        self.emu.write_byte(self.AI_INPUT_ADDRESS, 0x01)
    
    def press_b(self):
        self.emu.write_byte(self.AI_INPUT_ADDRESS, 0x02)
    
    def move_up(self):
        self.emu.write_byte(self.AI_INPUT_ADDRESS, 0x40)
    
    def move_down(self):
        self.emu.write_byte(self.AI_INPUT_ADDRESS, 0x80)
    
    def move_left(self):
        self.emu.write_byte(self.AI_INPUT_ADDRESS, 0x20)
    
    def move_right(self):
        self.emu.write_byte(self.AI_INPUT_ADDRESS, 0x10)
    
    def release(self):
        self.emu.write_byte(self.AI_INPUT_ADDRESS, 0x00)
    
    def combo(self, buttons):
        """Press multiple buttons at once"""
        mask = 0
        for button in buttons:
            mask |= button
        self.emu.write_byte(self.AI_INPUT_ADDRESS, mask)

# Usage example
controller = AIController(emu_bridge)

# Walk forward
controller.move_up()

# Run forward (B + Up)
controller.combo([0x02, 0x40])

# Press A to interact
controller.press_a()

# Release all
controller.release()
```

## Testing in mGBA

1. Load ROM: `File → Load ROM → pokeemerald.gba`
2. Open Lua console: `Tools → Scripting...`
3. Load script: `File → Load script... → ai_control.lua`
4. Watch the AI take control!

## Verification

To verify the hook is working:

```lua
-- test_ai_hook.lua
local AI_INPUT = 0x0203cf64

function test()
    -- Read current value
    local current = emu:read8(AI_INPUT)
    console:log("Current AI input: " .. current)
    
    -- Write test value
    emu:write8(AI_INPUT, 0xFF)
    
    -- Read back
    local new_val = emu:read8(AI_INPUT)
    console:log("New AI input: " .. new_val)
    
    if new_val == 0xFF then
        console:log("✓ AI hook is working!")
    else
        console:log("✗ AI hook failed")
    end
    
    -- Reset
    emu:write8(AI_INPUT, 0x00)
end

callbacks:add("start", test)
```

## Common Actions (Button Combinations)

```python
ACTIONS = {
    "IDLE":        0x00,
    "A":           0x01,
    "B":           0x02,
    "SELECT":      0x04,
    "START":       0x08,
    "RIGHT":       0x10,
    "LEFT":        0x20,
    "UP":          0x40,
    "DOWN":        0x80,
    "A+B":         0x03,  # Menu cancel
    "RUN_UP":      0x42,  # B + Up
    "RUN_DOWN":    0x82,  # B + Down
    "RUN_LEFT":    0x22,  # B + Left
    "RUN_RIGHT":   0x12,  # B + Right
}

# Use in RL environment
action_space = gym.spaces.Discrete(len(ACTIONS))
```

## File Locations

```
ROM:     E:\pokeplayer\source\pokeemerald-master\pokeemerald.gba
Size:    16,777,216 bytes (16 MB)
SHA1:    e4ed3bbcfa2fe5c4a003e23e3d489ece81463845

Source Files:
- include/ai_input.h  (Interface declarations)
- src/ai_input.c      (Implementation with EWRAM hook)

Build:
- make clean && make -j4  (Full rebuild)
- make -j4                (Incremental build)
```

## Documentation

- BUILD_SUCCESS.md - Detailed build information
- AI_INTEGRATION_GUIDE.md - Complete integration guide
- tools/native_runner/STATUS.md - Native runner status

---

**Ready to start?** Load the ROM in mGBA and test the Lua script above!
