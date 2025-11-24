PokeEmerald RL Integration

Overview
- This folder contains a scaffold to train reinforcement-learning agents to play the built ROM via an emulator bridge.
- It provides a Gym-style environment (`gym_poke_env.py`), a training example (`train.py`), and a bridge template (`mgba_bridge.lua`).

High-level approach
1. Build the ROM as usual (see root `Makefile`).
2. Run an emulator (mGBA or BizHawk). Load the ROM and run a Lua script or use the emulator API to expose memory/screen and accept button inputs.
3. The emulator bridge should implement a simple JSON-over-TCP protocol (examples below) used by `PokeEnv`.
4. Train an RL agent (PPO) using the provided `train.py`.

Protocol (JSON over TCP)
- The Python env connects to the emulator bridge on startup.
- Messages (examples):
  - {"cmd": "reset"}
  - {"cmd": "step", "buttons": ["A","LEFT"]}
  - {"cmd": "get_state"}
- Responses should be JSON with fields: `obs` (base64 or raw bytes), `ram` (optional table), `reward`, `done`, `info`.

Notes & Next steps
- The included `mgba_bridge.lua` is a template — you will likely need to adapt it for your emulator and available Lua/socket libs.
- Alternatively, BizHawk has a well-documented Lua API and may be easier for memory/screen access.
- If you want, I can:
  - generate a BizHawk Lua script tailored to reading battle state and injecting inputs, or
  - add in-ROM support (a small new `bot_input` RAM area and handler in `src/` to accept inputs from a shared memory region).

Legal/ethics
- Ensure you own any ROM you run and comply with emulator and ROM distribution laws.
