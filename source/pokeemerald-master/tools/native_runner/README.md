PokeEmerald Native x86 Runner

Overview
- Minimal scaffolding to build a native x86 runner for Windows using SDL2.
- Intended as a starting point for porting GBA-specific code to a desktop executable so AI policies can run natively.

Requirements
- CMake (3.10+)
- SDL2 development libraries (install via vcpkg, msys2, or SDL2 installer)

Build
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Run
- The generated `native_runner` executable is an initial test harness and does not yet include the game core.

Next steps
- Port GBA hardware API calls used by the game to the shim in `tools/native_runner/gba_shim.*`.
- Integrate game core files (from `src/`) into the CMake project, adapt build flags and missing platform APIs.
- Add AI policy embedding and a frame callback to call the policy each tick.

Optional: build game core
- You can attempt to compile selected `src/` files into the native runner by enabling the CMake option `BUILD_GAME_CORE`.
	This is experimental and will require adding many platform shims and fixing platform-specific code.

Example (from build dir):
```powershell
cmake .. -DBUILD_GAME_CORE=ON -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```
