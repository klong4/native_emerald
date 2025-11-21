"""
Pokemon Emerald OpenAI Gym Environment
Wraps the native_emerald emulator for reinforcement learning
"""

import ctypes
import numpy as np
from pathlib import Path
from typing import Tuple, Dict, Any, Optional
import gymnasium as gym
from gymnasium import spaces


class EmeraldEnv(gym.Env):
    """OpenAI Gym environment for Pokemon Emerald"""
    
    metadata = {'render_modes': ['human', 'rgb_array'], 'render_fps': 60}
    
    # GBA button mapping
    KEY_A = 0x01
    KEY_B = 0x02
    KEY_SELECT = 0x04
    KEY_START = 0x08
    KEY_RIGHT = 0x10
    KEY_LEFT = 0x20
    KEY_UP = 0x40
    KEY_DOWN = 0x80
    
    def __init__(self, rom_path: str, render_mode: Optional[str] = None):
        """
        Initialize Pokemon Emerald environment
        
        Args:
            rom_path: Path to pokeemerald.gba ROM file
            render_mode: 'human' for window, 'rgb_array' for numpy array
        """
        super().__init__()
        
        self.render_mode = render_mode
        self.rom_path = str(Path(rom_path).resolve())
        
        # Action space: 8 buttons (each can be pressed or not)
        # We'll use discrete actions for simplicity
        self.action_space = spaces.Discrete(9)  # 0=nothing, 1-8=individual buttons
        
        # Observation space: GBA screen (240x160 RGB)
        self.observation_space = spaces.Box(
            low=0, high=255,
            shape=(160, 240, 3),
            dtype=np.uint8
        )
        
        # Load emulator library
        lib_path = Path(__file__).parent.parent / 'build' / 'libpokemon_emu_lib.so'
        if not lib_path.exists():
            raise FileNotFoundError(f"Emulator library not found: {lib_path}")
        
        self.lib = ctypes.CDLL(str(lib_path))
        
        # Define C function signatures
        self._setup_ctypes()
        
        # Emulator state (opaque pointer)
        self.emu_state = None
        
        # Frame buffer
        self.frame_buffer = np.zeros((160, 240, 3), dtype=np.uint8)
        
        # Statistics
        self.frame_count = 0
        self.episode_reward = 0.0
    
    def _setup_ctypes(self):
        """Setup ctypes function signatures for emulator library"""
        
        # TODO: Define actual C API functions
        # For now, placeholder signatures
        
        # emu_init(rom_path) -> void*
        self.lib.emu_init.argtypes = [ctypes.c_char_p]
        self.lib.emu_init.restype = ctypes.c_void_p
        
        # emu_step(state, buttons) -> void
        self.lib.emu_step.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
        self.lib.emu_step.restype = None
        
        # emu_get_screen(state, buffer) -> void
        self.lib.emu_get_screen.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8)]
        self.lib.emu_get_screen.restype = None
        
        # emu_reset(state) -> void
        self.lib.emu_reset.argtypes = [ctypes.c_void_p]
        self.lib.emu_reset.restype = None
        
        # emu_cleanup(state) -> void
        self.lib.emu_cleanup.argtypes = [ctypes.c_void_p]
        self.lib.emu_cleanup.restype = None
        
        # emu_read_memory(state, addr) -> u8
        self.lib.emu_read_memory.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        self.lib.emu_read_memory.restype = ctypes.c_uint8
        
        # emu_write_memory(state, addr, value) -> void
        self.lib.emu_write_memory.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint8]
        self.lib.emu_write_memory.restype = None
    
    def reset(self, seed: Optional[int] = None, options: Optional[Dict] = None) -> Tuple[np.ndarray, Dict]:
        """
        Reset the environment to initial state
        
        Returns:
            observation: Initial screen
            info: Additional information
        """
        super().reset(seed=seed)
        
        if self.emu_state is None:
            # First initialization
            self.emu_state = self.lib.emu_init(self.rom_path.encode('utf-8'))
        else:
            # Reset existing emulator
            self.lib.emu_reset(self.emu_state)
        
        # Get initial observation
        observation = self._get_observation()
        
        self.frame_count = 0
        self.episode_reward = 0.0
        
        info = {
            'frame': self.frame_count,
            'episode_reward': self.episode_reward
        }
        
        return observation, info
    
    def step(self, action: int) -> Tuple[np.ndarray, float, bool, bool, Dict]:
        """
        Execute one step in the environment
        
        Args:
            action: Action to take (0-8)
        
        Returns:
            observation: Screen after action
            reward: Reward for this step
            terminated: Whether episode is done
            truncated: Whether episode was truncated
            info: Additional information
        """
        # Map discrete action to button press
        buttons = self._action_to_buttons(action)
        
        # Execute one frame
        self.lib.emu_step(self.emu_state, buttons)
        
        # Get new observation
        observation = self._get_observation()
        
        # Calculate reward (placeholder)
        reward = self._calculate_reward()
        
        # Check if episode is done (placeholder)
        terminated = False
        truncated = False
        
        self.frame_count += 1
        self.episode_reward += reward
        
        info = {
            'frame': self.frame_count,
            'episode_reward': self.episode_reward,
            'buttons': buttons
        }
        
        return observation, reward, terminated, truncated, info
    
    def render(self):
        """Render the environment"""
        if self.render_mode == 'rgb_array':
            return self._get_observation()
        elif self.render_mode == 'human':
            # SDL window is already rendering
            pass
    
    def close(self):
        """Cleanup resources"""
        if self.emu_state is not None:
            self.lib.emu_cleanup(self.emu_state)
            self.emu_state = None
    
    def _action_to_buttons(self, action: int) -> int:
        """Convert discrete action to button bitmask"""
        if action == 0:
            return 0  # No buttons
        elif action == 1:
            return self.KEY_A
        elif action == 2:
            return self.KEY_B
        elif action == 3:
            return self.KEY_UP
        elif action == 4:
            return self.KEY_DOWN
        elif action == 5:
            return self.KEY_LEFT
        elif action == 6:
            return self.KEY_RIGHT
        elif action == 7:
            return self.KEY_START
        elif action == 8:
            return self.KEY_SELECT
        return 0
    
    def _get_observation(self) -> np.ndarray:
        """Get current screen as numpy array"""
        # Copy screen data from emulator
        buffer_ptr = self.frame_buffer.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8))
        self.lib.emu_get_screen(self.emu_state, buffer_ptr)
        return self.frame_buffer.copy()
    
    def _calculate_reward(self) -> float:
        """Calculate reward for current state"""
        # Track progress metrics
        if not hasattr(self, '_prev_state'):
            self._prev_state = {
                'badges': 0,
                'money': 0,
                'party_hp': 0,
                'map_id': 0,
                'frame': 0
            }
        
        reward = 0.0
        
        # Get current game state
        badges = self.read_memory(0x0202420C)
        money = (self.read_memory(0x02024490) |
                 (self.read_memory(0x02024491) << 8) |
                 (self.read_memory(0x02024492) << 16) |
                 (self.read_memory(0x02024493) << 24))
        
        # Reward for gaining badges (huge reward)
        badge_count = bin(badges).count('1')
        prev_badge_count = bin(self._prev_state['badges']).count('1')
        if badge_count > prev_badge_count:
            reward += 1000.0 * (badge_count - prev_badge_count)
            print(f"🏆 New badge! Total: {badge_count}")
        
        # Reward for gaining money (small reward)
        money_gained = money - self._prev_state['money']
        if money_gained > 0:
            reward += money_gained / 1000.0  # Scale down
        
        # Penalty for losing HP
        party_count = self.read_memory(0x02024284)
        if party_count > 0 and party_count <= 6:
            total_hp = 0
            for i in range(party_count):
                hp_addr = 0x02024284 + 4 + (i * 100) + 0x56
                hp = self.read_memory(hp_addr) | (self.read_memory(hp_addr + 1) << 8)
                total_hp += hp
            
            hp_lost = self._prev_state['party_hp'] - total_hp
            if hp_lost > 0:
                reward -= hp_lost * 0.1
        
        # Small reward for exploring (new map)
        map_id = self.read_memory(0x02036DFD)
        if map_id != self._prev_state['map_id']:
            reward += 5.0
        
        # Small time penalty to encourage progress
        reward -= 0.01
        
        # Update previous state
        self._prev_state = {
            'badges': badges,
            'money': money,
            'party_hp': total_hp if party_count > 0 else 0,
            'map_id': map_id,
            'frame': self.frame_count
        }
        
        return reward
    
    def read_memory(self, addr: int) -> int:
        """Read byte from emulator memory"""
        return self.lib.emu_read_memory(self.emu_state, addr)
    
    def write_memory(self, addr: int, value: int):
        """Write byte to emulator memory"""
        self.lib.emu_write_memory(self.emu_state, addr, value & 0xFF)
    
    def get_game_state(self) -> Dict[str, Any]:
        """Extract game state from memory (placeholder)"""
        # TODO: Parse Pokemon Emerald memory to extract:
        # - Player position
        # - Party Pokemon (species, HP, level, etc.)
        # - Inventory
        # - Badges
        # - Current map
        # - Battle state
        
        return {
            'player_x': 0,
            'player_y': 0,
            'map_id': 0,
            'party_count': 0,
        }


if __name__ == '__main__':
    # Test the environment
    import time
    
    env = EmeraldEnv('../pokeemerald.gba', render_mode='human')
    
    observation, info = env.reset()
    print(f"Observation shape: {observation.shape}")
    print(f"Action space: {env.action_space}")
    
    # Run for 1000 frames
    for i in range(1000):
        action = env.action_space.sample()  # Random action
        observation, reward, terminated, truncated, info = env.step(action)
        
        if i % 60 == 0:
            print(f"Frame {info['frame']}, Reward: {reward:.2f}")
        
        if terminated or truncated:
            observation, info = env.reset()
        
        time.sleep(1/60)  # 60 FPS
    
    env.close()
