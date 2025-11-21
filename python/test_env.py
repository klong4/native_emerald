"""
Test script for Pokemon Emerald Gym environment
"""

import sys
from pathlib import Path

# Add parent directory to path to import emerald_env
sys.path.insert(0, str(Path(__file__).parent))

from emerald_env import EmeraldEnv
import numpy as np


def test_basic_functionality():
    """Test basic environment functionality"""
    print("=== Testing Pokemon Emerald Gym Environment ===\n")
    
    # Find ROM
    rom_paths = [
        '../pokeemerald.gba',
        '../../pokeemerald.gba',
        '../../source/pokeemerald-master/pokeemerald.gba',
    ]
    
    rom_path = None
    for path in rom_paths:
        if Path(path).exists():
            rom_path = path
            break
    
    if not rom_path:
        print("ERROR: Could not find pokeemerald.gba ROM file")
        print("Searched in:")
        for path in rom_paths:
            print(f"  - {Path(path).resolve()}")
        return False
    
    print(f"Using ROM: {Path(rom_path).resolve()}\n")
    
    try:
        # Create environment
        print("Creating environment...")
        env = EmeraldEnv(rom_path, render_mode='rgb_array')
        print(f"✓ Environment created")
        print(f"  Action space: {env.action_space}")
        print(f"  Observation space: {env.observation_space}\n")
        
        # Reset
        print("Resetting environment...")
        observation, info = env.reset()
        print(f"✓ Environment reset")
        print(f"  Observation shape: {observation.shape}")
        print(f"  Observation dtype: {observation.dtype}")
        print(f"  Info: {info}\n")
        
        # Verify observation
        assert observation.shape == (160, 240, 3), "Wrong observation shape"
        assert observation.dtype == np.uint8, "Wrong observation dtype"
        print("✓ Observation validation passed\n")
        
        # Run a few steps
        print("Running 10 steps...")
        for i in range(10):
            action = env.action_space.sample()
            observation, reward, terminated, truncated, info = env.step(action)
            
            print(f"  Step {i+1}: action={action}, reward={reward:.2f}, "
                  f"terminated={terminated}, frame={info['frame']}")
            
            assert observation.shape == (160, 240, 3), "Wrong observation shape after step"
        
        print("\n✓ Steps completed successfully\n")
        
        # Test memory access
        print("Testing memory access...")
        test_addr = 0x02000000  # EWRAM start
        
        # Write
        env.write_memory(test_addr, 0x42)
        
        # Read back
        value = env.read_memory(test_addr)
        assert value == 0x42, f"Memory read/write failed: expected 0x42, got {value:02X}"
        
        print(f"✓ Memory read/write working (wrote 0x42, read {value:02X})\n")
        
        # Cleanup
        print("Cleaning up...")
        env.close()
        print("✓ Environment closed\n")
        
        print("=== All tests passed! ===")
        return True
        
    except Exception as e:
        print(f"\n✗ Test failed with error: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_action_mapping():
    """Test action to button mapping"""
    print("\n=== Testing Action Mapping ===\n")
    
    rom_path = '../../source/pokeemerald-master/pokeemerald.gba'
    if not Path(rom_path).exists():
        print("Skipping action mapping test (ROM not found)")
        return True
    
    env = EmeraldEnv(rom_path, render_mode='rgb_array')
    
    actions = {
        0: "Nothing",
        1: "A",
        2: "B",
        3: "Up",
        4: "Down",
        5: "Left",
        6: "Right",
        7: "Start",
        8: "Select",
    }
    
    print("Action -> Button mapping:")
    for action, name in actions.items():
        buttons = env._action_to_buttons(action)
        print(f"  {action} ({name:7s}) -> 0x{buttons:02X}")
    
    env.close()
    print("\n✓ Action mapping test passed\n")
    return True


if __name__ == '__main__':
    success = True
    
    # Run tests
    success &= test_basic_functionality()
    success &= test_action_mapping()
    
    # Exit code
    sys.exit(0 if success else 1)
