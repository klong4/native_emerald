#ifndef SAVE_STATE_H
#define SAVE_STATE_H

#include "types.h"

struct EmulatorState;

// Save state to file
bool save_state_to_file(struct EmulatorState *emu, const char *filename);

// Load state from file
bool load_state_from_file(struct EmulatorState *emu, const char *filename);

// Save state to memory buffer
bool save_state_to_buffer(struct EmulatorState *emu, u8 *buffer, u32 *size);

// Load state from memory buffer
bool load_state_from_buffer(struct EmulatorState *emu, const u8 *buffer, u32 size);

// Get required buffer size for save state
u32 get_save_state_size(void);

#endif // SAVE_STATE_H
