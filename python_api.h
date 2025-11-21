#ifndef PYTHON_API_H
#define PYTHON_API_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque emulator state for Python
typedef void* EmuHandle;

// Initialize emulator with ROM
EmuHandle emu_init(const char *rom_path);

// Execute one frame with given button input
void emu_step(EmuHandle handle, u8 buttons);

// Get current screen buffer (240x160 RGB888)
void emu_get_screen(EmuHandle handle, u8 *buffer);

// Reset emulator to initial state
void emu_reset(EmuHandle handle);

// Cleanup and free emulator
void emu_cleanup(EmuHandle handle);

// Memory access
u8 emu_read_memory(EmuHandle handle, u32 addr);
void emu_write_memory(EmuHandle handle, u32 addr, u8 value);

// Get emulator statistics
u32 emu_get_frame_count(EmuHandle handle);
u32 emu_get_cpu_cycles(EmuHandle handle);

// Save state management
void emu_save_state(EmuHandle handle, const char *filename);
void emu_load_state(EmuHandle handle, const char *filename);

#ifdef __cplusplus
}
#endif

#endif // PYTHON_API_H
